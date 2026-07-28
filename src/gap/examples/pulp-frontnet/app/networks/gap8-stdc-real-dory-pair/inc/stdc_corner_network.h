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

#ifndef __STDC_CORNER_NETWORK_H__
#define __STDC_CORNER_NETWORK_H__

#include <stddef.h>
#include "pmsis.h"


struct stdc_corner_network_run_token {
  struct pi_device cluster_dev;
};


void stdc_corner_network_terminate();
void stdc_corner_network_initialize();
void stdc_corner_network_run_cluster(void * args);
struct stdc_corner_network_run_token stdc_corner_network_run_async(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir);
void stdc_corner_network_run_wait(struct stdc_corner_network_run_token token);
void stdc_corner_network_run(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir);
void stdc_corner_network_run_async_cl(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir, pi_device_t *cluster, pi_task_t *network_done);

void stdc_corner_execute_layer_fork(void *arg);


#ifdef DEFINE_CONSTANTS
// allocation of buffers with parameters needed by the network execution
static const char * L3_weights_files[] = {
  "stdc_corner_BNReluConvolution0_weights.hex", "stdc_corner_BNReluConvolution1_weights.hex", "stdc_corner_BNReluConvolution2_weights.hex", "stdc_corner_BNReluConvolution3_weights.hex", "stdc_corner_BNReluConvolution4_weights.hex", "stdc_corner_BNReluConvolution5_weights.hex", "stdc_corner_BNReluConvolution6_weights.hex", "stdc_corner_BNReluConvolution8_weights.hex", "stdc_corner_BNReluConvolution9_weights.hex", "stdc_corner_BNReluConvolution11_weights.hex", "stdc_corner_BNReluConvolution12_weights.hex", "stdc_corner_BNReluConvolution13_weights.hex"
};
static int L3_weights_size[12];
static int layers_pointers[14];
static char * Layers_name[14] = {"stdc_corner_BNReluConvolution0", "stdc_corner_BNReluConvolution1", "stdc_corner_BNReluConvolution2", "stdc_corner_BNReluConvolution3", "stdc_corner_BNReluConvolution4", "stdc_corner_BNReluConvolution5", "stdc_corner_BNReluConvolution6", "stdc_corner_ReluQAddition7", "stdc_corner_BNReluConvolution8", "stdc_corner_BNReluConvolution9", "stdc_corner_ReluQAddition10", "stdc_corner_BNReluConvolution11", "stdc_corner_BNReluConvolution12", "stdc_corner_BNReluConvolution13"};
static int L3_input_layers[14] = {1,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int L3_output_layers[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int allocate_layer[14] = {1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1};
static int branch_input[14] = {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0};
static int branch_output[14] = {0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0};
static int branch_change[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int weights_checksum[14] = {4, 106, 31, 64, 144, 4, 53, 0, 13, 202, 0, 3, 107, 231};
static int weights_size[14] = {272, 272, 384, 272, 768, 544, 1280, 0, 544, 1280, 0, 544, 640, 96};
static int activations_checksum[14][1] = {{
  139  },
{
  168  },
{
  62  },
{
  81  },
{
  152  },
{
  98  },
{
  160  },
{
  201  },
{
  192  },
{
  207  },
{
  45  },
{
  18  },
{
  120  },
{
  200  }
};
static int activations_size[14] = {19200, 76800, 76800, 76800, 19200, 38400, 38400, 38400, 38400, 38400, 38400, 38400, 38400, 19200};
static int out_mult_vector[14] = {1, 1, 1, 1, 1, 1, 1, 58, 1, 1, 52, 1, 1, 1};
static int out_shift_vector[14] = {23, 20, 23, 22, 22, 21, 23, 6, 22, 23, 6, 23, 22, 24};
static int activations_out_checksum[14][1] = {{
  548008 },
{
  805182 },
{
  303185 },
{
  285848 },
{
  292194 },
{
  215200 },
{
  220617 },
{
  294080 },
{
  313295 },
{
  329773 },
{
  432402 },
{
  302712 },
{
  315080 },
{
  393855 }
};
static int activations_out_size[14] = {76800, 76800, 76800, 19200, 38400, 38400, 38400, 38400, 38400, 38400, 38400, 38400, 19200, 4800};
static int layer_with_weights[14] = {1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1};
#endif

#endif  // __NETWORK_H__
