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

#ifndef __GAP8_NETWORK_H__
#define __GAP8_NETWORK_H__

#include <stddef.h>
#include "pmsis.h"


struct gap8_network_run_token {
  struct pi_device cluster_dev;
};


void gap8_network_terminate();
void gap8_network_initialize();
void gap8_network_run_cluster(void * args);
struct gap8_network_run_token gap8_network_run_async(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir);
void gap8_network_run_wait(struct gap8_network_run_token token);
void gap8_network_run(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir);
void gap8_network_run_async_cl(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir, pi_device_t *cluster, pi_task_t *network_done);

void gap8_execute_layer_fork(void *arg);


#ifdef DEFINE_CONSTANTS
// allocation of buffers with parameters needed by the network execution
static const char * L3_weights_files[] = {
  "gap8_ReluConvolution0_weights.hex", "gap8_ReluConvolution1_weights.hex", "gap8_ReluConvolution2_weights.hex", "gap8_ReluConvolution3_weights.hex", "gap8_ReluConvolution4_weights.hex", "gap8_ReluConvolution6_weights.hex"
};
static int L3_weights_size[6];
static int layers_pointers[7];
static char * Layers_name[7] = {"gap8_ReluConvolution0", "gap8_ReluConvolution1", "gap8_ReluConvolution2", "gap8_ReluConvolution3", "gap8_ReluConvolution4", "gap8_ReluPooling5", "gap8_ReluConvolution6"};
static int L3_input_layers[7] = {1,
0, 0, 0, 0, 0, 0};
static int L3_output_layers[7] = {0, 0, 0, 0, 0, 0, 0};
static int allocate_layer[7] = {1, 1, 1, 1, 1, 0, 1};
static int branch_input[7] = {0, 0, 0, 0, 0, 0, 0};
static int branch_output[7] = {0, 0, 0, 0, 0, 0, 0};
static int branch_change[7] = {0, 0, 0, 0, 0, 0, 0};
static int weights_checksum[7] = {137, 247, 178, 170, 184, 0, 60};
static int weights_size[7] = {18688, 55680, 83328, 83328, 83328, 0, 500};
static int activations_checksum[7][1] = {{
  200  },
{
  151  },
{
  57  },
{
  38  },
{
  55  },
{
  177  },
{
  144  }
};
static int activations_size[7] = {51200, 25600, 9600, 9600, 9600, 9600, 96};
static int out_mult_vector[7] = {34, 38, 36, 43, 37, 4100, 47};
static int out_shift_vector[7] = {14, 13, 12, 12, 12, 12, 10};
static int activations_out_checksum[7][1] = {{
  146071 },
{
  52025 },
{
  32294 },
{
  30007 },
{
  43953 },
{
  400 },
{
  547 }
};
static int activations_out_size[7] = {25600, 9600, 9600, 9600, 9600, 96, 5};
static int layer_with_weights[7] = {1, 1, 1, 1, 1, 0, 1};
#endif

#endif  // __NETWORK_H__
