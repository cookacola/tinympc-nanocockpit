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

#ifndef __STDC_ENCODER_NETWORK_H__
#define __STDC_ENCODER_NETWORK_H__

#include <stddef.h>
#include "pmsis.h"


struct stdc_encoder_network_run_token {
  struct pi_device cluster_dev;
};


void stdc_encoder_network_terminate();
void stdc_encoder_network_initialize();
void stdc_encoder_network_run_cluster(void * args);
struct stdc_encoder_network_run_token stdc_encoder_network_run_async(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir);
void stdc_encoder_network_run_wait(struct stdc_encoder_network_run_token token);
void stdc_encoder_network_run(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir);
void stdc_encoder_network_run_async_cl(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir, pi_device_t *cluster, pi_task_t *network_done);

void stdc_encoder_execute_layer_fork(void *arg);


#ifdef DEFINE_CONSTANTS
// allocation of buffers with parameters needed by the network execution
static const char * L3_weights_files[] = {
  "stdc_encoder_BNReluConvolution0_weights.hex", "stdc_encoder_BNReluConvolution1_weights.hex", "stdc_encoder_BNReluConvolution2_weights.hex", "stdc_encoder_BNReluConvolution3_weights.hex", "stdc_encoder_BNReluConvolution4_weights.hex", "stdc_encoder_BNReluConvolution5_weights.hex", "stdc_encoder_BNReluConvolution6_weights.hex", "stdc_encoder_BNReluConvolution8_weights.hex", "stdc_encoder_BNReluConvolution9_weights.hex"
};
static int L3_weights_size[9];
static int layers_pointers[11];
static char * Layers_name[11] = {"stdc_encoder_BNReluConvolution0", "stdc_encoder_BNReluConvolution1", "stdc_encoder_BNReluConvolution2", "stdc_encoder_BNReluConvolution3", "stdc_encoder_BNReluConvolution4", "stdc_encoder_BNReluConvolution5", "stdc_encoder_BNReluConvolution6", "stdc_encoder_ReluQAddition7", "stdc_encoder_BNReluConvolution8", "stdc_encoder_BNReluConvolution9", "stdc_encoder_ReluQAddition10"};
static int L3_input_layers[11] = {1,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int L3_output_layers[11] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int allocate_layer[11] = {1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0};
static int branch_input[11] = {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1};
static int branch_output[11] = {0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0};
static int branch_change[11] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int weights_checksum[11] = {10, 77, 15, 131, 149, 10, 41, 0, 11, 204, 0};
static int weights_size[11] = {272, 272, 384, 272, 768, 544, 1280, 0, 544, 1280, 0};
static int activations_checksum[11][1] = {{
  139  },
{
  168  },
{
  62  },
{
  170  },
{
  96  },
{
  156  },
{
  195  },
{
  150  },
{
  248  },
{
  192  },
{
  41  }
};
static int activations_size[11] = {19200, 76800, 76800, 76800, 19200, 38400, 38400, 38400, 38400, 38400, 38400};
static int out_mult_vector[11] = {1, 1, 1, 1, 1, 1, 1, 58, 1, 1, 52};
static int out_shift_vector[11] = {23, 20, 23, 22, 22, 21, 23, 6, 22, 23, 6};
static int activations_out_checksum[11][1] = {{
  548008 },
{
  805182 },
{
  302506 },
{
  287328 },
{
  291228 },
{
  214723 },
{
  218774 },
{
  292088 },
{
  313536 },
{
  328745 },
{
  430230 }
};
static int activations_out_size[11] = {76800, 76800, 76800, 19200, 38400, 38400, 38400, 38400, 38400, 38400, 38400};
static int layer_with_weights[11] = {1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0};
#endif

#endif  // __NETWORK_H__
