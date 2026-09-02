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

#ifndef __STDC_CORNER_HEAD_NETWORK_H__
#define __STDC_CORNER_HEAD_NETWORK_H__

#include <stddef.h>
#include "pmsis.h"


struct stdc_corner_head_network_run_token {
  struct pi_device cluster_dev;
};


void stdc_corner_head_network_terminate();
void stdc_corner_head_network_initialize();
void stdc_corner_head_network_run_cluster(void * args);
struct stdc_corner_head_network_run_token stdc_corner_head_network_run_async(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir);
void stdc_corner_head_network_run_wait(struct stdc_corner_head_network_run_token token);
void stdc_corner_head_network_run(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir);
void stdc_corner_head_network_run_async_cl(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir, pi_device_t *cluster, pi_task_t *network_done);

void stdc_corner_head_execute_layer_fork(void *arg);


#ifdef DEFINE_CONSTANTS
// allocation of buffers with parameters needed by the network execution
static const char * L3_weights_files[] = {
  "stdc_corner_head_BNReluConvolution0_weights.hex", "stdc_corner_head_BNReluConvolution1_weights.hex", "stdc_corner_head_BNReluConvolution2_weights.hex"
};
static int L3_weights_size[3];
static int layers_pointers[3];
static char * Layers_name[3] = {"stdc_corner_head_BNReluConvolution0", "stdc_corner_head_BNReluConvolution1", "stdc_corner_head_BNReluConvolution2"};
static int L3_input_layers[3] = {1,
0, 0};
static int L3_output_layers[3] = {0, 0, 0};
static int allocate_layer[3] = {1, 1, 1};
static int branch_input[3] = {0, 0, 0};
static int branch_output[3] = {0, 0, 0};
static int branch_change[3] = {0, 0, 0};
static int weights_checksum[3] = {130, 105, 176};
static int weights_size[3] = {544, 640, 96};
static int activations_checksum[3][1] = {{
  18  },
{
  103  },
{
  50  }
};
static int activations_size[3] = {38400, 38400, 19200};
static int out_mult_vector[3] = {1, 1, 1};
static int out_shift_vector[3] = {23, 22, 24};
static int activations_out_checksum[3][1] = {{
  311143 },
{
  320050 },
{
  381011 }
};
static int activations_out_size[3] = {38400, 19200, 4800};
static int layer_with_weights[3] = {1, 1, 1};
#endif

#endif  // __NETWORK_H__
