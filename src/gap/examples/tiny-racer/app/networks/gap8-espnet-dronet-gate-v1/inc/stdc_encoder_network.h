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
  "stdc_encoder_BNReluConvolution0_weights.hex", "stdc_encoder_BNReluConvolution1_weights.hex", "stdc_encoder_BNReluConvolution2_weights.hex", "stdc_encoder_BNReluConvolution3_weights.hex", "stdc_encoder_BNReluConvolution4_weights.hex", "stdc_encoder_BNReluConvolution5_weights.hex", "stdc_encoder_BNReluConvolution6_weights.hex", "stdc_encoder_BNReluConvolution8_weights.hex", "stdc_encoder_BNReluConvolution9_weights.hex", "stdc_encoder_BNReluConvolution11_weights.hex", "stdc_encoder_BNReluConvolution12_weights.hex", "stdc_encoder_BNReluConvolution13_weights.hex", "stdc_encoder_BNReluConvolution14_weights.hex", "stdc_encoder_BNReluConvolution16_weights.hex", "stdc_encoder_BNReluConvolution17_weights.hex", "stdc_encoder_BNReluConvolution19_weights.hex", "stdc_encoder_BNReluConvolution20_weights.hex"
};
static int L3_weights_size[17];
static int layers_pointers[22];
static char * Layers_name[22] = {"stdc_encoder_BNReluConvolution0", "stdc_encoder_BNReluConvolution1", "stdc_encoder_BNReluConvolution2", "stdc_encoder_BNReluConvolution3", "stdc_encoder_BNReluConvolution4", "stdc_encoder_BNReluConvolution5", "stdc_encoder_BNReluConvolution6", "stdc_encoder_ReluQAddition7", "stdc_encoder_BNReluConvolution8", "stdc_encoder_BNReluConvolution9", "stdc_encoder_ReluQAddition10", "stdc_encoder_BNReluConvolution11", "stdc_encoder_BNReluConvolution12", "stdc_encoder_BNReluConvolution13", "stdc_encoder_BNReluConvolution14", "stdc_encoder_ReluQAddition15", "stdc_encoder_BNReluConvolution16", "stdc_encoder_BNReluConvolution17", "stdc_encoder_ReluQAddition18", "stdc_encoder_BNReluConvolution19", "stdc_encoder_BNReluConvolution20", "stdc_encoder_ReluQAddition21"};
static int L3_input_layers[22] = {1,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int L3_output_layers[22] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int allocate_layer[22] = {1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0};
static int branch_input[22] = {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1};
static int branch_output[22] = {0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0};
static int branch_change[22] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int weights_checksum[22] = {199, 18, 195, 106, 4, 187, 238, 0, 81, 190, 0, 31, 202, 144, 192, 0, 202, 147, 0, 219, 174, 0};
static int weights_size[22] = {544, 400, 512, 400, 1024, 800, 1536, 0, 800, 1536, 0, 800, 3072, 1600, 5120, 0, 1600, 5120, 0, 1600, 5120, 0};
static int activations_checksum[22][1] = {{
  199  },
{
  117  },
{
  151  },
{
  20  },
{
  93  },
{
  132  },
{
  117  },
{
  219  },
{
  51  },
{
  162  },
{
  230  },
{
  33  },
{
  209  },
{
  146  },
{
  23  },
{
  90  },
{
  81  },
{
  137  },
{
  158  },
{
  27  },
{
  202  },
{
  170  }
};
static int activations_size[22] = {51200, 102400, 102400, 102400, 25600, 51200, 51200, 51200, 51200, 51200, 51200, 51200, 12800, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600};
static int out_mult_vector[22] = {1, 1, 1, 1, 1, 1, 1, 47, 1, 1, 37, 1, 1, 1, 1, 60, 1, 1, 55, 1, 1, 42};
static int out_shift_vector[22] = {23, 20, 18, 22, 23, 21, 22, 6, 22, 22, 6, 22, 23, 21, 23, 6, 17, 24, 6, 22, 24, 6};
static int activations_out_checksum[22][1] = {{
  1103733 },
{
  411287 },
{
  455444 },
{
  166749 },
{
  298884 },
{
  221045 },
{
  390107 },
{
  486451 },
{
  186530 },
{
  329702 },
{
  450337 },
{
  103633 },
{
  223378 },
{
  175639 },
{
  201818 },
{
  388689 },
{
  166793 },
{
  204702 },
{
  498971 },
{
  233418 },
{
  266154 },
{
  489880 }
};
static int activations_out_size[22] = {102400, 102400, 102400, 25600, 51200, 51200, 51200, 51200, 51200, 51200, 51200, 12800, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600};
static int layer_with_weights[22] = {1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0};
#endif

#endif  // __NETWORK_H__
