#ifndef GAP8_PERCEPTION_OUTPUT_H
#define GAP8_PERCEPTION_OUTPUT_H

#include "perception_output_contract.h"

#include <stdint.h>

#define GAP8_OUTPUT_BYTES \
    (PERCEPTION_GRID_WIDTH * PERCEPTION_GRID_HEIGHT * PERCEPTION_OUTPUT_CHANNELS)

typedef enum {
    GAP8_GATE_ACCEPTED = 0,
    GAP8_GATE_REJECT_CONFIDENCE = 1,
    GAP8_GATE_REJECT_TRIANGLE_AREA = 2,
    GAP8_GATE_REJECT_TRIANGLE_RATIO = 3,
    GAP8_GATE_REJECT_QUAD_CONVEXITY = 4,
    GAP8_GATE_REJECT_QUAD_AREA = 5,
    GAP8_GATE_REJECT_QUAD_RATIO = 6,
} gap8_gate_rejection_e;

void gap8_decode_sequential_output(const uint8_t *packed,
                                   float corners_xy_crop[8],
                                   float corner_peaks[4],
                                   float corner_ambiguity[4],
                                   float offsets_m[4],
                                   float confidence_scores[4]);

int gap8_validate_gate_candidate(const float corners_xy_crop[8],
                                 const float corner_peaks[4],
                                 const float corner_ambiguity[4],
                                 uint8_t *rejection_reason,
                                 uint8_t *confident_corner_mask);

#endif
