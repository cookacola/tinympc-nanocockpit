#include "olgmd1.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

/* 30 Hz coefficients, round(256 * dt/(dt + tau)). */
#define DECAY_LEFT_Q8 26u
#define FFI_ALPHA_Q8 197u
#define ON_SELF_ALPHA_Q8 177u
#define ON_ADJ_ALPHA_Q8 135u
#define ON_DIAG_ALPHA_Q8 109u
#define OFF_SELF_ALPHA_Q8 91u
#define OFF_ADJ_ALPHA_Q8 56u
/* tau_off_diag = 180 ms. The reference C accidentally assigns ON diagonal. */
#define OFF_DIAG_ALPHA_Q8 40u
#define SFA_ALPHA_Q8 240u
#define SFA_THRESHOLD_Q15 98
#define FFI_WEIGHT_DIVISOR 8u
#define GROUP_CW 4u
#define GROUP_DELTA_Q8 3u

/* sigmoid(x), x = MP/(N*0.5), Q15, sampled every 1/8 over [0, 8]. */
static const uint16_t sigmoid_q15[65] = {
  16384, 17406, 18421, 19420, 20396, 21343, 22255, 23126,
  23955, 24736, 25470, 26154, 26789, 27376, 27916, 28410,
  28861, 29271, 29643, 29979, 30281, 30554, 30798, 31017,
  31213, 31388, 31544, 31683, 31807, 31916, 32014, 32101,
  32178, 32246, 32306, 32360, 32407, 32449, 32486, 32519,
  32548, 32573, 32596, 32616, 32634, 32649, 32663, 32675,
  32686, 32695, 32704, 32711, 32718, 32724, 32729, 32733,
  32737, 32741, 32744, 32746, 32749, 32751, 32753, 32755,
  32756,
};

static uint8_t saturate_u8(uint32_t value) {
  return value > UINT8_MAX ? UINT8_MAX : (uint8_t)value;
}

static uint16_t iir_q8(uint16_t input_q8, uint16_t previous_q8,
                       uint16_t alpha_q8) {
  const uint32_t value = alpha_q8 * input_q8 +
      (256u - alpha_q8) * previous_q8 + 128u;
  return (uint16_t)(value >> 8);
}

static uint8_t delayed(uint8_t current, uint8_t previous,
                       uint16_t alpha_q8) {
  return (uint8_t)((alpha_q8 * current +
      (256u - alpha_q8) * previous + 128u) >> 8);
}

static void downsample_2x2(uint8_t output[OLGMD1_PIXELS],
                           const uint8_t *frame, uint16_t stride) {
  for (int y = 0; y < OLGMD1_HEIGHT; ++y) {
    const uint8_t *row0 = frame + (2 * y) * stride;
    const uint8_t *row1 = row0 + stride;
    for (int x = 0; x < OLGMD1_WIDTH; ++x) {
      const int sx = 2 * x;
      const uint16_t sum = (uint16_t)row0[sx] + row0[sx + 1] +
          row1[sx] + row1[sx + 1];
      output[y * OLGMD1_WIDTH + x] = (uint8_t)((sum + 2u) >> 2);
    }
  }
}

static uint16_t adaptive_weight(uint16_t ffi_q8, uint16_t base_q8) {
  const uint16_t adaptive_q8 = ffi_q8 / FFI_WEIGHT_DIVISOR;
  return adaptive_q8 > base_q8 ? adaptive_q8 : base_q8;
}

static uint32_t inhibition_q8(const uint8_t *current,
                              const uint8_t *previous, int index,
                              uint16_t self_alpha, uint16_t adj_alpha,
                              uint16_t diag_alpha) {
  uint32_t value = (uint32_t)delayed(
      current[index], previous[index], self_alpha) << 8;
  const int adjacent[4] = {-1, 1, -OLGMD1_WIDTH, OLGMD1_WIDTH};
  const int diagonal[4] = {
      -OLGMD1_WIDTH - 1, -OLGMD1_WIDTH + 1,
       OLGMD1_WIDTH - 1,  OLGMD1_WIDTH + 1,
  };
  for (int n = 0; n < 4; ++n) {
    value += (uint32_t)delayed(current[index + adjacent[n]],
                               previous[index + adjacent[n]], adj_alpha) << 6;
    value += (uint32_t)delayed(current[index + diagonal[n]],
                               previous[index + diagonal[n]], diag_alpha) << 5;
  }
  return value;
}

