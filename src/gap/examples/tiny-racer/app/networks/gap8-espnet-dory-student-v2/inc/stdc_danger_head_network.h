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
static int weights_checksum[35] = {27, 142, 46, 209, 0, 79, 139, 0, 72, 239, 0, 144, 156, 122, 178, 0, 249, 35, 0, 255, 194, 0, 107, 82, 0, 0, 79, 0, 82, 50, 0, 143, 244, 0, 25};
static int weights_size[35] = {800, 3072, 1600, 5120, 0, 1600, 5120, 0, 1600, 5120, 0, 1600, 7680, 2400, 10752, 0, 2400, 10752, 0, 2400, 10752, 0, 2400, 10752, 0, 2400, 10752, 0, 2400, 10752, 0, 2400, 10752, 0, 112};
static int activations_checksum[35][1] = {{
  157  },
{
  15  },
{
  96  },
{
  161  },
{
  229  },
{
  228  },
{
  227  },
{
  35  },
{
  53  },
{
  187  },
{
  21  },
{
  59  },
{
  68  },
{
  234  },
{
  7  },
{
  235  },
{
  131  },
{
  179  },
{
  191  },
{
  170  },
{
  204  },
{
  136  },
{
  139  },
{
  106  },
{
  224  },
{
  93  },
{
  55  },
{
  38  },
{
  113  },
{
  197  },
{
  4  },
{
  206  },
{
  69  },
{
  20  },
{
  204  }
};
static int activations_size[35] = {51200, 12800, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 6400, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600};
static int out_mult_vector[35] = {1, 1, 1, 1, 44, 1, 1, 40, 1, 1, 46, 1, 1, 1, 1, 41, 1, 1, 40, 1, 1, 51, 1, 1, 53, 1, 1, 52, 1, 1, 52, 1, 1, 32, 1};
static int out_shift_vector[35] = {23, 23, 23, 24, 6, 23, 24, 6, 23, 24, 6, 23, 24, 22, 24, 6, 22, 24, 6, 16, 24, 6, 22, 24, 6, 21, 23, 6, 21, 23, 6, 21, 24, 5, 24};
static int activations_out_checksum[35][1] = {{
  181263 },
{
  482656 },
{
  309665 },
{
  423141 },
{
  614116 },
{
  367843 },
{
  448035 },
{
  654645 },
{
  374715 },
{
  450069 },
{
  619067 },
{
  103492 },
{
  201962 },
{
  133127 },
{
  166891 },
{
  232579 },
{
  166579 },
{
  142271 },
{
  157098 },
{
  127436 },
{
  165512 },
{
  185483 },
{
  122218 },
{
  122592 },
{
  198749 },
{
  136759 },
{
  151078 },
{
  186737 },
{
  121797 },
{
  141828 },
{
  146638 },
{
  113989 },
{
  262420 },
{
  209868 },
{
  18860 }
};
static int activations_out_size[35] = {12800, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 6400, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 9600, 100};
static int layer_with_weights[35] = {1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1};
#endif

#endif  // __NETWORK_H__
