/*
 * network.h
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

#ifndef __STDC_PRESENCE_HEAD_NETWORK_H__
#define __STDC_PRESENCE_HEAD_NETWORK_H__

#include <stddef.h>
#include "pmsis.h"


struct stdc_presence_head_network_run_token {
  struct pi_device cluster_dev;
};


void stdc_presence_head_network_terminate();
void stdc_presence_head_network_initialize();
void stdc_presence_head_network_run_cluster(void * args);
struct stdc_presence_head_network_run_token stdc_presence_head_network_run_async(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir);
void stdc_presence_head_network_run_wait(struct stdc_presence_head_network_run_token token);
void stdc_presence_head_network_run(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir);
void stdc_presence_head_network_run_async_cl(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir, pi_device_t *cluster, pi_task_t *network_done);

void stdc_presence_head_execute_layer_fork(void *arg);


#ifdef DEFINE_CONSTANTS
// allocation of buffers with parameters needed by the network execution
static const char * L3_weights_files[] = {
  "stdc_presence_head_BNReluConvolution0_weights.hex"
};
static int L3_weights_size[1];
static int layers_pointers[2];
static char * Layers_name[2] = {"stdc_presence_head_BNReluConvolution0", "stdc_presence_head_ReluPooling1"};
static int L3_input_layers[2] = {1,
0};
static int L3_output_layers[2] = {0, 0};
static int allocate_layer[2] = {1, 0};
static int branch_input[2] = {0, 0};
static int branch_output[2] = {0, 0};
static int branch_change[2] = {0, 0};
static int weights_checksum[2] = {132, 0};
static int weights_size[2] = {80, 0};
static int activations_checksum[2][1] = {{
  152  },
{
  181  }
};
static int activations_size[2] = {25600, 400};
static int out_mult_vector[2] = {1, 16400};
static int out_shift_vector[2] = {24, 14};
static int activations_out_checksum[2][1] = {{
  17845 },
{
  44 }
};
static int activations_out_size[2] = {400, 1};
static int layer_with_weights[2] = {1, 0};
#endif

#endif  // __NETWORK_H__
