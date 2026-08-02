#include "gap8_perception_output.h"

#include <math.h>

#define PERCEPTION_OUTPUT_EPSILON 0.06313495337963104f
#define PERCEPTION_CORNER_PEAK_MIN (-0.5f)
#define PERCEPTION_CORNER_AMBIGUITY_MIN 0.06f
#define PERCEPTION_MINIMUM_CONFIDENT_CORNERS 3
#define PERCEPTION_MINIMUM_AREA_PX2 100.0f
#define PERCEPTION_MINIMUM_TRIANGLE_AREA_PX2 \
    (0.5f * PERCEPTION_MINIMUM_AREA_PX2)
#define PERCEPTION_MAXIMUM_TRIANGLE_SIDE_RATIO 8.0f
#define PERCEPTION_MAXIMUM_QUAD_SIDE_RATIO 6.0f

static float logical_score(uint8_t value) {
    return (float)value * PERCEPTION_OUTPUT_EPSILON
         - PERCEPTION_TERMINAL_SCORE_OFFSET;
}

static uint8_t output_at(const uint8_t *packed, int channel, int y, int x) {
    return packed[(y * PERCEPTION_GRID_WIDTH + x) *
                      PERCEPTION_OUTPUT_CHANNELS + channel];
}

/* The deployed DORY terminal tensor presents its corner pairs right-to-left
 * relative to the model's semantic TL, TR, BR, BL contract. Keep the public
 * decoder canonical so validation, telemetry, and downstream control all see
 * TL, TR, BR, BL rather than compensating only in the diagnostic viewer. */
static int corner_source_channel(int semantic_corner) {
    static const uint8_t source_channels[4] = {1, 0, 3, 2};
    return source_channels[semantic_corner];
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
    for (int corner = 0; corner < 4; ++corner) {
        const int channel = corner_source_channel(corner);
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

        corners_xy_crop[2 * corner] = 8.0f * (best_x + 0.5f) - 0.5f;
        corners_xy_crop[2 * corner + 1] = 8.0f * (best_y + 0.5f) - 0.5f;
        corner_peaks[corner] = logical_score(best);
        corner_ambiguity[corner] = logical_score(best) - logical_score(second);
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
                                 const float corner_ambiguity[4],
                                 uint8_t *rejection_reason,
                                 uint8_t *confident_corner_mask) {
    int confident[4];
    int confident_count = 0;
    uint8_t confident_mask = 0;
    float signed_area2 = 0.0f;
    float sign = 0.0f;
    float side_min = 0.0f;
    float side_max = 0.0f;

    for (int corner = 0; corner < 4; ++corner) {
        if (corner_peaks[corner] >= PERCEPTION_CORNER_PEAK_MIN &&
            corner_ambiguity[corner] >= PERCEPTION_CORNER_AMBIGUITY_MIN) {
            confident[confident_count++] = corner;
            confident_mask |= (uint8_t)(1U << corner);
        }
    }
    if (confident_corner_mask) *confident_corner_mask = confident_mask;
    if (confident_count < PERCEPTION_MINIMUM_CONFIDENT_CORNERS) {
        if (rejection_reason) *rejection_reason = GAP8_GATE_REJECT_CONFIDENCE;
        return 0;
    }

    /* With exactly three confident heatmaps, validate only those observations.
     * The fourth heatmap still supplies a candidate coordinate for telemetry,
     * but cannot veto an otherwise coherent partial gate. */
    if (confident_count == 3) {
        const int a = confident[0];
        const int b = confident[1];
        const int c = confident[2];
        const float triangle_cross = cross2(
            corners[2 * a], corners[2 * a + 1],
            corners[2 * b], corners[2 * b + 1],
            corners[2 * c], corners[2 * c + 1]);
        const float triangle_area = triangle_cross < 0.0f
                                      ? -0.5f * triangle_cross
                                      : 0.5f * triangle_cross;
        const int triangle[3] = {a, b, c};
        for (int edge = 0; edge < 3; ++edge) {
            const int from = triangle[edge];
            const int to = triangle[(edge + 1) % 3];
            const float dx = corners[2 * to] - corners[2 * from];
            const float dy = corners[2 * to + 1] - corners[2 * from + 1];
            const float side = sqrtf(dx * dx + dy * dy);
            if (edge == 0 || side < side_min) side_min = side;
            if (edge == 0 || side > side_max) side_max = side;
        }
        if (triangle_area < PERCEPTION_MINIMUM_TRIANGLE_AREA_PX2) {
            if (rejection_reason) {
                *rejection_reason = GAP8_GATE_REJECT_TRIANGLE_AREA;
            }
            return 0;
        }
        if (side_min <= 0.0f ||
            side_max / side_min > PERCEPTION_MAXIMUM_TRIANGLE_SIDE_RATIO) {
            if (rejection_reason) {
                *rejection_reason = GAP8_GATE_REJECT_TRIANGLE_RATIO;
            }
            return 0;
        }
        if (rejection_reason) *rejection_reason = GAP8_GATE_ACCEPTED;
        return 1;
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
        if (cross * sign <= 0.0f) {
            if (rejection_reason) {
                *rejection_reason = GAP8_GATE_REJECT_QUAD_CONVEXITY;
            }
            return 0;
        }
        signed_area2 += corners[2 * edge] * corners[2 * next + 1]
                      - corners[2 * next] * corners[2 * edge + 1];
        if (edge == 0 || side < side_min) side_min = side;
        if (edge == 0 || side > side_max) side_max = side;
    }

    const float area = signed_area2 < 0.0f ? -0.5f * signed_area2
                                           : 0.5f * signed_area2;
    if (area < PERCEPTION_MINIMUM_AREA_PX2) {
        if (rejection_reason) *rejection_reason = GAP8_GATE_REJECT_QUAD_AREA;
        return 0;
    }
    if (side_min <= 0.0f ||
        side_max / side_min > PERCEPTION_MAXIMUM_QUAD_SIDE_RATIO) {
        if (rejection_reason) *rejection_reason = GAP8_GATE_REJECT_QUAD_RATIO;
        return 0;
    }
    if (rejection_reason) *rejection_reason = GAP8_GATE_ACCEPTED;
    return 1;
}
