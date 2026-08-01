#ifndef GAP8_PERCEPTION_OUTPUT_H
#define GAP8_PERCEPTION_OUTPUT_H

#include "perception_output_contract.h"

#include <stdint.h>

#define GAP8_OUTPUT_BYTES \
    (PERCEPTION_GRID_WIDTH * PERCEPTION_GRID_HEIGHT * PERCEPTION_OUTPUT_CHANNELS)

void gap8_decode_sequential_output(const uint8_t *packed,
                                   float corners_xy_crop[8],
                                   float corner_peaks[4],
                                   float corner_ambiguity[4],
                                   float offsets_m[4],
                                   float confidence_scores[4]);

int gap8_validate_gate_candidate(const float corners_xy_crop[8],
                                 const float corner_peaks[4],
                                 const float corner_ambiguity[4]);

#endif
