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

#ifndef __STDC_DANGER_NETWORK_H__
#define __STDC_DANGER_NETWORK_H__

#include <stddef.h>
#include "pmsis.h"


struct stdc_danger_network_run_token {
  struct pi_device cluster_dev;
};


void stdc_danger_network_terminate();
void stdc_danger_network_initialize();
void stdc_danger_network_run_cluster(void * args);
struct stdc_danger_network_run_token stdc_danger_network_run_async(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir);
void stdc_danger_network_run_wait(struct stdc_danger_network_run_token token);
void stdc_danger_network_run(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir);
void stdc_danger_network_run_async_cl(void *l2_buffer, size_t l2_buffer_size, void *l2_final_output, int exec, int initial_dir, pi_device_t *cluster, pi_task_t *network_done);

void stdc_danger_execute_layer_fork(void *arg);


#ifdef DEFINE_CONSTANTS
// allocation of buffers with parameters needed by the network execution
static const char * L3_weights_files[] = {
  "stdc_danger_BNReluConvolution0_weights.hex", "stdc_danger_BNReluConvolution1_weights.hex", "stdc_danger_BNReluConvolution2_weights.hex", "stdc_danger_BNReluConvolution3_weights.hex", "stdc_danger_BNReluConvolution4_weights.hex", "stdc_danger_BNReluConvolution5_weights.hex", "stdc_danger_BNReluConvolution6_weights.hex", "stdc_danger_BNReluConvolution8_weights.hex", "stdc_danger_BNReluConvolution9_weights.hex", "stdc_danger_BNReluConvolution11_weights.hex", "stdc_danger_BNReluConvolution12_weights.hex", "stdc_danger_BNReluConvolution13_weights.hex", "stdc_danger_BNReluConvolution14_weights.hex", "stdc_danger_BNReluConvolution16_weights.hex", "stdc_danger_BNReluConvolution17_weights.hex", "stdc_danger_BNReluConvolution19_weights.hex", "stdc_danger_BNReluConvolution20_weights.hex", "stdc_danger_BNReluConvolution22_weights.hex", "stdc_danger_BNReluConvolution23_weights.hex", "stdc_danger_BNReluConvolution24_weights.hex", "stdc_danger_BNReluConvolution25_weights.hex", "stdc_danger_BNReluConvolution27_weights.hex", "stdc_danger_BNReluConvolution28_weights.hex", "stdc_danger_BNReluConvolution30_weights.hex", "stdc_danger_BNReluConvolution31_weights.hex", "stdc_danger_BNReluConvolution33_weights.hex", "stdc_danger_BNReluConvolution34_weights.hex", "stdc_danger_BNReluConvolution36_weights.hex", "stdc_danger_BNReluConvolution37_weights.hex", "stdc_danger_BNReluConvolution39_weights.hex", "stdc_danger_BNReluConvolution40_weights.hex", "stdc_danger_BNReluConvolution42_weights.hex", "stdc_danger_BNReluConvolution43_weights.hex", "stdc_danger_BNReluConvolution45_weights.hex"
};
static int L3_weights_size[34];
static int layers_pointers[46];
static char * Layers_name[46] = {"stdc_danger_BNReluConvolution0", "stdc_danger_BNReluConvolution1", "stdc_danger_BNReluConvolution2", "stdc_danger_BNReluConvolution3", "stdc_danger_BNReluConvolution4", "stdc_danger_BNReluConvolution5", "stdc_danger_BNReluConvolution6", "stdc_danger_ReluQAddition7", "stdc_danger_BNReluConvolution8", "stdc_danger_BNReluConvolution9", "stdc_danger_ReluQAddition10", "stdc_danger_BNReluConvolution11", "stdc_danger_BNReluConvolution12", "stdc_danger_BNReluConvolution13", "stdc_danger_BNReluConvolution14", "stdc_danger_ReluQAddition15", "stdc_danger_BNReluConvolution16", "stdc_danger_BNReluConvolution17", "stdc_danger_ReluQAddition18", "stdc_danger_BNReluConvolution19", "stdc_danger_BNReluConvolution20", "stdc_danger_ReluQAddition21", "stdc_danger_BNReluConvolution22", "stdc_danger_BNReluConvolution23", "stdc_danger_BNReluConvolution24", "stdc_danger_BNReluConvolution25", "stdc_danger_ReluQAddition26", "stdc_danger_BNReluConvolution27", "stdc_danger_BNReluConvolution28", "stdc_danger_ReluQAddition29", "stdc_danger_BNReluConvolution30", "stdc_danger_BNReluConvolution31", "stdc_danger_ReluQAddition32", "stdc_danger_BNReluConvolution33", "stdc_danger_BNReluConvolution34", "stdc_danger_ReluQAddition35", "stdc_danger_BNReluConvolution36", "stdc_danger_BNReluConvolution37", "stdc_danger_ReluQAddition38", "stdc_danger_BNReluConvolution39", "stdc_danger_BNReluConvolution40", "stdc_danger_ReluQAddition41", "stdc_danger_BNReluConvolution42", "stdc_danger_BNReluConvolution43", "stdc_danger_ReluQAddition44", "stdc_danger_BNReluConvolution45"};
static int L3_input_layers[46] = {1,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int L3_output_layers[46] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int allocate_layer[46] = {1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1};
static int branch_input[46] = {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0};
static int branch_output[46] = {0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0};
static int branch_change[46] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int weights_checksum[46] = {23, 51, 80, 174, 226, 147, 90, 0, 12, 145, 0, 53, 209, 76, 182, 0, 23, 242, 0, 89, 134, 0, 203, 79, 198, 213, 0, 85, 16, 0, 134, 187, 0, 133, 222, 0, 89, 139, 0, 100, 219, 0, 239, 1, 0, 40};
static int weights_size[46] = {272, 272, 384, 272, 768, 544, 1280, 0, 544, 1280, 0, 544, 2560, 1088, 4608, 0, 1088, 4608, 0, 1088, 4608, 0, 1088, 6912, 1632, 9984, 0, 1632, 9984, 0, 1632, 9984, 0, 1632, 9984, 0, 1632, 9984, 0, 1632, 9984, 0, 1632, 9984, 0, 104};
static int activations_checksum[46][1] = {{
  139  },
{
  12  },
{
  79  },
{
  9  },
{
  193  },
{
  17  },
{
  21  },
{
  132  },
{
  65  },
{
  81  },
{
  240  },
{
  5  },
{
  230  },
{
  140  },
{
  46  },
{
  9  },
{
  155  },
{
  84  },
{
  222  },
{
  112  },
{
  63  },
{
  117  },
{
  57  },
{
  22  },
{
  7  },
{
  5  },
{
  34  },
{
  200  },
{
  137  },
{
  68  },
{
  192  },
{
  198  },
{
  80  },
{
  106  },
{
  83  },
{
  249  },
{
  93  },
{
  38  },
{
  45  },
{
  82  },
{
  208  },
{
  105  },
{
  192  },
{
  101  },
{
  162  },
{
  1  }
};
static int activations_size[46] = {19200, 76800, 76800, 76800, 19200, 38400, 38400, 38400, 38400, 38400, 38400, 38400, 9600, 19200, 19200, 19200, 19200, 19200, 19200, 19200, 19200, 19200, 19200, 5120, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680};
static int out_mult_vector[46] = {1, 1, 1, 1, 1, 1, 1, 59, 1, 1, 49, 1, 1, 1, 1, 58, 1, 1, 44, 1, 1, 49, 1, 1, 1, 1, 53, 1, 1, 44, 1, 1, 47, 1, 1, 57, 1, 1, 52, 1, 1, 45, 1, 1, 47, 1};
static int out_shift_vector[46] = {23, 20, 23, 22, 23, 22, 23, 6, 22, 23, 6, 24, 23, 21, 23, 6, 22, 23, 6, 23, 23, 6, 23, 24, 22, 24, 6, 22, 23, 6, 22, 23, 6, 22, 23, 6, 22, 23, 6, 21, 23, 6, 21, 22, 6, 24};
static int activations_out_checksum[46][1] = {{
  448780 },
{
  781135 },
{
  319753 },
{
  264129 },
{
  218897 },
{
  167957 },
{
  230532 },
{
  302145 },
{
  245329 },
{
  276720 },
{
  385285 },
{
  82918 },
{
  192396 },
{
  143662 },
{
  162313 },
{
  270747 },
{
  173652 },
{
  222686 },
{
  315504 },
{
  127039 },
{
  189301 },
{
  363065 },
{
  48406 },
{
  102663 },
{
  82693 },
{
  85282 },
{
  133576 },
{
  72841 },
{
  87108 },
{
  144576 },
{
  76230 },
{
  57424 },
{
  134506 },
{
  73043 },
{
  85497 },
{
  157533 },
{
  64806 },
{
  74541 },
{
  172370 },
{
  44752 },
{
  74857 },
{
  156352 },
{
  80485 },
{
  107938 },
{
  182273 },
{
  3470 }
};
static int activations_out_size[46] = {76800, 76800, 76800, 19200, 38400, 38400, 38400, 38400, 38400, 38400, 38400, 9600, 19200, 19200, 19200, 19200, 19200, 19200, 19200, 19200, 19200, 19200, 5120, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 80};
static int layer_with_weights[46] = {1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1};
#endif

#endif  // __NETWORK_H__
