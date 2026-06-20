/*
 * test_template.c
 * Alessio Burrello <alessio.burrello@unibo.it>
 *
 * Copyright (C) 2019-2020 University of Bologna
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. 
 */
#include "mem.h"
#include "network.h"

#include "pmsis.h"

#define VERBOSE 1

// NOTE: the DORY template ships a PMU_set_voltage() stub here for non-GAP8
// targets. On the GAP8 ai_deck SDK config the real one lives in librt.a, so
// the stub collides (multiple definition). Use the SDK's directly.


void application(void * arg) {
/*
    Opening of Filesystem and Ram
*/
  mem_init();
  network_initialize();
  /*
    Allocating space for input
  */
  void *l2_buffer = pi_l2_malloc(380000);
  if (NULL == l2_buffer) {
#ifdef VERBOSE
    printf("ERROR: L2 buffer allocation failed.");
#endif
    pmsis_exit(-1);
  }
#ifdef VERBOSE
  printf("\nL2 Buffer alloc initial\t@ 0x%08x:\tOk\n", (unsigned int)l2_buffer);
#endif
  size_t l2_input_size = 15360;
  size_t input_size = 1000000;
  int initial_dir = 1;

  void *ram_input = ram_malloc(input_size);
      load_file_to_ram(ram_input, "inputs.hex");
      ram_read(l2_buffer, ram_input, l2_input_size);
      network_run(l2_buffer, 380000, l2_buffer, 0, initial_dir);

  // gate8 golden check: network_run copied the 32B (8xint32) final FC output
  // into l2_final_output (== l2_buffer). Print it. Order: TL,TR,BR,BL.
  // Expected (== NEMO golden out_layer8): 299980 133586 326367 398733 697466 359231 689251 145986
  int32_t *gate_out = (int32_t *) l2_buffer;
  printf("GATE8_OUT:");
  for (int i = 0; i < 8; i++) printf(" %d", (int) gate_out[i]);
  printf("\n");

  ram_free(ram_input, input_size);
  network_terminate();
  pi_l2_free(l2_buffer, 380000);
}

int main () {
#ifndef TARGET_CHIP_FAMILY_GAP9
  PMU_set_voltage(1000, 0);
#else
  pi_pmu_voltage_set(PI_PMU_VOLTAGE_DOMAIN_CHIP, PI_PMU_VOLT_800);
#endif
  pi_time_wait_us(10000);
  pi_freq_set(PI_FREQ_DOMAIN_FC, 100000000);
  pi_time_wait_us(10000);
  pi_freq_set(PI_FREQ_DOMAIN_CL, 100000000);
  pi_time_wait_us(10000);

  #if __PLATFORM__ == ARCHI_PLATFORM_FPGA
    *(int*)(ICACHE_PREFETCH) = 0xFFFF;
  #endif

  pmsis_kickoff((void*)application);
  return 0;
}
