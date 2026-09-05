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

#ifndef __ESPNET_GLOBAL_HEAD_NETWORK_H__
#define __ESPNET_GLOBAL_HEAD_NETWORK_H__

#include <stddef.h>
#include "pmsis.h"


struct espnet_global_head_network_run_token {
  struct pi_device cluster_dev;
};


void espnet_global_head_network_terminate();
void espnet_global_head_network_initialize();
void espnet_global_head_network_run_cluster(void * args);
struct espnet_global_head_network_run_token espnet_global_head_network_run_async(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir);
void espnet_global_head_network_run_wait(struct espnet_global_head_network_run_token token);
void espnet_global_head_network_run(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir);
void espnet_global_head_network_run_async_cl(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir, pi_device_t *cluster, pi_task_t *network_done);

void espnet_global_head_execute_layer_fork(void *arg);


#ifdef DEFINE_CONSTANTS
// allocation of buffers with parameters needed by the network execution
static const char * L3_weights_files[] = {
  "espnet_global_head_ReluConvolution0_weights.hex", "espnet_global_head_ReluConvolution1_weights.hex", "espnet_global_head_ReluConvolution2_weights.hex", "espnet_global_head_ReluConvolution3_weights.hex", "espnet_global_head_ReluConvolution4_weights.hex", "espnet_global_head_ReluConvolution6_weights.hex"
};
static int L3_weights_size[6];
static int layers_pointers[7];
static char * Layers_name[7] = {"espnet_global_head_ReluConvolution0", "espnet_global_head_ReluConvolution1", "espnet_global_head_ReluConvolution2", "espnet_global_head_ReluConvolution3", "espnet_global_head_ReluConvolution4", "espnet_global_head_ReluPooling5", "espnet_global_head_ReluConvolution6"};
static int L3_input_layers[7] = {1,
0, 0, 0, 0, 0, 0};
static int L3_output_layers[7] = {0, 0, 0, 0, 0, 0, 0};
static int allocate_layer[7] = {1, 1, 1, 1, 1, 0, 1};
static int branch_input[7] = {0, 0, 0, 0, 0, 0, 0};
static int branch_output[7] = {0, 0, 0, 0, 0, 0, 0};
static int branch_change[7] = {0, 0, 0, 0, 0, 0, 0};
static int weights_checksum[7] = {63, 215, 162, 247, 108, 0, 224};
static int weights_size[7] = {18688, 55680, 83328, 83328, 83328, 0, 300};
static int activations_checksum[7][1] = {{
  17  },
{
  132  },
{
  105  },
{
  254  },
{
  76  },
{
  191  },
{
  233  }
};
static int activations_size[7] = {51200, 25600, 9600, 9600, 9600, 9600, 96};
static int out_mult_vector[7] = {50, 42, 44, 38, 37, 4100, 50};
static int out_shift_vector[7] = {15, 13, 12, 12, 12, 12, 11};
static int activations_out_checksum[7][1] = {{
  203908 },
{
  74857 },
{
  51198 },
{
  36940 },
{
  52927 },
{
  489 },
{
  326 }
};
static int activations_out_size[7] = {25600, 9600, 9600, 9600, 9600, 96, 3};
static int layer_with_weights[7] = {1, 1, 1, 1, 1, 0, 1};
#endif

#endif  // __NETWORK_H__
