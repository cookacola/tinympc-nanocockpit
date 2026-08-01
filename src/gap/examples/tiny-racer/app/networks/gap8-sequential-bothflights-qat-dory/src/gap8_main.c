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
#include "gap8_network.h"

#include "pmsis.h"




void application(void * arg) {
/*
    Opening of Filesystem and Ram
*/
  mem_init();
  gap8_network_initialize();
  /*
    Allocating space for input
  */
  void *l2_buffer = pi_l2_malloc(180000);
  if (NULL == l2_buffer) {
#ifdef VERBOSE
    printf("ERROR: L2 buffer allocation failed.");
#endif
    pmsis_exit(-1);
  }
#ifdef VERBOSE
  printf("\nL2 Buffer alloc initial\t@ 0x%08x:\tOk\n", (unsigned int)l2_buffer);
#endif
  size_t l2_input_size = 19200;
  size_t input_size = 131072;
  int initial_dir = 1;

#ifdef DORY_CHECKSUM_HARNESS
  extern const uint8_t gap8_checksum_input[];
  for (size_t i = 0; i < l2_input_size; ++i)
    ((uint8_t *)l2_buffer)[i] = gap8_checksum_input[i];
#else
  void *ram_input = ram_malloc(input_size);
  load_file_to_ram(ram_input, "gap8_inputs.hex");
  ram_read(l2_buffer, ram_input, l2_input_size);
#endif
  gap8_network_run(l2_buffer, 180000, l2_buffer, 0, initial_dir);
#ifndef DORY_CHECKSUM_HARNESS
  ram_free(ram_input, input_size);
#endif
  gap8_network_terminate();
  pi_l2_free(l2_buffer, 180000);
  pmsis_exit(0);
}

int main () {
#ifndef DORY_CHECKSUM_HARNESS
#ifndef TARGET_CHIP_FAMILY_GAP9
  PMU_set_voltage(1000, 0);
#else
  pi_pmu_voltage_set(PI_PMU_VOLTAGE_DOMAIN_CHIP, PI_PMU_VOLT_800);
#endif
#ifndef DORY_CHECKSUM_HARNESS
  pi_time_wait_us(10000);
#endif
  pi_freq_set(PI_FREQ_DOMAIN_FC, 100000000);
#ifndef DORY_CHECKSUM_HARNESS
  pi_time_wait_us(10000);
#endif
  pi_freq_set(PI_FREQ_DOMAIN_CL, 100000000);
#ifndef DORY_CHECKSUM_HARNESS
  pi_time_wait_us(10000);
#endif

#endif

  pmsis_kickoff((void*)application);
  return 0;
}
