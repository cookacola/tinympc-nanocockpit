#ifndef GAP8_PERCEPTION_OUTPUT_H
#define GAP8_PERCEPTION_OUTPUT_H
#include <stdint.h>
#define GAP8_INPUT_WIDTH 160
#define GAP8_INPUT_HEIGHT 120
#define GAP8_OUTPUT_BYTES 4880
#define GAP8_CONTROL_WIDTH 20
#define GAP8_CONTROL_HEIGHT 20
void gap8_decode_corner_argmax(const uint8_t *packed,
                               float corners_xy[8],
                               uint8_t confidence[4]);
void gap8_pool_control_maps(const uint8_t *packed,
                            uint8_t obstacle_presence[400],
                            uint8_t inverse_range[400],
                            uint8_t uncertainty[400],
                            uint8_t gate_opening[400]);
#endif
