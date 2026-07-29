#include "gap8_perception_output.h"
#include <string.h>

#define CORNER_W 40
#define CORNER_H 30
#define CORNER_C 4
#define DANGER_W 10
#define DANGER_H 8
#define DANGER_OFFSET (CORNER_W * CORNER_H * CORNER_C)
static const uint8_t corner_threshold[4] = {136, 141, 187, 131};
#define DANGER_Q_THRESHOLD 42

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

  /* A gate permission map is emitted only after confidence, ordering,
   * convexity, area, and aspect checks. Invalid geometry is a safe no-op. */
  for (int c = 0; c < 4; ++c) {
    if (confidence[c] < corner_threshold[c]) return;
  }
  if (!(corners[0] < corners[2] && corners[6] < corners[4] &&
        corners[1] < corners[7] && corners[3] < corners[5])) return;
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
    if (side * sign <= 0.0f) return;
  }
  float area = signed_area2 < 0.0f ? -0.5f * signed_area2
                                  : 0.5f * signed_area2;
  if (area < 128.0f || area > 23000.0f) return;
  float width = 0.5f * (
      (corners[2] - corners[0]) + (corners[4] - corners[6]));
  float height = 0.5f * (
      (corners[7] - corners[1]) + (corners[5] - corners[3]));
  if (width <= 0.0f || height <= 0.0f ||
      width / height < 0.35f || width / height > 2.85f) return;
  float cx = 0.0f, cy = 0.0f, inset[8];
  for (int c = 0; c < 4; ++c) {
    cx += 0.25f * corners[2 * c];
    cy += 0.25f * corners[2 * c + 1];
  }
  for (int c = 0; c < 4; ++c) {
    inset[2 * c] = 0.88f * corners[2 * c] + 0.12f * cx;
    inset[2 * c + 1] = 0.88f * corners[2 * c + 1] + 0.12f * cy;
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
