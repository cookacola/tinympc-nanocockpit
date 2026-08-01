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
  "gap8_BNReluConvolution0_weights.hex", "gap8_BNReluConvolution1_weights.hex", "gap8_BNReluConvolution2_weights.hex", "gap8_BNReluConvolution3_weights.hex", "gap8_BNReluConvolution4_weights.hex", "gap8_BNReluConvolution5_weights.hex", "gap8_BNReluConvolution6_weights.hex", "gap8_BNReluConvolution7_weights.hex", "gap8_BNReluConvolution8_weights.hex", "gap8_BNReluConvolution9_weights.hex", "gap8_BNReluConvolution10_weights.hex", "gap8_BNReluConvolution11_weights.hex", "gap8_BNReluConvolution12_weights.hex", "gap8_BNReluConvolution13_weights.hex", "gap8_BNReluConvolution14_weights.hex", "gap8_BNReluConvolution15_weights.hex", "gap8_BNReluConvolution16_weights.hex", "gap8_BNReluConvolution17_weights.hex", "gap8_BNReluConvolution18_weights.hex", "gap8_BNReluConvolution19_weights.hex", "gap8_BNReluConvolution20_weights.hex", "gap8_BNReluConvolution21_weights.hex", "gap8_BNReluConvolution22_weights.hex", "gap8_BNReluConvolution23_weights.hex"
};
static int L3_weights_size[24];
static int layers_pointers[24];
static char * Layers_name[24] = {"gap8_BNReluConvolution0", "gap8_BNReluConvolution1", "gap8_BNReluConvolution2", "gap8_BNReluConvolution3", "gap8_BNReluConvolution4", "gap8_BNReluConvolution5", "gap8_BNReluConvolution6", "gap8_BNReluConvolution7", "gap8_BNReluConvolution8", "gap8_BNReluConvolution9", "gap8_BNReluConvolution10", "gap8_BNReluConvolution11", "gap8_BNReluConvolution12", "gap8_BNReluConvolution13", "gap8_BNReluConvolution14", "gap8_BNReluConvolution15", "gap8_BNReluConvolution16", "gap8_BNReluConvolution17", "gap8_BNReluConvolution18", "gap8_BNReluConvolution19", "gap8_BNReluConvolution20", "gap8_BNReluConvolution21", "gap8_BNReluConvolution22", "gap8_BNReluConvolution23"};
static int L3_input_layers[24] = {1,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int L3_output_layers[24] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int allocate_layer[24] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
static int branch_input[24] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int branch_output[24] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int branch_change[24] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int weights_checksum[24] = {142, 126, 117, 144, 54, 5, 77, 153, 211, 154, 115, 23, 165, 153, 43, 21, 61, 246, 28, 123, 85, 15, 219, 226};
static int weights_size[24] = {272, 272, 576, 408, 1024, 544, 1920, 816, 3584, 1088, 6912, 1632, 9984, 1632, 9984, 1632, 9984, 1632, 9984, 1632, 9984, 1632, 9984, 1248};
static int activations_checksum[24][1] = {{
  206  },
{
  200  },
{
  28  },
{
  67  },
{
  253  },
{
  190  },
{
  12  },
{
  144  },
{
  159  },
{
  251  },
{
  163  },
{
  75  },
{
  18  },
{
  86  },
{
  197  },
{
  56  },
{
  203  },
{
  44  },
{
  217  },
{
  81  },
{
  165  },
{
  44  },
{
  20  },
{
  141  }
};
static int activations_size[24] = {19200, 76800, 76800, 115200, 28800, 38400, 38400, 57600, 14400, 19200, 19200, 28800, 28800, 28800, 28800, 28800, 28800, 28800, 28800, 28800, 28800, 28800, 28800, 28800};
static int out_mult_vector[24] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
static int out_shift_vector[24] = {18, 20, 23, 22, 23, 21, 23, 15, 23, 21, 23, 21, 24, 21, 24, 20, 23, 21, 23, 21, 22, 19, 20, 23};
static int activations_out_checksum[24][1] = {{
  369352 },
{
  985116 },
{
  404803 },
{
  296701 },
{
  204222 },
{
  456972 },
{
  270224 },
{
  139679 },
{
  129787 },
{
  161955 },
{
  222795 },
{
  317202 },
{
  244822 },
{
  263109 },
{
  181560 },
{
  260299 },
{
  190252 },
{
  198105 },
{
  122961 },
{
  119717 },
{
  74028 },
{
  101908 },
{
  131981 },
{
  186280 }
};
static int activations_out_size[24] = {76800, 76800, 115200, 28800, 38400, 38400, 57600, 14400, 19200, 19200, 28800, 28800, 28800, 28800, 28800, 28800, 28800, 28800, 28800, 28800, 28800, 28800, 28800, 3600};
static int layer_with_weights[24] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
#endif

#endif  // __NETWORK_H__
