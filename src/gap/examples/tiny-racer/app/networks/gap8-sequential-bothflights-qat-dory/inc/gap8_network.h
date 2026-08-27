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
static int weights_checksum[24] = {198, 80, 253, 34, 76, 251, 153, 224, 226, 126, 149, 122, 215, 140, 131, 134, 248, 3, 51, 80, 181, 193, 88, 149};
static int weights_size[24] = {400, 400, 768, 600, 1280, 800, 2304, 1200, 4096, 1600, 7680, 2400, 10752, 2400, 10752, 2400, 10752, 2400, 10752, 2400, 10752, 2400, 10752, 1344};
static int activations_checksum[24][1] = {{
  35  },
{
  141  },
{
  122  },
{
  165  },
{
  94  },
{
  101  },
{
  231  },
{
  121  },
{
  9  },
{
  223  },
{
  68  },
{
  56  },
{
  25  },
{
  210  },
{
  63  },
{
  89  },
{
  29  },
{
  44  },
{
  166  },
{
  167  },
{
  23  },
{
  14  },
{
  215  },
{
  90  }
};
static int activations_size[24] = {19200, 76800, 76800, 115200, 28800, 38400, 38400, 57600, 14400, 19200, 19200, 28800, 28800, 28800, 28800, 28800, 28800, 28800, 28800, 28800, 28800, 28800, 28800, 28800};
static int out_mult_vector[24] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
static int out_shift_vector[24] = {24, 22, 23, 22, 23, 21, 23, 22, 23, 21, 17, 21, 24, 20, 23, 21, 23, 20, 23, 19, 22, 20, 19, 22};
static int activations_out_checksum[24][1] = {{
  932493 },
{
  738938 },
{
  661157 },
{
  316510 },
{
  214629 },
{
  346087 },
{
  293753 },
{
  167177 },
{
  119007 },
{
  199236 },
{
  279608 },
{
  363545 },
{
  224722 },
{
  252991 },
{
  183641 },
{
  186909 },
{
  132908 },
{
  130470 },
{
  116647 },
{
  130839 },
{
  58126 },
{
  27095 },
{
  73818 },
{
  185801 }
};
static int activations_out_size[24] = {76800, 76800, 115200, 28800, 38400, 38400, 57600, 14400, 19200, 19200, 28800, 28800, 28800, 28800, 28800, 28800, 28800, 28800, 28800, 28800, 28800, 28800, 28800, 3600};
static int layer_with_weights[24] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
#endif

#endif  // __NETWORK_H__
