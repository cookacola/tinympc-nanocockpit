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

#ifndef __STDC_GATE_HEAD_NETWORK_H__
#define __STDC_GATE_HEAD_NETWORK_H__

#include <stddef.h>
#include "pmsis.h"


struct stdc_gate_head_network_run_token {
  struct pi_device cluster_dev;
};


void stdc_gate_head_network_terminate();
void stdc_gate_head_network_initialize();
void stdc_gate_head_network_run_cluster(void * args);
struct stdc_gate_head_network_run_token stdc_gate_head_network_run_async(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir);
void stdc_gate_head_network_run_wait(struct stdc_gate_head_network_run_token token);
void stdc_gate_head_network_run(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir);
void stdc_gate_head_network_run_async_cl(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir, pi_device_t *cluster, pi_task_t *network_done);

void stdc_gate_head_execute_layer_fork(void *arg);


#ifdef DEFINE_CONSTANTS
// allocation of buffers with parameters needed by the network execution
static const char * L3_weights_files[] = {
  "stdc_gate_head_BNReluConvolution0_weights.hex", "stdc_gate_head_BNReluConvolution1_weights.hex", "stdc_gate_head_BNReluConvolution2_weights.hex", "stdc_gate_head_BNReluConvolution3_weights.hex"
};
static int L3_weights_size[4];
static int layers_pointers[4];
static char * Layers_name[4] = {"stdc_gate_head_BNReluConvolution0", "stdc_gate_head_BNReluConvolution1", "stdc_gate_head_BNReluConvolution2", "stdc_gate_head_BNReluConvolution3"};
static int L3_input_layers[4] = {1,
0, 0, 0};
static int L3_output_layers[4] = {0, 0, 0, 0};
static int allocate_layer[4] = {1, 1, 1, 1};
static int branch_input[4] = {0, 0, 0, 0};
static int branch_output[4] = {0, 0, 0, 0};
static int branch_change[4] = {0, 0, 0, 0};
static int weights_checksum[4] = {41, 242, 59, 109};
static int weights_size[4] = {2560, 800, 768, 32};
static int activations_checksum[4][1] = {{
  152  },
{
  124  },
{
  87  },
{
  27  }
};
static int activations_size[4] = {25600, 12800, 12800, 6400};
static int out_mult_vector[4] = {1, 1, 1, 1};
static int out_shift_vector[4] = {23, 21, 22, 24};
static int activations_out_checksum[4][1] = {{
  215420 },
{
  35927 },
{
  79899 },
{
  80068 }
};
static int activations_out_size[4] = {12800, 12800, 6400, 400};
static int layer_with_weights[4] = {1, 1, 1, 1};
#endif

#endif  // __NETWORK_H__
