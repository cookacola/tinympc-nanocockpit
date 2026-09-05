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
  "gap8_BNReluConvolution0_weights.hex", "gap8_BNReluConvolution1_weights.hex", "gap8_BNReluConvolution2_weights.hex", "gap8_BNReluConvolution3_weights.hex", "gap8_BNReluConvolution4_weights.hex", "gap8_BNReluConvolution5_weights.hex", "gap8_BNReluConvolution6_weights.hex", "gap8_BNReluConvolution8_weights.hex", "gap8_BNReluConvolution9_weights.hex"
};
static int L3_weights_size[9];
static int layers_pointers[11];
static char * Layers_name[11] = {"gap8_BNReluConvolution0", "gap8_BNReluConvolution1", "gap8_BNReluConvolution2", "gap8_BNReluConvolution3", "gap8_BNReluConvolution4", "gap8_BNReluConvolution5", "gap8_BNReluConvolution6", "gap8_ReluQAddition7", "gap8_BNReluConvolution8", "gap8_BNReluConvolution9", "gap8_ReluQAddition10"};
static int L3_input_layers[11] = {1,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int L3_output_layers[11] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int allocate_layer[11] = {1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0};
static int branch_input[11] = {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1};
static int branch_output[11] = {0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0};
static int branch_change[11] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int weights_checksum[11] = {14, 175, 239, 145, 194, 38, 61, 0, 135, 181, 0};
static int weights_size[11] = {688, 400, 512, 400, 1024, 800, 1536, 0, 800, 1536, 0};
static int activations_checksum[11][1] = {{
  194  },
{
  62  },
{
  158  },
{
  92  },
{
  47  },
{
  236  },
{
  136  },
{
  250  },
{
  39  },
{
  125  },
{
  204  }
};
static int activations_size[11] = {76800, 102400, 102400, 102400, 25600, 51200, 51200, 51200, 51200, 51200, 51200};
static int out_mult_vector[11] = {1, 1, 1, 1, 1, 1, 1, 55, 1, 1, 53};
static int out_shift_vector[11] = {24, 22, 22, 22, 22, 21, 23, 6, 22, 23, 6};
static int activations_out_checksum[11][1] = {{
  1056574 },
{
  854174 },
{
  1352796 },
{
  376111 },
{
  1319148 },
{
  1005960 },
{
  1512698 },
{
  1838119 },
{
  902013 },
{
  1026252 },
{
  1579208 }
};
static int activations_out_size[11] = {102400, 102400, 102400, 25600, 51200, 51200, 51200, 51200, 51200, 51200, 51200};
static int layer_with_weights[11] = {1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0};
#endif

#endif  // __NETWORK_H__
