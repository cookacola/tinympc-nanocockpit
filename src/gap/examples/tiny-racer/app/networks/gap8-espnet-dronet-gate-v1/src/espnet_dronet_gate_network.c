#include "network.h"
#include "stdc_encoder_network.h"
#include "stdc_corner_head_network.h"
#include "stdc_gate_head_network.h"
#include "stdc_presence_head_network.h"
#include "stdc_navigation_head_network.h"
#include <string.h>

#define SHARED_BYTES 25600
#define CORNER_BYTES 1600
#define GATE_BYTES 400
#define PRESENCE_BYTES 1
#define NAVIGATION_BYTES 2

static PI_L2 uint8_t shared_output[SHARED_BYTES];
static PI_L2 uint8_t corner_output[CORNER_BYTES];
static PI_L2 uint8_t gate_output[GATE_BYTES];
static PI_L2 uint8_t presence_output[PRESENCE_BYTES];
static PI_L2 uint8_t navigation_output[NAVIGATION_BYTES];
static PI_FC_L1 pi_task_t encoder_done, corner_done, gate_done, presence_done, navigation_done;
static void *workspace, *combined_output;
static size_t workspace_size;
static int run_exec, run_initial_dir;
static pi_device_t *run_cluster;
static pi_task_t *user_done;

static void navigation_finished(void *arg) {
  (void)arg;
#ifdef TINY_RACER_PARITY_TEST
  printf("PARITY stage=navigation\n");
#endif
  memcpy(combined_output, corner_output, CORNER_BYTES);
  memcpy((uint8_t *)combined_output + CORNER_BYTES, gate_output, GATE_BYTES);
  memcpy((uint8_t *)combined_output + CORNER_BYTES + GATE_BYTES,
         presence_output, PRESENCE_BYTES);
  memcpy((uint8_t *)combined_output + CORNER_BYTES + GATE_BYTES + PRESENCE_BYTES,
         navigation_output, NAVIGATION_BYTES);
  pi_task_push(user_done);
}
static void presence_finished(void *arg) {
  (void)arg;
#ifdef TINY_RACER_PARITY_TEST
  printf("PARITY stage=presence\n");
#endif
  memcpy(workspace, shared_output, SHARED_BYTES);
  stdc_navigation_head_network_run_async_cl(
      workspace, workspace_size, navigation_output, run_exec, run_initial_dir,
      run_cluster, pi_task_callback(&navigation_done, navigation_finished, NULL));
}
static void gate_finished(void *arg) {
  (void)arg;
#ifdef TINY_RACER_PARITY_TEST
  printf("PARITY stage=gate\n");
#endif
  memcpy(workspace, shared_output, SHARED_BYTES);
  stdc_presence_head_network_run_async_cl(
      workspace, workspace_size, presence_output, run_exec, run_initial_dir,
      run_cluster, pi_task_callback(&presence_done, presence_finished, NULL));
}
static void corner_finished(void *arg) {
  (void)arg;
#ifdef TINY_RACER_PARITY_TEST
  printf("PARITY stage=corner\n");
#endif
  memcpy(workspace, shared_output, SHARED_BYTES);
  stdc_gate_head_network_run_async_cl(
      workspace, workspace_size, gate_output, run_exec, run_initial_dir,
      run_cluster, pi_task_callback(&gate_done, gate_finished, NULL));
}
static void encoder_finished(void *arg) {
  (void)arg;
#ifdef TINY_RACER_PARITY_TEST
  printf("PARITY stage=encoder\n");
#endif
  memcpy(workspace, shared_output, SHARED_BYTES);
  stdc_corner_head_network_run_async_cl(
      workspace, workspace_size, corner_output, run_exec, run_initial_dir,
      run_cluster, pi_task_callback(&corner_done, corner_finished, NULL));
}
void network_initialize(void) {
  stdc_encoder_network_initialize();
  stdc_corner_head_network_initialize();
  stdc_gate_head_network_initialize();
  stdc_presence_head_network_initialize();
  stdc_navigation_head_network_initialize();
}
void network_terminate(void) {
  stdc_encoder_network_terminate();
  stdc_corner_head_network_terminate();
  stdc_gate_head_network_terminate();
  stdc_presence_head_network_terminate();
  stdc_navigation_head_network_terminate();
}
void network_run_async_cl(void *l2_buffer, size_t l2_buffer_size,
                          void *l2_final_output, int exec, int initial_dir,
                          pi_device_t *cluster, pi_task_t *network_done) {
  workspace = l2_buffer; workspace_size = l2_buffer_size;
  combined_output = l2_final_output; run_exec = exec;
  run_initial_dir = initial_dir; run_cluster = cluster; user_done = network_done;
  stdc_encoder_network_run_async_cl(
      workspace, workspace_size, shared_output, exec, initial_dir, cluster,
      pi_task_callback(&encoder_done, encoder_finished, NULL));
}