static uint16_t sigmoid_from_mp(uint32_t membrane_potential) {
  /* LUT index is round(8 * 2*MP/N), hence MP/400 for N=6400. */
  uint32_t index = (membrane_potential + 200u) / 400u;
  if (index > 64u) index = 64u;
  return sigmoid_q15[index];
}

/*
 * Paper equation: S_spike = floor(exp(4 * (K - T_spike))).
 * Comparing in Q15 against ceil(ln(n) * 2^15 / 4) avoids floating point.
 * Counts saturate at eight because the paper's collision threshold is 6--8.
 */
static uint8_t paper_spike_count_q15(uint16_t membrane_q15,
                                     uint16_t threshold_q15) {
  static const uint16_t count_offsets_q15[7] = {
      5679u, 9000u, 11357u, 13185u, 14679u, 15941u, 17035u,
  };
  if (membrane_q15 < threshold_q15) return 0u;
  const uint32_t delta = (uint32_t)membrane_q15 - threshold_q15;
  uint8_t count = 1u;
  while (count < 8u && delta >= count_offsets_q15[count - 1u]) ++count;
  return count;
}

olgmd1_config_t olgmd1_default_config(void) {
  return (olgmd1_config_t) {
      .temporal_threshold = 1,
      .ffi_threshold_q8 = 1u << 8,
      .base_on_weight_q8 = 1u << 8,
      .base_off_weight_q8 = 77u,
      .grouping_threshold = 35,
      .spike_threshold_q15 = 22938u, /* round(0.70 * 2^15) */
      .collision_window = 6,
      .collision_spikes = 6,
  };
}

void olgmd1_init(olgmd1_state_t *state) {
  memset(state, 0, sizeof(*state));
  /* The equation-faithful SFA state starts at sigmoid(0) = 0.5. */
  state->membrane_q15 = 16384u;
  state->previous_sigmoid_q15 = 16384u;
}

