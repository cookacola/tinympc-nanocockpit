#include "gap8_perception_output.h"
#include <string.h>

#define CORNER_W 40
#define CORNER_H 30
#define CORNER_C 4
#define DANGER_W 10
#define DANGER_H 8
#define DANGER_OFFSET (CORNER_W * CORNER_H * CORNER_C)
static const uint8_t corner_threshold[4] = {136, 141, 187, 131};
#define DANGER_Q_THRESHOLD 40

static float cross2(float ax, float ay, float bx, float by,
                    float px, float py) {
  return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

void gap8_decode_corner_argmax(const uint8_t *packed,
                               float corners_xy[8],
                               uint8_t confidence[4]) {
  for (int c = 0; c < 4; ++c) {
    uint8_t best = 0;
    int bx = 0, by = 0;
    for (int y = 0; y < CORNER_H; ++y) {
      for (int x = 0; x < CORNER_W; ++x) {
        uint8_t value = packed[(y * CORNER_W + x) * CORNER_C + c];
        if (value > best) { best = value; bx = x; by = y; }
      }
    }
    corners_xy[2 * c] = (bx + 0.5f) * 4.0f;
    corners_xy[2 * c + 1] = (by + 0.5f) * 4.0f + 20.0f;
    confidence[c] = best;
  }
}

int gap8_validate_or_recover_gate(float corners[8],
                                  const uint8_t confidence[4]) {
  int confident_count = 0;
  int recovered_corner = -1;
  float original_recovered[2] = {0.0f, 0.0f};
#define GAP8_REJECT_GATE() do {                                          if (confident_count == 3 && recovered_corner >= 0) {                   corners[2 * recovered_corner] = original_recovered[0];               corners[2 * recovered_corner + 1] = original_recovered[1];         }                                                                    return -1;                                                         } while (0)
  for (int c = 0; c < 4; ++c) {
    if (confidence[c] >= corner_threshold[c]) confident_count++;
    else recovered_corner = c;
  }
  if (confident_count < 3) GAP8_REJECT_GATE();
  if (confident_count == 3) {
    int opposite = (recovered_corner + 2) & 3;
    int previous = (recovered_corner + 3) & 3;
    int following = (recovered_corner + 1) & 3;
    original_recovered[0] = corners[2 * recovered_corner];
    original_recovered[1] = corners[2 * recovered_corner + 1];
    corners[2 * recovered_corner] =
        corners[2 * previous] + corners[2 * following]
        - corners[2 * opposite];
    corners[2 * recovered_corner + 1] =
        corners[2 * previous + 1] + corners[2 * following + 1]
        - corners[2 * opposite + 1];
    if (corners[2 * recovered_corner] < 0.0f ||
        corners[2 * recovered_corner] >= 160.0f ||
        corners[2 * recovered_corner + 1] < 20.0f ||
        corners[2 * recovered_corner + 1] >= 140.0f) GAP8_REJECT_GATE();
  }
  if (!(corners[0] < corners[2] && corners[6] < corners[4] &&
        corners[1] < corners[7] && corners[3] < corners[5])) GAP8_REJECT_GATE();
  float signed_area2 = 0.0f;
  float sign = 0.0f;
  for (int edge = 0; edge < 4; ++edge) {
    int next = (edge + 1) & 3;
    int after = (edge + 2) & 3;
    signed_area2 += corners[2 * edge] * corners[2 * next + 1]
                  - corners[2 * next] * corners[2 * edge + 1];
    float side = cross2(
        corners[2 * edge], corners[2 * edge + 1],
        corners[2 * next], corners[2 * next + 1],
        corners[2 * after], corners[2 * after + 1]);
    if (edge == 0) sign = side;
    if (side * sign <= 0.0f) GAP8_REJECT_GATE();
  }
  float area = signed_area2 < 0.0f ? -0.5f * signed_area2
                                  : 0.5f * signed_area2;
  if (area < 128.0f || area > 23000.0f) GAP8_REJECT_GATE();
  float width = 0.5f * (
      (corners[2] - corners[0]) + (corners[4] - corners[6]));
  float height = 0.5f * (
      (corners[7] - corners[1]) + (corners[5] - corners[3]));
  if (width <= 0.0f || height <= 0.0f ||
      width / height < 0.35f || width / height > 2.85f) GAP8_REJECT_GATE();
#undef GAP8_REJECT_GATE
  return recovered_corner >= 0 ? recovered_corner : 4;
}

void gap8_pool_control_maps(const uint8_t *packed,
                            uint8_t obstacle_presence[400],
                            uint8_t inverse_range[400],
                            uint8_t uncertainty[400],
                            uint8_t gate_opening[400]) {
  const uint8_t *danger = packed + DANGER_OFFSET;
  float corners[8];
  uint8_t confidence[4];
  gap8_decode_corner_argmax(packed, corners, confidence);
  memset(inverse_range, 0, 400);
  memset(uncertainty, 0, 400);
  memset(gate_opening, 0, 400);
  /* Nearest conservative expansion maps the 10x8 output into the central
   * 20x16 control rows. Top/bottom cropped regions are marked maximally
   * dangerous because the CNN did not observe them. */
  memset(obstacle_presence, 255, 400);
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 20; ++x) {
      obstacle_presence[(y + 2) * 20 + x] =
          danger[(y / 2) * DANGER_W + x / 2] >= DANGER_Q_THRESHOLD
              ? 255 : 0;
    }
  }

  /* Invalid geometry is a safe no-op. A recovered gate receives a more
   * conservative permission inset than a fully observed gate. */
  int recovered_corner = gap8_validate_or_recover_gate(corners, confidence);
  if (recovered_corner < 0) return;
  float sign = cross2(
      corners[0], corners[1], corners[2], corners[3],
      corners[4], corners[5]);
  float cx = 0.0f, cy = 0.0f, inset[8];
  for (int c = 0; c < 4; ++c) {
    cx += 0.25f * corners[2 * c];
    cy += 0.25f * corners[2 * c + 1];
  }
  float corner_weight = recovered_corner < 4 ? 0.80f : 0.88f;
  float center_weight = 1.0f - corner_weight;
  for (int c = 0; c < 4; ++c) {
    inset[2 * c] =
        corner_weight * corners[2 * c] + center_weight * cx;
    inset[2 * c + 1] =
        corner_weight * corners[2 * c + 1] + center_weight * cy;
  }
  for (int y = 0; y < 20; ++y) {
    for (int x = 0; x < 20; ++x) {
      float px = (x + 0.5f) * 8.0f;
      float py = (y + 0.5f) * 8.0f;
      int inside = 1;
      for (int edge = 0; edge < 4; ++edge) {
        int next = (edge + 1) & 3;
        float side = cross2(
            inset[2 * edge], inset[2 * edge + 1],
            inset[2 * next], inset[2 * next + 1], px, py);
        if (side * sign < 0.0f) { inside = 0; break; }
      }
      if (inside) gate_opening[y * 20 + x] = 255;
    }
  }
}
