#include "gap8_perception_output.h"

const float gap8_output_epsilon = 0.1988997012f;
const float gap8_output_spatial_offset[GAP8_OUTPUT_CHANNELS] = {8.042428835f, 11.11182962f, 11.5725678f, 8.653360231f, 12.78889833f, 7.226821764f, 5.082098348f, 36.62235342f};
const float gap8_output_learned_bias[GAP8_OUTPUT_CHANNELS] = {-5.684194088f, -0.3608348966f, -0.453181088f, -3.457415581f, 0.1745830327f, 1.191730738f, 1.386082292f, -1.406413555f};

void gap8_decode_corner_argmax(const uint8_t *packed_hwc,
                               float corners_xy[8],
                               uint8_t confidence[4]) {
  for (int channel = 0; channel < 4; ++channel) {
    uint8_t best = 0;
    int best_x = 0;
    int best_y = 0;
    for (int y = 0; y < GAP8_OUTPUT_HEIGHT; ++y) {
      for (int x = 0; x < GAP8_OUTPUT_WIDTH; ++x) {
        uint8_t value = packed_hwc[gap8_output_index(
            x, y, (gap8_output_channel_t)channel)];
        if (value > best) {
          best = value;
          best_x = x;
          best_y = y;
        }
      }
    }
    corners_xy[2 * channel] = (best_x + 0.5f) * 4.0f;
    corners_xy[2 * channel + 1] = (best_y + 0.5f) * 4.0f;
    confidence[channel] = best;
  }
}

void gap8_pool_control_maps(const uint8_t *packed_hwc,
                            uint8_t obstacle_presence[400],
                            uint8_t inverse_range[400],
                            uint8_t uncertainty[400],
                            uint8_t gate_opening[400]) {
  const int channels[4] = {
    GAP8_OUTPUT_OBSTACLE_PRESENCE, GAP8_OUTPUT_INVERSE_RANGE,
    GAP8_OUTPUT_UNCERTAINTY, GAP8_OUTPUT_GATE
  };
  uint8_t *outputs[4] = {
    obstacle_presence, inverse_range, uncertainty, gate_opening
  };
  for (int oy = 0; oy < GAP8_CONTROL_HEIGHT; ++oy) {
    for (int ox = 0; ox < GAP8_CONTROL_WIDTH; ++ox) {
      for (int map = 0; map < 4; ++map) {
        /* A single dangerous/near/uncertain source pixel must survive
         * downsampling. Gate opening is a permission signal, so require all
         * four source pixels to agree by conservatively min-pooling it. */
        uint8_t pooled = map == 3 ? 255 : 0;
        for (int dy = 0; dy < 2; ++dy) {
          for (int dx = 0; dx < 2; ++dx) {
            uint8_t value = packed_hwc[gap8_output_index(
                2 * ox + dx, 2 * oy + dy,
                (gap8_output_channel_t)channels[map])];
            if (map == 3) {
              if (value < pooled) pooled = value;
            } else if (value > pooled) {
              pooled = value;
            }
          }
        }
        outputs[map][oy * GAP8_CONTROL_WIDTH + ox] = pooled;
      }
    }
  }
}