olgmd1_result_t olgmd1_step(
    olgmd1_state_t *state, olgmd1_scratch_t *scratch,
    const uint8_t *frame, uint16_t stride,
    const olgmd1_config_t *config) {
  olgmd1_result_t result = {0};
  downsample_2x2(scratch->work, frame, stride);

  if (!state->initialized) {
    memcpy(state->previous_image, scratch->work, OLGMD1_PIXELS);
    state->initialized = true;
    return result;
  }

  const unsigned previous_plane = state->current_plane;
  const unsigned current_plane = previous_plane ^ 1u;
  uint8_t *on_current = state->on[current_plane];
  uint8_t *off_current = state->off[current_plane];
  const uint8_t *on_previous = state->on[previous_plane];
  const uint8_t *off_previous = state->off[previous_plane];
  uint32_t on_sum = 0;
  uint32_t off_sum = 0;

  /* P layer and persistent half-wave ON/OFF rectification. */
  for (int i = 0; i < OLGMD1_PIXELS; ++i) {
    int difference = (int)scratch->work[i] - state->previous_image[i];
    if (difference > 127) difference = 127;
    if (difference < -127) difference = -127;
    uint32_t on_value = (DECAY_LEFT_Q8 * on_previous[i] + 128u) >> 8;
    uint32_t off_value = (DECAY_LEFT_Q8 * off_previous[i] + 128u) >> 8;
    if (difference >= config->temporal_threshold) {
      on_value += (uint32_t)difference;
    } else if (difference <= -(int)config->temporal_threshold) {
      off_value += (uint32_t)-difference;
    }
    on_current[i] = saturate_u8(on_value);
    off_current[i] = saturate_u8(off_value);
    on_sum += on_current[i];
    off_sum += off_current[i];
    state->previous_image[i] = scratch->work[i];
  }

  const uint16_t on_mean_q8 = (uint16_t)((on_sum << 8) / OLGMD1_PIXELS);
  const uint16_t off_mean_q8 = (uint16_t)((off_sum << 8) / OLGMD1_PIXELS);
  state->ffi_on_q8 = iir_q8(on_mean_q8, state->ffi_on_q8, FFI_ALPHA_Q8);
  state->ffi_off_q8 = iir_q8(off_mean_q8, state->ffi_off_q8, FFI_ALPHA_Q8);
  const bool on_active = state->ffi_on_q8 >= config->ffi_threshold_q8;
  const bool off_active = state->ffi_off_q8 >= config->ffi_threshold_q8;
  const uint16_t on_weight_q8 = adaptive_weight(
      state->ffi_on_q8, config->base_on_weight_q8);
  const uint16_t off_weight_q8 = adaptive_weight(
      state->ffi_off_q8, config->base_off_weight_q8);

  memset(scratch->summation, 0, OLGMD1_PIXELS);
  for (int y = 1; y < OLGMD1_HEIGHT - 1; ++y) {
    for (int x = 1; x < OLGMD1_WIDTH - 1; ++x) {
      const int i = y * OLGMD1_WIDTH + x;
      uint32_t s_on = 0;
      uint32_t s_off = 0;
      if (on_active) {
        const uint32_t inhibited = (on_weight_q8 * inhibition_q8(
            on_current, on_previous, i, ON_SELF_ALPHA_Q8,
            ON_ADJ_ALPHA_Q8, ON_DIAG_ALPHA_Q8) + 32768u) >> 16;
        s_on = on_current[i] > inhibited ? on_current[i] - inhibited : 0u;
      }
      if (off_active) {
        const uint32_t inhibited = (off_weight_q8 * inhibition_q8(
            off_current, off_previous, i, OFF_SELF_ALPHA_Q8,
            OFF_ADJ_ALPHA_Q8, OFF_DIAG_ALPHA_Q8) + 32768u) >> 16;
        /* The published reference accidentally uses ON and W_on here. */
        s_off = off_current[i] > inhibited ? off_current[i] - inhibited : 0u;
      }
      scratch->summation[i] = saturate_u8(s_on + s_off);
    }
  }

  /* Group local excitation Ce, then G = S*Ce/(Delta + max(Ce)/Cw). */
  memset(scratch->work, 0, OLGMD1_PIXELS);
  uint8_t max_ce = 0;
  for (int y = 1; y < OLGMD1_HEIGHT - 1; ++y) {
    for (int x = 1; x < OLGMD1_WIDTH - 1; ++x) {
      const int i = y * OLGMD1_WIDTH + x;
      uint16_t sum = 0;
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          sum += scratch->summation[i + dy * OLGMD1_WIDTH + dx];
        }
      }
      const uint8_t ce = (uint8_t)(sum / 9u);
      scratch->work[i] = ce;
      if (ce > max_ce) max_ce = ce;
    }
  }

  const uint32_t group_divisor_q8 = GROUP_DELTA_Q8 +
      (uint32_t)max_ce * (256u / GROUP_CW);
  uint32_t membrane_potential = 0;
  if (group_divisor_q8 != 0u) {
    for (int i = 0; i < OLGMD1_PIXELS; ++i) {
      const uint32_t grouped = ((uint32_t)scratch->summation[i] *
          scratch->work[i] * 256u) / group_divisor_q8;
      if (grouped >= config->grouping_threshold) membrane_potential += grouped;
    }
  }

  const uint16_t sigmoid = sigmoid_from_mp(membrane_potential);
  const int32_t difference = (int32_t)sigmoid - state->previous_sigmoid_q15;
  int32_t membrane;
  if (difference <= SFA_THRESHOLD_Q15) {
    membrane = ((int32_t)SFA_ALPHA_Q8 *
        ((int32_t)state->membrane_q15 + difference) + 128) >> 8;
  } else {
    membrane = ((int32_t)SFA_ALPHA_Q8 * sigmoid + 128) >> 8;
  }
  if (membrane < 0) membrane = 0;
  if (membrane > 32767) membrane = 32767;
  state->membrane_q15 = (uint16_t)membrane;
  state->previous_sigmoid_q15 = sigmoid;

  const uint8_t window = config->collision_window > 8u
      ? 8u : config->collision_window;
  const uint8_t spike_count = paper_spike_count_q15(
      state->membrane_q15, config->spike_threshold_q15);
  for (int i = 7; i > 0; --i) {
    state->spike_history[i] = state->spike_history[i - 1];
  }
  state->spike_history[0] = spike_count;
  uint16_t accumulated_spikes = 0u;
  for (uint8_t i = 0; i < window; ++i) {
    accumulated_spikes += state->spike_history[i];
  }

  state->current_plane = (uint8_t)current_plane;
  result.valid = true;
  result.membrane_q15 = state->membrane_q15;
  result.ffi_on_q8 = state->ffi_on_q8;
  result.ffi_off_q8 = state->ffi_off_q8;
  result.active_polarities = (uint8_t)((on_active ? 1u : 0u) |
      (off_active ? 2u : 0u));
  result.spike_count = spike_count;
  result.threat = window != 0u &&
      accumulated_spikes >= config->collision_spikes;
  return result;
}
