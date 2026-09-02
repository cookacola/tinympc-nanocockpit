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

#ifndef __STDC_DANGER_HEAD_NETWORK_H__
#define __STDC_DANGER_HEAD_NETWORK_H__

#include <stddef.h>
#include "pmsis.h"


struct stdc_danger_head_network_run_token {
  struct pi_device cluster_dev;
};


void stdc_danger_head_network_terminate();
void stdc_danger_head_network_initialize();
void stdc_danger_head_network_run_cluster(void * args);
struct stdc_danger_head_network_run_token stdc_danger_head_network_run_async(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir);
void stdc_danger_head_network_run_wait(struct stdc_danger_head_network_run_token token);
void stdc_danger_head_network_run(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir);
void stdc_danger_head_network_run_async_cl(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir, pi_device_t *cluster, pi_task_t *network_done);

void stdc_danger_head_execute_layer_fork(void *arg);


#ifdef DEFINE_CONSTANTS
// allocation of buffers with parameters needed by the network execution
static const char * L3_weights_files[] = {
  "stdc_danger_head_BNReluConvolution0_weights.hex", "stdc_danger_head_BNReluConvolution1_weights.hex", "stdc_danger_head_BNReluConvolution2_weights.hex", "stdc_danger_head_BNReluConvolution3_weights.hex", "stdc_danger_head_BNReluConvolution5_weights.hex", "stdc_danger_head_BNReluConvolution6_weights.hex", "stdc_danger_head_BNReluConvolution8_weights.hex", "stdc_danger_head_BNReluConvolution9_weights.hex", "stdc_danger_head_BNReluConvolution11_weights.hex", "stdc_danger_head_BNReluConvolution12_weights.hex", "stdc_danger_head_BNReluConvolution13_weights.hex", "stdc_danger_head_BNReluConvolution14_weights.hex", "stdc_danger_head_BNReluConvolution16_weights.hex", "stdc_danger_head_BNReluConvolution17_weights.hex", "stdc_danger_head_BNReluConvolution19_weights.hex", "stdc_danger_head_BNReluConvolution20_weights.hex", "stdc_danger_head_BNReluConvolution22_weights.hex", "stdc_danger_head_BNReluConvolution23_weights.hex", "stdc_danger_head_BNReluConvolution25_weights.hex", "stdc_danger_head_BNReluConvolution26_weights.hex", "stdc_danger_head_BNReluConvolution28_weights.hex", "stdc_danger_head_BNReluConvolution29_weights.hex", "stdc_danger_head_BNReluConvolution31_weights.hex", "stdc_danger_head_BNReluConvolution32_weights.hex", "stdc_danger_head_BNReluConvolution34_weights.hex"
};
static int L3_weights_size[25];
static int layers_pointers[35];
static char * Layers_name[35] = {"stdc_danger_head_BNReluConvolution0", "stdc_danger_head_BNReluConvolution1", "stdc_danger_head_BNReluConvolution2", "stdc_danger_head_BNReluConvolution3", "stdc_danger_head_ReluQAddition4", "stdc_danger_head_BNReluConvolution5", "stdc_danger_head_BNReluConvolution6", "stdc_danger_head_ReluQAddition7", "stdc_danger_head_BNReluConvolution8", "stdc_danger_head_BNReluConvolution9", "stdc_danger_head_ReluQAddition10", "stdc_danger_head_BNReluConvolution11", "stdc_danger_head_BNReluConvolution12", "stdc_danger_head_BNReluConvolution13", "stdc_danger_head_BNReluConvolution14", "stdc_danger_head_ReluQAddition15", "stdc_danger_head_BNReluConvolution16", "stdc_danger_head_BNReluConvolution17", "stdc_danger_head_ReluQAddition18", "stdc_danger_head_BNReluConvolution19", "stdc_danger_head_BNReluConvolution20", "stdc_danger_head_ReluQAddition21", "stdc_danger_head_BNReluConvolution22", "stdc_danger_head_BNReluConvolution23", "stdc_danger_head_ReluQAddition24", "stdc_danger_head_BNReluConvolution25", "stdc_danger_head_BNReluConvolution26", "stdc_danger_head_ReluQAddition27", "stdc_danger_head_BNReluConvolution28", "stdc_danger_head_BNReluConvolution29", "stdc_danger_head_ReluQAddition30", "stdc_danger_head_BNReluConvolution31", "stdc_danger_head_BNReluConvolution32", "stdc_danger_head_ReluQAddition33", "stdc_danger_head_BNReluConvolution34"};
static int L3_input_layers[35] = {1,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int L3_output_layers[35] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int allocate_layer[35] = {1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1};
static int branch_input[35] = {0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0};
static int branch_output[35] = {0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0};
static int branch_change[35] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int weights_checksum[35] = {138, 91, 109, 0, 0, 227, 14, 0, 206, 135, 0, 29, 230, 101, 97, 0, 196, 214, 0, 250, 32, 0, 64, 25, 0, 29, 207, 0, 137, 158, 0, 251, 186, 0, 212};
static int weights_size[35] = {544, 2560, 1088, 4608, 0, 1088, 4608, 0, 1088, 4608, 0, 1088, 6912, 1632, 9984, 0, 1632, 9984, 0, 1632, 9984, 0, 1632, 9984, 0, 1632, 9984, 0, 1632, 9984, 0, 1632, 9984, 0, 104};
static int activations_checksum[35][1] = {{
  150  },
{
  196  },
{
  68  },
{
  86  },
{
  117  },
{
  132  },
{
  169  },
{
  233  },
{
  144  },
{
  65  },
{
  166  },
{
  80  },
{
  165  },
{
  61  },
{
  57  },
{
  233  },
{
  39  },
{
  35  },
{
  193  },
{
  213  },
{
  238  },
{
  66  },
{
  1  },
{
  151  },
{
  218  },
{
  200  },
{
  140  },
{
  149  },
{
  23  },
{
  147  },
{
  157  },
{
  155  },
{
  182  },
{
  82  },
{
  181  }
};
static int activations_size[35] = {38400, 9600, 19200, 19200, 19200, 19200, 19200, 19200, 19200, 19200, 19200, 19200, 5120, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680};
static int out_mult_vector[35] = {1, 1, 1, 1, 56, 1, 1, 47, 1, 1, 47, 1, 1, 1, 1, 55, 1, 1, 43, 1, 1, 40, 1, 1, 58, 1, 1, 49, 1, 1, 51, 1, 1, 42, 1};
static int out_shift_vector[35] = {24, 22, 21, 23, 6, 22, 23, 6, 22, 23, 6, 23, 24, 21, 23, 6, 22, 23, 6, 22, 24, 6, 22, 23, 6, 22, 23, 6, 21, 23, 6, 22, 22, 6, 24};
static int activations_out_checksum[35][1] = {{
  80324 },
{
  155972 },
{
  114262 },
{
  169333 },
{
  240516 },
{
  122537 },
{
  230633 },
{
  327312 },
{
  136513 },
{
  198566 },
{
  368464 },
{
  44453 },
{
  90941 },
{
  74041 },
{
  81129 },
{
  127015 },
{
  78115 },
{
  84417 },
{
  134357 },
{
  87278 },
{
  64578 },
{
  109057 },
{
  72343 },
{
  77274 },
{
  144072 },
{
  53900 },
{
  62101 },
{
  132631 },
{
  39827 },
{
  58269 },
{
  140443 },
{
  68022 },
{
  92498 },
{
  140981 },
{
  2613 }
};
static int activations_out_size[35] = {9600, 19200, 19200, 19200, 19200, 19200, 19200, 19200, 19200, 19200, 19200, 5120, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 80};
static int layer_with_weights[35] = {1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1};
#endif

#endif  // __NETWORK_H__
