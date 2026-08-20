#ifndef GAP8_PERCEPTION_OUTPUT_H
#define GAP8_PERCEPTION_OUTPUT_H
#include <math.h>
#include <stdint.h>
#define GAP8_INPUT_WIDTH 160
#define GAP8_INPUT_HEIGHT 160
#define GAP8_INPUT_CHANNELS 2
#define GAP8_OUTPUT_BYTES 2003
#define GAP8_CORNER_OFFSET 0
#define GAP8_GATE_OFFSET 1600
#define GAP8_PRESENCE_OFFSET 2000
#define GAP8_NAVIGATION_OFFSET 2001
static const float gap8_corner_epsilon = 0.1555055827f;
static const float gap8_gate_epsilon = 2.960508347f;
static const float gap8_presence_epsilon = 0.1207355261f;
static const float gap8_navigation_epsilon = 0.06085930765f;
static const float gap8_corner_offset[4] = {29.64420019f, 28.75417219f, 35.61135946f, 25.24959837f};
static const float gap8_corner_bias[4] = {-0.1234570593f, -0.1256028265f, -0.05130393803f, -0.401463747f};
static const float gap8_gate_offset = 703.5765771f;
static const float gap8_gate_bias = -0.2522831857f;
static const float gap8_gate_logit_scale = 1.0f;
static const float gap8_presence_offset = 5.558176859f;
static const float gap8_presence_bias = 0.01029471494f;
static const float gap8_navigation_offset[2] = {1.242780192f, 7.570797308f};
static const float gap8_navigation_bias[2] = {0.0f, 0.0f};
static const float gap8_presence_logit_scale = 4.0f;
static const float gap8_navigation_logit_scale[2] = {1.0f, 8.0f};
static const float gap8_navigation_yaw_calibration[2] = {0.07248172055f, -0.0005410605551f};
static const float gap8_navigation_collision_calibration[2] = {0.1308291176f, -0.445862131f};
static const float gap8_confidence_mean[3] = {0.7985369852f, 0.266170449f, 0.6138960815f};
static const float gap8_confidence_scale[3] = {0.2844120154f, 0.3834623682f, 0.149421324f};
static const float gap8_confidence_weight[3] = {1.214142425f, 0.2318861439f, 1.305251985f};
static const float gap8_confidence_bias = 1.114017256f;
static const float gap8_confidence_threshold = 0.6174671283f;
static const float gap8_collision_probability_threshold = 0.4936617552f;
static const uint8_t gap8_corner_q_threshold[4] = {191, 186, 229, 165};
static inline void gap8_decode_corner_argmax(const uint8_t *packed,
                                              float corners_xy[8],
                                              uint8_t confidence[4]) {
  for (int channel = 0; channel < 4; ++channel) {
    uint8_t best = 0; int best_x = 0, best_y = 0;
    for (int y = 0; y < 20; ++y) for (int x = 0; x < 20; ++x) {
      const uint8_t value = packed[(y * 20 + x) * 4 + channel];
      if (value > best) { best = value; best_x = x; best_y = y; }
    }
    corners_xy[2 * channel] = (best_x + 0.5f) * 8.0f;
    corners_xy[2 * channel + 1] = (best_y + 0.5f) * 8.0f;
    confidence[channel] = best;
  }
}
static inline int gap8_validate_or_recover_gate(float corners_xy[8],
                                                 const uint8_t confidence[4]) {
  (void)corners_xy; int valid = 0;
  for (int i = 0; i < 4; ++i) valid += confidence[i] >= gap8_corner_q_threshold[i];
  return valid >= 3 ? 4 : -1;
}
static inline float gap8_decode_presence_logit(uint8_t quantized) {
  return (((float)quantized * gap8_presence_epsilon
           - gap8_presence_offset + gap8_presence_bias)
          * gap8_presence_logit_scale);
}
static inline void gap8_decode_navigation(const uint8_t quantized[2],
                                           float *yaw,
                                           float *collision_probability) {
  float decoded_yaw =
      ((float)quantized[0] * gap8_navigation_epsilon
       - gap8_navigation_offset[0] + gap8_navigation_bias[0])
      * gap8_navigation_logit_scale[0];
  float collision_logit =
      ((float)quantized[1] * gap8_navigation_epsilon
       - gap8_navigation_offset[1] + gap8_navigation_bias[1])
      * gap8_navigation_logit_scale[1];
  decoded_yaw = decoded_yaw * gap8_navigation_yaw_calibration[0]
                + gap8_navigation_yaw_calibration[1];
  collision_logit =
      collision_logit * gap8_navigation_collision_calibration[0]
      + gap8_navigation_collision_calibration[1];
  *yaw = decoded_yaw;
  *collision_probability = 1.0f / (1.0f + expf(-collision_logit));
}
#endif
