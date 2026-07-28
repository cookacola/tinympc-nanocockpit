#ifndef GAP8_PERCEPTION_OUTPUT_H
#define GAP8_PERCEPTION_OUTPUT_H

#include <stdint.h>

#define GAP8_INPUT_WIDTH 160
#define GAP8_INPUT_HEIGHT 160
#define GAP8_OUTPUT_WIDTH 40
#define GAP8_OUTPUT_HEIGHT 40
#define GAP8_OUTPUT_CHANNELS 8
#define GAP8_OUTPUT_BYTES (GAP8_OUTPUT_WIDTH * GAP8_OUTPUT_HEIGHT * GAP8_OUTPUT_CHANNELS)
#define GAP8_CONTROL_WIDTH 20
#define GAP8_CONTROL_HEIGHT 20

typedef enum {
  GAP8_OUTPUT_CORNER_TL = 0,
  GAP8_OUTPUT_CORNER_TR = 1,
  GAP8_OUTPUT_CORNER_BR = 2,
  GAP8_OUTPUT_CORNER_BL = 3,
  GAP8_OUTPUT_OBSTACLE_PRESENCE = 4,
  GAP8_OUTPUT_INVERSE_RANGE = 5,
  GAP8_OUTPUT_UNCERTAINTY = 6,
  GAP8_OUTPUT_GATE = 7,
} gap8_output_channel_t;

extern const float gap8_output_epsilon;
extern const float gap8_output_spatial_offset[GAP8_OUTPUT_CHANNELS];
extern const float gap8_output_learned_bias[GAP8_OUTPUT_CHANNELS];

static inline uint32_t gap8_output_index(uint32_t x, uint32_t y,
                                         gap8_output_channel_t channel) {
  return (y * GAP8_OUTPUT_WIDTH + x) * GAP8_OUTPUT_CHANNELS + channel;
}

static inline float gap8_output_logit(uint8_t value,
                                      gap8_output_channel_t channel) {
  return value * gap8_output_epsilon
       - gap8_output_spatial_offset[channel]
       + gap8_output_learned_bias[channel];
}

void gap8_decode_corner_argmax(const uint8_t *packed_hwc,
                               float corners_xy[8],
                               uint8_t confidence[4]);
void gap8_pool_control_maps(const uint8_t *packed_hwc,
                            uint8_t obstacle_presence[400],
                            uint8_t inverse_range[400],
                            uint8_t uncertainty[400],
                            uint8_t gate_opening[400]);

#endif
