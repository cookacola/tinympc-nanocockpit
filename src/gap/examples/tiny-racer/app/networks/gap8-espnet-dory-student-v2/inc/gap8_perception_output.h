#ifndef GAP8_PERCEPTION_OUTPUT_H
#define GAP8_PERCEPTION_OUTPUT_H
#include <stdint.h>
#define GAP8_INPUT_WIDTH 160
#define GAP8_INPUT_HEIGHT 160
#define GAP8_INPUT_CHANNELS 2
#define GAP8_OUTPUT_BYTES 8100
#define GAP8_CONTROL_WIDTH 20
#define GAP8_CONTROL_HEIGHT 20
#define GAP8_CORNER_Q_THRESHOLD_0 191
#define GAP8_CORNER_Q_THRESHOLD_1 220
#define GAP8_CORNER_Q_THRESHOLD_2 201
#define GAP8_CORNER_Q_THRESHOLD_3 172
#define GAP8_GATE_Q_THRESHOLD 214
#define GAP8_DANGER_Q_THRESHOLD 216
#define GAP8_DANGER_QUANT_EPSILON 0.1509468406f
#define GAP8_DANGER_QUANT_OFFSET 31.13149725f
#define GAP8_DANGER_QUANT_BIAS 0.1076813042f
#define GAP8_DANGER_PROBABILITY_THRESHOLD 0.822892487f
void gap8_decode_corner_argmax(const uint8_t *packed,
                               float corners_xy[8], uint8_t confidence[4]);
int gap8_validate_or_recover_gate(float corners_xy[8],
                                  const uint8_t confidence[4]);
void gap8_pool_control_maps(const uint8_t *packed,
                            uint8_t obstacle_presence[400],
                            uint8_t inverse_range[400],
                            uint8_t uncertainty[400],
                            uint8_t gate_opening[400]);
#endif
