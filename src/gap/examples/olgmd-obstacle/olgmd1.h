#ifndef GATE_OLGMD1_H
#define GATE_OLGMD1_H

#include <stdbool.h>
#include <stdint.h>

#define OLGMD1_INPUT_WIDTH 160
#define OLGMD1_INPUT_HEIGHT 160
#define OLGMD1_WIDTH 80
#define OLGMD1_HEIGHT 80
#define OLGMD1_PIXELS (OLGMD1_WIDTH * OLGMD1_HEIGHT)

/*
 * Fixed-point oLGMD1 configuration. Alphas and adaptive inhibition weights are
 * Q8; sigmoid/SFA values are unsigned Q15. Defaults assume a 30 Hz camera.
 */
typedef struct {
  uint8_t temporal_threshold;
  uint16_t ffi_threshold_q8;
  uint16_t base_on_weight_q8;
  uint16_t base_off_weight_q8;
  uint8_t grouping_threshold;
  uint16_t spike_threshold_q15;
  uint8_t collision_window;
  uint8_t collision_spikes;
} olgmd1_config_t;

typedef struct {
  uint8_t previous_image[OLGMD1_PIXELS];
  uint8_t on[2][OLGMD1_PIXELS];
  uint8_t off[2][OLGMD1_PIXELS];
  uint16_t ffi_on_q8;
  uint16_t ffi_off_q8;
  uint16_t membrane_q15;
  uint16_t previous_sigmoid_q15;
  uint8_t current_plane;
  uint8_t spike_history[8];
  bool initialized;
} olgmd1_state_t;

/* Two reusable image planes: downsample/Ce and S. */
typedef struct {
  uint8_t work[OLGMD1_PIXELS];
  uint8_t summation[OLGMD1_PIXELS];
} olgmd1_scratch_t;

typedef struct {
  bool valid;
  bool threat;
  uint16_t membrane_q15;
  uint16_t ffi_on_q8;
  uint16_t ffi_off_q8;
  uint8_t active_polarities; /* bit 0: ON, bit 1: OFF */
  uint8_t spike_count;
} olgmd1_result_t;

olgmd1_config_t olgmd1_default_config(void);
void olgmd1_init(olgmd1_state_t *state);
olgmd1_result_t olgmd1_step(
    olgmd1_state_t *state, olgmd1_scratch_t *scratch,
    const uint8_t *frame, uint16_t stride,
    const olgmd1_config_t *config);

#endif
