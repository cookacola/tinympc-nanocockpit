#include "network.h"
#include "espnet_output.h"
#include "mem.h"
#include <string.h>
#include <stdio.h>
#include "espnet_encoder_network.h"
#include "espnet_global_head_network.h"
#include "espnet_gate_encoder_network.h"
#include "espnet_gate_head_network.h"
#include "espnet_corner_head_network.h"

static void *saved_input;
static PI_L2 uint8_t features[ESPNET_FEATURE_BYTES];
static PI_L2 uint8_t outputs[ESPNET_OUTPUT_BYTES];
static pi_task_t step_done[5];
static void *workspace, *final_output;
static size_t workspace_size;
static int run_exec, run_dir, busy;
static pi_device_t *run_cluster;
static pi_task_t *user_done;

static void finished(void *arg) {
    (void)arg;
    memcpy(final_output, outputs, ESPNET_OUTPUT_BYTES);
    busy = 0;
    pi_task_push(user_done);
}
static void gate_finished(void *arg) {
    (void)arg;
    memcpy(workspace, features, ESPNET_FEATURE_BYTES);
    espnet_corner_head_network_run_async_cl(workspace, workspace_size,
        outputs, run_exec, run_dir, run_cluster,
        pi_task_callback(&step_done[4], finished, NULL));
}
static void gate_encoder_finished(void *arg) {
    (void)arg;
    memcpy(workspace, features, ESPNET_FEATURE_BYTES);
    espnet_gate_head_network_run_async_cl(workspace, workspace_size,
        outputs + ESPNET_GATE_OFFSET, run_exec, run_dir, run_cluster,
        pi_task_callback(&step_done[3], gate_finished, NULL));
}
static void collision_finished(void *arg) {
    (void)arg;
    ram_read(workspace, saved_input, ESPNET_INPUT_BYTES);
    espnet_gate_encoder_network_run_async_cl(workspace, workspace_size,
        features, run_exec, run_dir, run_cluster,
        pi_task_callback(&step_done[2], gate_encoder_finished, NULL));
}
static void encoder_finished(void *arg) {
    (void)arg;
    memcpy(workspace, features, ESPNET_FEATURE_BYTES);
    espnet_global_head_network_run_async_cl(workspace, workspace_size,
        outputs + ESPNET_COLLISION_OFFSET, run_exec, run_dir, run_cluster,
        pi_task_callback(&step_done[1], collision_finished, NULL));
}
void network_initialize(void) {
    saved_input = ram_malloc(ESPNET_INPUT_BYTES);
    if (!saved_input) { printf("ESPNET input preservation allocation failed\n"); pmsis_exit(-1); }
    espnet_encoder_network_initialize();
    espnet_global_head_network_initialize();
    espnet_gate_encoder_network_initialize();
    espnet_gate_head_network_initialize();
    espnet_corner_head_network_initialize();
}
void network_terminate(void) {
    espnet_encoder_network_terminate();
    espnet_global_head_network_terminate();
    espnet_gate_encoder_network_terminate();
    espnet_gate_head_network_terminate();
    espnet_corner_head_network_terminate();
    ram_free(saved_input, ESPNET_INPUT_BYTES);
    saved_input = NULL;
}
void network_run_async_cl(void *buffer, size_t size, void *output,
                          int exec, int initial_dir,
                          pi_device_t *cluster, pi_task_t *done) {
    if (busy || !saved_input || size < ESPNET_WORKSPACE_BYTES || initial_dir != 1) {
        printf("ESPNET invalid inference state/workspace/direction\n");
        pmsis_exit(-1);
    }
    busy = 1;
    workspace = buffer; workspace_size = size; final_output = output;
    run_exec = exec; run_dir = initial_dir; run_cluster = cluster; user_done = done;
    ram_write(saved_input, buffer, ESPNET_INPUT_BYTES);
    espnet_encoder_network_run_async_cl(buffer, size, features, exec, initial_dir,
        cluster, pi_task_callback(&step_done[0], encoder_finished, NULL));
}
