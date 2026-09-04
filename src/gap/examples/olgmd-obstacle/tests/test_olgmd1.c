#include "olgmd1.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint8_t frame[OLGMD1_INPUT_WIDTH * OLGMD1_INPUT_HEIGHT];
static olgmd1_state_t state;
static olgmd1_scratch_t scratch;

_Static_assert(sizeof(olgmd1_state_t) == 32018u, "oLGMD1 state size changed");
_Static_assert(sizeof(olgmd1_scratch_t) == 12800u,
               "oLGMD1 scratch size changed");

static void make_square(uint8_t background, uint8_t foreground, int half_size) {
  memset(frame, background, sizeof(frame));
  if (half_size == 0) return;
  const int cx = OLGMD1_INPUT_WIDTH / 2;
  const int cy = OLGMD1_INPUT_HEIGHT / 2;
  for (int y = cy - half_size; y < cy + half_size; ++y) {
    memset(frame + y * OLGMD1_INPUT_WIDTH + cx - half_size,
           foreground, (size_t)(2 * half_size));
  }
}

static void make_shifted_square(uint8_t background, uint8_t foreground,
                                int half_size, int offset_x) {
  memset(frame, background, sizeof(frame));
  const int cx = OLGMD1_INPUT_WIDTH / 2 + offset_x;
  const int cy = OLGMD1_INPUT_HEIGHT / 2;
  for (int y = cy - half_size; y < cy + half_size; ++y) {
    memset(frame + y * OLGMD1_INPUT_WIDTH + cx - half_size,
           foreground, (size_t)(2 * half_size));
  }
}

static olgmd1_result_t step(const olgmd1_config_t *config) {
  return olgmd1_step(&state, &scratch, frame, OLGMD1_INPUT_WIDTH, config);
}

static void test_stationary(void) {
  const olgmd1_config_t config = olgmd1_default_config();
  olgmd1_init(&state);
  make_square(96, 96, 0);
  assert(!step(&config).valid);
  for (int i = 0; i < 12; ++i) {
    const olgmd1_result_t result = step(&config);
    assert(result.valid);
    assert(!result.threat);
    assert(result.ffi_on_q8 == 0);
    assert(result.ffi_off_q8 == 0);
  }
}

static void test_polarity_and_looming(uint8_t background, uint8_t foreground,
                                      uint8_t expected_polarity) {
  olgmd1_config_t config = olgmd1_default_config();
  /* Exercise the paper-style integer spike accumulator. */
  config.spike_threshold_q15 = 8000;
  config.collision_window = 6;
  config.collision_spikes = 6;
  olgmd1_init(&state);
  make_square(background, foreground, 4);
  (void)step(&config);

  bool saw_polarity = false;
  bool saw_threat = false;
  for (int half_size = 8; half_size <= 72; half_size += 4) {
    make_square(background, foreground, half_size);
    const olgmd1_result_t result = step(&config);
    saw_polarity |= (result.active_polarities & expected_polarity) != 0;
    saw_threat |= result.threat;
  }
  assert(saw_polarity);
  assert(saw_threat);
}

static void test_source_defect_corrections(void) {
  test_polarity_and_looming(24, 232, 1u);
  /* A dark loom exercises the corrected OFF state and OFF adaptive weight. */
  test_polarity_and_looming(232, 24, 2u);
}

static void test_nonlooming_sequences(void) {
  const olgmd1_config_t config = olgmd1_default_config();

  /* Translation at constant angular size must not trip the default detector. */
  olgmd1_init(&state);
  make_shifted_square(24, 232, 12, -24);
  assert(!step(&config).valid);
  for (int offset = -20; offset <= 24; offset += 4) {
    make_shifted_square(24, 232, 12, offset);
    assert(!step(&config).threat);
  }

  /*
   * A receding target can retain SFA energy from the initial large target,
   * but the rolling spike sum must clear as the response decays.
   */
  olgmd1_init(&state);
  make_square(24, 232, 60);
  assert(!step(&config).valid);
  olgmd1_result_t receding = {0};
  for (int half_size = 56; half_size >= 8; half_size -= 4) {
    make_square(24, 232, half_size);
    receding = step(&config);
  }
  assert(!receding.threat);

  /* Global exposure steps exercise FFI but cannot meet the spike window. */
  olgmd1_init(&state);
  memset(frame, 40, sizeof(frame));
  assert(!step(&config).valid);
  memset(frame, 200, sizeof(frame));
  assert(!step(&config).threat);
  for (int i = 0; i < 8; ++i) assert(!step(&config).threat);
}

int main(void) {
  test_stationary();
  test_source_defect_corrections();
  test_nonlooming_sequences();
  printf("oLGMD1 host tests passed\n");
  return 0;
}
