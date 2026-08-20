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

#ifndef __STDC_NAVIGATION_HEAD_NETWORK_H__
#define __STDC_NAVIGATION_HEAD_NETWORK_H__

#include <stddef.h>
#include "pmsis.h"


struct stdc_navigation_head_network_run_token {
  struct pi_device cluster_dev;
};


void stdc_navigation_head_network_terminate();
void stdc_navigation_head_network_initialize();
void stdc_navigation_head_network_run_cluster(void * args);
struct stdc_navigation_head_network_run_token stdc_navigation_head_network_run_async(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir);
void stdc_navigation_head_network_run_wait(struct stdc_navigation_head_network_run_token token);
void stdc_navigation_head_network_run(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir);
void stdc_navigation_head_network_run_async_cl(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir, pi_device_t *cluster, pi_task_t *network_done);

void stdc_navigation_head_execute_layer_fork(void *arg);


#ifdef DEFINE_CONSTANTS
// allocation of buffers with parameters needed by the network execution
static const char * L3_weights_files[] = {
  "stdc_navigation_head_BNReluConvolution0_weights.hex", "stdc_navigation_head_BNReluConvolution1_weights.hex", "stdc_navigation_head_BNReluConvolution2_weights.hex", "stdc_navigation_head_BNReluConvolution3_weights.hex", "stdc_navigation_head_BNReluConvolution5_weights.hex", "stdc_navigation_head_BNReluConvolution6_weights.hex", "stdc_navigation_head_BNReluConvolution8_weights.hex", "stdc_navigation_head_BNReluConvolution9_weights.hex", "stdc_navigation_head_BNReluConvolution11_weights.hex", "stdc_navigation_head_BNReluConvolution12_weights.hex", "stdc_navigation_head_BNReluConvolution14_weights.hex", "stdc_navigation_head_BNReluConvolution15_weights.hex", "stdc_navigation_head_BNReluConvolution17_weights.hex", "stdc_navigation_head_BNReluConvolution18_weights.hex", "stdc_navigation_head_BNReluConvolution20_weights.hex", "stdc_navigation_head_BNReluConvolution21_weights.hex", "stdc_navigation_head_BNReluConvolution23_weights.hex"
};
static int L3_weights_size[17];
static int layers_pointers[25];
static char * Layers_name[25] = {"stdc_navigation_head_BNReluConvolution0", "stdc_navigation_head_BNReluConvolution1", "stdc_navigation_head_BNReluConvolution2", "stdc_navigation_head_BNReluConvolution3", "stdc_navigation_head_ReluQAddition4", "stdc_navigation_head_BNReluConvolution5", "stdc_navigation_head_BNReluConvolution6", "stdc_navigation_head_ReluQAddition7", "stdc_navigation_head_BNReluConvolution8", "stdc_navigation_head_BNReluConvolution9", "stdc_navigation_head_ReluQAddition10", "stdc_navigation_head_BNReluConvolution11", "stdc_navigation_head_BNReluConvolution12", "stdc_navigation_head_ReluQAddition13", "stdc_navigation_head_BNReluConvolution14", "stdc_navigation_head_BNReluConvolution15", "stdc_navigation_head_ReluQAddition16", "stdc_navigation_head_BNReluConvolution17", "stdc_navigation_head_BNReluConvolution18", "stdc_navigation_head_ReluQAddition19", "stdc_navigation_head_BNReluConvolution20", "stdc_navigation_head_BNReluConvolution21", "stdc_navigation_head_ReluQAddition22", "stdc_navigation_head_BNReluConvolution23", "stdc_navigation_head_ReluPooling24"};
static int L3_input_layers[25] = {1,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int L3_output_layers[25] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int allocate_layer[25] = {1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0};
static int branch_input[25] = {0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0};
static int branch_output[25] = {0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0};
static int branch_change[25] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int weights_checksum[25] = {210, 242, 200, 188, 0, 217, 134, 0, 46, 154, 0, 119, 195, 0, 30, 158, 0, 202, 41, 0, 77, 22, 0, 25, 0};
static int weights_size[25] = {1600, 7680, 2400, 10752, 0, 2400, 10752, 0, 2400, 10752, 0, 2400, 10752, 0, 2400, 10752, 0, 2400, 10752, 0, 2400, 10752, 0, 224, 0};
static int activations_checksum[25][1] = {{
  152  },
{
  229  },
{
  151  },
{
  157  },
{
  209  },
{
  129  },
{
  142  },
{
  129  },
{
  159  },
{
  50  },
{
  145  },
{
  184  },
{
  89  },
{
  232  },
{
  209  },
{
  114  },
{
  168  },
{
  165  },
{
  28  },
{
  224  },
{
  116  },
{
  61  },
{
  46  },
{
  124  },
{
  34  }
};
static int activations_size[25] = {25600, 6400, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 200};
static int out_mult_vector[25] = {1, 1, 1, 1, 37, 1, 1, 64, 1, 1, 32, 1, 1, 64, 1, 1, 32, 1, 1, 59, 1, 1, 54, 1, 4100};
static int out_shift_vector[25] = {23, 24, 17, 24, 6, 19, 24, 6, 19, 23, 5, 13, 23, 6, 19, 22, 5, 14, 23, 6, 20, 23, 6, 24, 12};
static int activations_out_checksum[25][1] = {{
  79589 },
{
  56983 },
{
  83613 },
{
  32209 },
{
  49537 },
{
  115854 },
{
  15745 },
{
  39583 },
{
  73266 },
{
  16529 },
{
  35256 },
{
  52313 },
{
  46824 },
{
  58065 },
{
  66162 },
{
  73384 },
{
  100517 },
{
  72220 },
{
  477664 },
{
  471412 },
{
  126269 },
{
  254510 },
{
  587132 },
{
  14370 },
{
  142 }
};
static int activations_out_size[25] = {6400, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 200, 2};
static int layer_with_weights[25] = {1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0};
#endif

#endif  // __NETWORK_H__
