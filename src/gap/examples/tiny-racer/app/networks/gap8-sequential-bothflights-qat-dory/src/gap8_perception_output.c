#include "gap8_perception_output.h"

#include <math.h>

#define PERCEPTION_OUTPUT_EPSILON 0.11307859420776367f
#define PERCEPTION_CORNER_PEAK_MIN 0.0f
#define PERCEPTION_CORNER_AMBIGUITY_MIN 0.5f
#define PERCEPTION_MINIMUM_AREA_PX2 100.0f
#define PERCEPTION_MAXIMUM_SIDE_RATIO 6.0f

static float logical_score(uint8_t value) {
    return (float)value * PERCEPTION_OUTPUT_EPSILON
         - PERCEPTION_TERMINAL_SCORE_OFFSET;
}

static uint8_t output_at(const uint8_t *packed, int channel, int y, int x) {
    return packed[(y * PERCEPTION_GRID_WIDTH + x) *
                      PERCEPTION_OUTPUT_CHANNELS + channel];
}

static float cross2(float ax, float ay, float bx, float by,
                    float px, float py) {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

void gap8_decode_sequential_output(const uint8_t *packed,
                                   float corners_xy_crop[8],
                                   float corner_peaks[4],
                                   float corner_ambiguity[4],
                                   float offsets_m[4],
                                   float confidence_scores[4]) {
    for (int channel = 0; channel < 4; ++channel) {
        uint8_t best = 0;
        int best_x = 0;
        int best_y = 0;
        for (int y = 0; y < PERCEPTION_GRID_HEIGHT; ++y) {
            for (int x = 0; x < PERCEPTION_GRID_WIDTH; ++x) {
                const uint8_t value = output_at(packed, channel, y, x);
                if (value > best) {
                    best = value;
                    best_x = x;
                    best_y = y;
                }
            }
        }

        uint8_t second = 0;
        for (int y = 0; y < PERCEPTION_GRID_HEIGHT; ++y) {
            for (int x = 0; x < PERCEPTION_GRID_WIDTH; ++x) {
                if (x >= best_x - 1 && x <= best_x + 1 &&
                    y >= best_y - 1 && y <= best_y + 1) {
                    continue;
                }
                const uint8_t value = output_at(packed, channel, y, x);
                if (value > second) second = value;
            }
        }

        corners_xy_crop[2 * channel] = 8.0f * (best_x + 0.5f) - 0.5f;
        corners_xy_crop[2 * channel + 1] = 8.0f * (best_y + 0.5f) - 0.5f;
        corner_peaks[channel] = logical_score(best);
        corner_ambiguity[channel] = logical_score(best) - logical_score(second);
    }

    for (int field = 0; field < 4; ++field) {
        float offset_sum = 0.0f;
        float confidence_sum = 0.0f;
        for (int y = 0; y < PERCEPTION_GRID_HEIGHT; ++y) {
            for (int x = 0; x < PERCEPTION_GRID_WIDTH; ++x) {
                offset_sum += logical_score(output_at(
                    packed, PERCEPTION_OFFSET_BASE + field, y, x));
                confidence_sum += logical_score(output_at(
                    packed, PERCEPTION_CONFIDENCE_BASE + field, y, x));
            }
        }
        const float cells = (float)(PERCEPTION_GRID_WIDTH * PERCEPTION_GRID_HEIGHT);
        float offset_score = offset_sum / cells;
        if (offset_score < -PERCEPTION_SCORE_LIMIT) {
            offset_score = -PERCEPTION_SCORE_LIMIT;
        } else if (offset_score > PERCEPTION_SCORE_LIMIT) {
            offset_score = PERCEPTION_SCORE_LIMIT;
        }
        offsets_m[field] = PERCEPTION_OFFSET_MIN +
            (offset_score + PERCEPTION_SCORE_LIMIT) /
            (2.0f * PERCEPTION_SCORE_LIMIT) *
            (PERCEPTION_OFFSET_MAX - PERCEPTION_OFFSET_MIN);
        confidence_scores[field] = confidence_sum / cells;
    }
}

int gap8_validate_gate_candidate(const float corners[8],
                                 const float corner_peaks[4],
                                 const float corner_ambiguity[4]) {
    float signed_area2 = 0.0f;
    float sign = 0.0f;
    float side_min = 0.0f;
    float side_max = 0.0f;

    for (int corner = 0; corner < 4; ++corner) {
        if (corner_peaks[corner] < PERCEPTION_CORNER_PEAK_MIN ||
            corner_ambiguity[corner] < PERCEPTION_CORNER_AMBIGUITY_MIN) {
            return 0;
        }
    }
    for (int edge = 0; edge < 4; ++edge) {
        const int next = (edge + 1) & 3;
        const int after = (edge + 2) & 3;
        const float dx = corners[2 * next] - corners[2 * edge];
        const float dy = corners[2 * next + 1] - corners[2 * edge + 1];
        const float side_squared = dx * dx + dy * dy;
        const float side = sqrtf(side_squared);
        const float cross = cross2(corners[2 * edge], corners[2 * edge + 1],
                                   corners[2 * next], corners[2 * next + 1],
                                   corners[2 * after], corners[2 * after + 1]);
        if (edge == 0) sign = cross;
        if (cross * sign <= 0.0f) return 0;
        signed_area2 += corners[2 * edge] * corners[2 * next + 1]
                      - corners[2 * next] * corners[2 * edge + 1];
        if (edge == 0 || side < side_min) side_min = side;
        if (edge == 0 || side > side_max) side_max = side;
    }

    const float area = signed_area2 < 0.0f ? -0.5f * signed_area2
                                           : 0.5f * signed_area2;
    return area >= PERCEPTION_MINIMUM_AREA_PX2 && side_min > 0.0f &&
           side_max / side_min <= PERCEPTION_MAXIMUM_SIDE_RATIO;
}
