#include "network.h"
#include "stdc_encoder_network.h"
#include "stdc_corner_head_network.h"
#include "stdc_danger_head_network.h"
#include <string.h>

#define STDC_SHARED_BYTES 38400
#define STDC_CORNER_BYTES 4800
#define STDC_DANGER_BYTES 80

static PI_L2 uint8_t shared_output[STDC_SHARED_BYTES];
static PI_L2 uint8_t corner_output[STDC_CORNER_BYTES];
static PI_L2 uint8_t danger_output[STDC_DANGER_BYTES];
static PI_FC_L1 pi_task_t encoder_done;
static PI_FC_L1 pi_task_t corner_done;
static PI_FC_L1 pi_task_t danger_done;
static void *workspace;
static size_t workspace_size;
static void *combined_output;
static int run_exec;
static int run_initial_dir;
static pi_device_t *run_cluster;
static pi_task_t *user_done;

static void danger_finished(void *arg) {
  (void)arg;
  memcpy(combined_output, corner_output, STDC_CORNER_BYTES);
  memcpy((uint8_t *)combined_output + STDC_CORNER_BYTES,
         danger_output, STDC_DANGER_BYTES);
  pi_task_push(user_done);
}

static void corner_finished(void *arg) {
  (void)arg;
  memcpy(workspace, shared_output, STDC_SHARED_BYTES);
  stdc_danger_head_network_run_async_cl(
      workspace, workspace_size, danger_output, run_exec, run_initial_dir,
      run_cluster, pi_task_callback(&danger_done, danger_finished, NULL));
}

static void encoder_finished(void *arg) {
  (void)arg;
  memcpy(workspace, shared_output, STDC_SHARED_BYTES);
  stdc_corner_head_network_run_async_cl(
      workspace, workspace_size, corner_output, run_exec, run_initial_dir,
      run_cluster, pi_task_callback(&corner_done, corner_finished, NULL));
}

void network_initialize(void) {
  stdc_encoder_network_initialize();
  stdc_corner_head_network_initialize();
  stdc_danger_head_network_initialize();
}

void network_terminate(void) {
  stdc_encoder_network_terminate();
  stdc_corner_head_network_terminate();
  stdc_danger_head_network_terminate();
}

void network_run_async_cl(void *l2_buffer, size_t l2_buffer_size,
                          void *l2_final_output, int exec, int initial_dir,
                          pi_device_t *cluster, pi_task_t *network_done) {
  workspace = l2_buffer;
  workspace_size = l2_buffer_size;
  combined_output = l2_final_output;
  run_exec = exec;
  run_initial_dir = initial_dir;
  run_cluster = cluster;
  user_done = network_done;
  stdc_encoder_network_run_async_cl(
      workspace, workspace_size, shared_output, exec, initial_dir, cluster,
      pi_task_callback(&encoder_done, encoder_finished, NULL));
}
