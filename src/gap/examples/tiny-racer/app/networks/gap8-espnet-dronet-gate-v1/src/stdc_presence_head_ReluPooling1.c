/*
 * pooling_layer_template.c
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
// first_layer                    0
// ULTRA_VERBOSE                  False
// verbose_log
// node                           <dory.Parsers.HW_node.HW_node object at 0x7e83c5ae8850>
// sdk                            gap_sdk
// number_of_clusters             1
// optional_type                  8bit
// func_name                      stdc_presence_head_ReluPooling1
// flag_DW                        0
// optional                       ReluAveragePool
// FLAG_BATCHNORM                 0
// has_bias                       0
// FLAG_RELU                      1
// type                           uint8_t
// conv_overlap1                  0
// conv_overlap2                  0
// padding_top                    0
// padding_bottom                 0
// padding_left                   0
// padding_right                  0
// stride                         20
// g                              1
// nif                            1
// out_mul                        16400
// out_add                        0
// out_shift                      14
// data_type_x                    uint
// data_type_y                    int
// data_type_activations          int
// data_type_weights              int
// nof                            1
// factor                         1.0
// double_buffering               1
// x_h                            20
// x_w                            20
// x_data_size_byte               8
// x_tile_size_nif                1
// x_tile_size_h                  20
// x_tile_size_w                  20
// x_tile_size_byte               400
// x_tile_size_nif_byte           1
// x_stride_w_byte                20
// x_stride_c_byte                1
// y_h                            1
// y_w                            1
// y_data_size_byte               8
// act_dim_bit                    None
// y_tile_size_nof                1
// y_tile_size_h                  1
// y_tile_size_w                  1
// y_tile_size_byte               1
// y_stride_w_byte                1
// y_stride_c_byte                1
// y_tile_size_nof_byte           1
// tile_dim_h                     1
// tile_dim_w                     1
// tile_dim_nof                   1
// tile_dim_nif                   1
// tile_n_in_last                 1
// fs1                            20
// fs2                            20
// W_data_size_byte               None
// b_data_size_byte               32
// W_tile_size_nof                1
// b_size_byte                    0
// W_tile_size_nif                1
// W_tile_size_nif_last           1
// k_tile_size_byte               0
// lambda_tile_size_byte          0
// k_size_byte                    0
// lambda_size_byte               0
// k_tile_size_byte_transfer      0
// lambda_tile_size_byte_transfer 0
// l1_x_offset                    0
// l1_y_offset                    408
// y_tile_size_nof_last           1
// y_tile_size_h_last             1
// y_tile_size_w_last             1
// y_length_nof_byte_last         1
// x_tile_size_nif_last           1
// x_tile_size_nif_byte_last      1
// x_tile_size_h_last             20
// x_tile_size_w_last             20


#include "stdc_presence_head_ReluPooling1.h"
#include "pulp.h"
#include "pmsis.h"
#include "dory_get_tile.h"
#include "dory_dma.h"
#include "pulp_nn_kernels.h"
void stdc_presence_head_ReluPooling1(
  void *args
) {
  unsigned int *real_arg = (unsigned int *) args;
  unsigned int l3_x =(unsigned int)  real_arg[0];
  unsigned int l3_y =(unsigned int)  real_arg[1];
  unsigned int l3_W =(unsigned int)  real_arg[2];
  unsigned int l2_x =(unsigned int)  real_arg[3];
  unsigned int l2_x_2 =(unsigned int)  real_arg[4];
  unsigned int l2_y =(unsigned int)  real_arg[5];
  unsigned int l2_W =(unsigned int)  real_arg[6];
  unsigned int l1_buffer =(unsigned int)  real_arg[7];
  unsigned int hyperram =(unsigned int)  real_arg[8];
  unsigned int out_shift_in = (unsigned int) real_arg[10];
  int p_r, p_l, p_t, p_b;
  unsigned short x_tile_size_nif;
  unsigned short x_tile_size_h;
  unsigned short x_tile_size_w;
  unsigned short x_tile_size_byte;
  unsigned short x_length_h_px;
  unsigned short x_length_nif_byte;
  int pad_offset_h, pad_offset_w;
  uint8_t *x;
  uint8_t *y;
  int y_tile_size_nof;
  int y_tile_size_h;
  int y_tile_size_w;
  int y_tile_size_byte;
  int y_length_h_px;
  int y_length_nof_byte;
 uint8_t *im2col;
  im2col = l1_buffer + 441;
  uint32_t dory_dma_channel = dory_dma_allocate();
  volatile DMA_copy DMA_copy_x, DMA_copy_y;
  // copy first tiles
  //l2_x has input activations

  DMA_copy_x.hwc_to_chw = 0;
  DMA_copy_x.stride_2d = 20;
  DMA_copy_x.stride_1d = 1;
  DMA_copy_x.dir = 1;
  DMA_copy_x.tid = dory_dma_channel;

  DMA_copy_y.hwc_to_chw = 0;
  DMA_copy_y.stride_2d = 1;
  DMA_copy_y.stride_1d = 1;
  DMA_copy_y.dir = 0;
  DMA_copy_y.tid = dory_dma_channel;

  // tile loop indeces
  int _i_nof_load=0, _i_nif_load=0, _i_h_load=0, _i_w_load=0;

  // last-tile flags
  int last_nof, last_nif, last_h, last_w;
  int iter;
  // tile loop nest
  for(iter=0; iter<1*1*1; iter++) {
    last_nof = (_i_nof_load+1 == 1) ? 1 : 0;
    last_nif = (_i_nof_load+1 == 1) ? 1 : 0;
    last_h = (_i_h_load+1 == 1) ? 1 : 0;
    last_w = (_i_w_load+1 == 1) ? 1 : 0;

    x_tile_size_nif = (last_nif) ? 1 : 1;
    x_tile_size_h   = (last_h)   ? 20 : 20;
    x_tile_size_w   = (last_w)   ? 20 : 20;
    x_tile_size_byte = x_tile_size_nif*x_tile_size_h*x_tile_size_w*8/8;
    x_length_nif_byte = (last_nif)   ? 1 : 1;
    // additionally overlap by padding for the first tile after a border one
    //this because in the first tile we use less pixels from x_buffer, since we have the ones of padding
    pad_offset_h=0, pad_offset_w=0;
    if(_i_h_load > 0)
      pad_offset_h = 0;
    if(_i_w_load > 0)
      pad_offset_w = 0;

    DMA_copy_x.ext = dory_get_tile_3d(l2_x, _i_h_load, _i_w_load, _i_nif_load, 20, 20, 1, 20, 1,  0, 0,0, pad_offset_h, pad_offset_w, 0, 8);
    DMA_copy_x.loc = (l1_buffer + 0);
    DMA_copy_x.number_of_2d_copies = x_tile_size_h;
    DMA_copy_x.number_of_1d_copies = x_tile_size_w;
    DMA_copy_x.length_1d_copy = x_length_nif_byte;
    dory_dma_memcpy_async(&DMA_copy_x);
    dory_dma_barrier(&DMA_copy_x);
    y_tile_size_h   = (last_h)   ? 1 : 1;
    y_tile_size_w   = (last_w)   ? 1 : 1;

    x = (uint8_t *) (l1_buffer + 0);
    y = (uint8_t *) (l1_buffer + 408);


    y_tile_size_nof = (last_nof) ? 1 : 1;
    y_tile_size_h   = (last_h)   ? 1 : 1;
    y_tile_size_w   = (last_w)   ? 1 : 1;
    y_tile_size_byte = y_tile_size_nof*y_tile_size_h*y_tile_size_w*8/8;
    y_length_nof_byte = (last_nof)   ? 1 : 1;
    p_r = 0;
    p_l = 0;
    p_t = 0;
    p_b = 0;
    if (_i_h_load == 0)
      p_t = 0;
    if (_i_w_load == 0)
      p_l = 0;
    if (_i_h_load == 1-1)
      p_b = 0;
    if (_i_w_load == 1-1)
      p_r = 0;
    pi_cl_team_barrier(0);

// aggiungere padding su tutti i lati, acc_out, and filter asymettric
    pulp_nn_avgpool(
    x, y,
    16400,
    14,
    0,
    x_tile_size_w,
    x_tile_size_h,
    x_tile_size_nif,
    y_tile_size_w,
    y_tile_size_h,
    20,
    20,
    p_t,
    p_b,
    p_l,
    p_r,
    20,
    20,
    1
    );
    pi_cl_team_barrier(0);
    // transfering of output to L2
    DMA_copy_y.ext = dory_get_tile_3d(l2_y, _i_h_load, _i_w_load, _i_nof_load, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 8);
    DMA_copy_y.loc = (l1_buffer + 408);
    DMA_copy_y.number_of_2d_copies = y_tile_size_h;
    DMA_copy_y.number_of_1d_copies = y_tile_size_w;
    DMA_copy_y.length_1d_copy = y_length_nof_byte;
    dory_dma_memcpy_async(&DMA_copy_y);
    dory_dma_barrier(&DMA_copy_y);

    // loop nest is nof,h,w,(nif=0)
    _i_w_load += 1;
    if(_i_w_load==1)
    {
      _i_w_load = 0;
      _i_h_load += 1;
      if(_i_h_load==1)
      {
        _i_h_load = 0;
        _i_nif_load += 1;
        _i_nof_load += 1;
      }
    }
    pi_cl_team_barrier(0);
  }
  dory_dma_free(&DMA_copy_y);
}
