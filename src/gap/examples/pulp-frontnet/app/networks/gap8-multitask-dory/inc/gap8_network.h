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
  "gap8_BNReluConvolution0_weights.hex", "gap8_BNReluConvolution1_weights.hex", "gap8_BNReluConvolution2_weights.hex", "gap8_BNReluConvolution3_weights.hex", "gap8_BNReluConvolution4_weights.hex", "gap8_BNReluConvolution5_weights.hex", "gap8_BNReluConvolution6_weights.hex", "gap8_BNReluConvolution8_weights.hex", "gap8_BNReluConvolution9_weights.hex", "gap8_BNReluConvolution10_weights.hex", "gap8_BNReluConvolution12_weights.hex", "gap8_BNReluConvolution13_weights.hex", "gap8_BNReluConvolution15_weights.hex", "gap8_BNReluConvolution16_weights.hex", "gap8_BNReluConvolution18_weights.hex", "gap8_BNReluConvolution19_weights.hex", "gap8_BNReluConvolution21_weights.hex", "gap8_BNReluConvolution22_weights.hex", "gap8_BNReluConvolution24_weights.hex", "gap8_BNReluConvolution25_weights.hex", "gap8_BNReluConvolution27_weights.hex", "gap8_BNReluConvolution28_weights.hex", "gap8_BNReluConvolution30_weights.hex", "gap8_BNReluConvolution31_weights.hex", "gap8_BNReluConvolution33_weights.hex", "gap8_BNReluConvolution34_weights.hex", "gap8_BNReluConvolution36_weights.hex", "gap8_BNReluConvolution37_weights.hex", "gap8_BNReluConvolution39_weights.hex", "gap8_BNReluConvolution40_weights.hex", "gap8_BNReluConvolution42_weights.hex", "gap8_BNReluConvolution43_weights.hex", "gap8_BNReluConvolution45_weights.hex", "gap8_BNReluConvolution46_weights.hex", "gap8_BNReluConvolution47_weights.hex"
};
static int L3_weights_size[35];
static int layers_pointers[48];
static char * Layers_name[48] = {"gap8_BNReluConvolution0", "gap8_BNReluConvolution1", "gap8_BNReluConvolution2", "gap8_BNReluConvolution3", "gap8_BNReluConvolution4", "gap8_BNReluConvolution5", "gap8_BNReluConvolution6", "gap8_ReluQAddition7", "gap8_BNReluConvolution8", "gap8_BNReluConvolution9", "gap8_BNReluConvolution10", "gap8_ReluQAddition11", "gap8_BNReluConvolution12", "gap8_BNReluConvolution13", "gap8_ReluQAddition14", "gap8_BNReluConvolution15", "gap8_BNReluConvolution16", "gap8_ReluQAddition17", "gap8_BNReluConvolution18", "gap8_BNReluConvolution19", "gap8_ReluQAddition20", "gap8_BNReluConvolution21", "gap8_BNReluConvolution22", "gap8_ReluQAddition23", "gap8_BNReluConvolution24", "gap8_BNReluConvolution25", "gap8_ReluQAddition26", "gap8_BNReluConvolution27", "gap8_BNReluConvolution28", "gap8_ReluQAddition29", "gap8_BNReluConvolution30", "gap8_BNReluConvolution31", "gap8_ReluQAddition32", "gap8_BNReluConvolution33", "gap8_BNReluConvolution34", "gap8_ReluQAddition35", "gap8_BNReluConvolution36", "gap8_BNReluConvolution37", "gap8_ReluQAddition38", "gap8_BNReluConvolution39", "gap8_BNReluConvolution40", "gap8_ReluQAddition41", "gap8_BNReluConvolution42", "gap8_BNReluConvolution43", "gap8_ReluQAddition44", "gap8_BNReluConvolution45", "gap8_BNReluConvolution46", "gap8_BNReluConvolution47"};
static int L3_input_layers[48] = {1,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int L3_output_layers[48] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int allocate_layer[48] = {1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1};
static int branch_input[48] = {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0};
static int branch_output[48] = {0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0};
static int branch_change[48] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int weights_checksum[48] = {251, 190, 119, 56, 214, 174, 224, 0, 204, 162, 189, 0, 150, 247, 0, 246, 14, 0, 13, 74, 0, 136, 145, 0, 228, 211, 0, 151, 151, 0, 55, 15, 0, 122, 240, 0, 46, 213, 0, 220, 154, 0, 119, 165, 0, 12, 137, 138};
static int weights_size[48] = {136, 136, 192, 204, 400, 340, 560, 0, 448, 272, 384, 0, 272, 384, 0, 272, 384, 0, 272, 384, 0, 272, 384, 0, 272, 384, 0, 272, 384, 0, 272, 384, 0, 272, 384, 0, 272, 384, 0, 272, 384, 0, 272, 384, 0, 272, 288, 160};
static int activations_checksum[48][1] = {{
  54  },
{
  83  },
{
  78  },
{
  17  },
{
  45  },
{
  228  },
{
  61  },
{
  37  },
{
  227  },
{
  2  },
{
  92  },
{
  155  },
{
  184  },
{
  127  },
{
  111  },
{
  53  },
{
  43  },
{
  53  },
{
  151  },
{
  100  },
{
  196  },
{
  210  },
{
  164  },
{
  215  },
{
  188  },
{
  101  },
{
  29  },
{
  208  },
{
  16  },
{
  60  },
{
  180  },
{
  163  },
{
  208  },
{
  166  },
{
  115  },
{
  84  },
{
  233  },
{
  88  },
{
  70  },
{
  112  },
{
  188  },
{
  152  },
{
  150  },
{
  42  },
{
  15  },
{
  169  },
{
  125  },
{
  156  }
};
static int activations_size[48] = {25600, 51200, 51200, 76800, 19200, 32000, 32000, 32000, 32000, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 19200};
static int out_mult_vector[48] = {1, 1, 1, 1, 1, 1, 1, 52, 1, 1, 1, 44, 1, 1, 49, 1, 1, 49, 1, 1, 32, 1, 1, 49, 1, 1, 53, 1, 1, 49, 1, 1, 34, 1, 1, 57, 1, 1, 48, 1, 1, 44, 1, 1, 42, 1, 1, 1};
static int out_shift_vector[48] = {24, 20, 23, 22, 23, 20, 23, 6, 23, 20, 22, 6, 22, 22, 6, 22, 22, 6, 22, 23, 5, 22, 22, 6, 22, 22, 6, 22, 22, 6, 21, 23, 6, 21, 23, 6, 22, 22, 6, 21, 23, 6, 21, 23, 6, 21, 22, 23};
static int activations_out_checksum[48][1] = {{
  469075 },
{
  1395022 },
{
  1374481 },
{
  172333 },
{
  250852 },
{
  226365 },
{
  189989 },
{
  295139 },
{
  87810 },
{
  337244 },
{
  173467 },
{
  129464 },
{
  76671 },
{
  252783 },
{
  264757 },
{
  126507 },
{
  142389 },
{
  277399 },
{
  137828 },
{
  226244 },
{
  419282 },
{
  246436 },
{
  186839 },
{
  439228 },
{
  163173 },
{
  212253 },
{
  490192 },
{
  215312 },
{
  151868 },
{
  358324 },
{
  364451 },
{
  199888 },
{
  267942 },
{
  556403 },
{
  266068 },
{
  349673 },
{
  257880 },
{
  263238 },
{
  363120 },
{
  574652 },
{
  178584 },
{
  344726 },
{
  679978 },
{
  185359 },
{
  314537 },
{
  914045 },
{
  454812 },
{
  512490 }
};
static int activations_out_size[48] = {51200, 51200, 76800, 19200, 32000, 32000, 32000, 32000, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 25600, 19200, 12800};
static int layer_with_weights[48] = {1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1};
#endif

#endif  // __NETWORK_H__
