/*
 * main.c
 * Charles Chen <charles8@andrew.cmu.edu>
 *
 * Copyright (C) 2022-2025 IDSIA, USI-SUPSI
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
 *
 * This software is based on the following publication:
 *    E. Cereda, A. Giusti, D. Palossi. "NanoCockpit: Performance-optimized
 *    Application Framework for AI-based Autonomous Nanorobotics"
 * We kindly ask for a citation if you use in academic work.
 */

#include "config.h"
#include "camera.h"
#include "cluster.h"
#include "collision_uart.h"
#include "coroutine.h"
#include "cpx/cpx.h"
#include "crc32.h"
#include "debug.h"
#include "espnet_output.h"
#include "espnet_decode.h"
#include "mem.h"
#include "network.h"
#include "soc.h"
#include "streamer.h"
#include "trace.h"
#include "uart.h"
#include "uart_protocol.h"

#include <pmsis.h>

#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#define IMAGE_WIDTH 160
#define IMAGE_HEIGHT 160
#define GAP8_TEMPORAL_NETWORK 1
#define CORNER_COUNT 4
#define CORNER_COORD_COUNT 8
#define NETWORK_INPUT_BYTES ESPNET_INPUT_BYTES
#define NETWORK_L2_WORKSPACE_SIZE ESPNET_WORKSPACE_BYTES
#define CORNER_HEATMAP_BYTES ESPNET_CORNER_BYTES
#define DANGER_MAP_OFFSET ESPNET_COLLISION_OFFSET
#define DANGER_MAP_BYTES 3
#define GAP8_OUTPUT_BYTES ESPNET_OUTPUT_BYTES
_Static_assert(CAMERA_CROP_WIDTH == 160 && CAMERA_CROP_HEIGHT == 160,
               "ESPNet requires a 160x160 grayscale crop");

static uart_t uart;
static uart_protocol_t uart_protocol;
static camera_t camera;
static cpx_t cpx;
static streamer_t streamer;
static pi_device_t cluster;

static PI_FC_L1 state_msg_t latest_state;
static PI_FC_L1 uint32_t state_timestamp;
static PI_FC_L1 tof_msg_t latest_tof;
static PI_FC_L1 uint32_t tof_timestamp;
static PI_L2 inference_stamped_msg_t latest_inference;

static void *l2_buffer;
static size_t l2_buffer_size;
#ifdef GAP8_TEMPORAL_NETWORK
static PI_L2 uint8_t previous_frame[IMAGE_WIDTH * IMAGE_HEIGHT];
static PI_FC_L1 bool previous_frame_valid;
#endif

static PI_FC_L1 co_fn_ctx_t inference_ctx;
static PI_FC_L1 co_fn_ctx_t streamer_rx_ctx;

typedef struct {
    uint32_t stm32_timestamp;
    uint32_t source_timestamp_ms;
    bool temporal_valid;
    frame_t *camera_frame;
} inference_args_t;

#ifdef TINY_RACER_PARITY_TEST
/*
 * Deterministic, one-shot DORY replay for comparing against the host ONNX
 * graph. PARITY_INPUT supplies this exact 160x160x3 HWC uint8 tensor through
 * ReadFS; no camera frame or visualization path participates.
 */
static void run_parity_test(void) {
    static PI_FC_L1 pi_task_t network_done;
    void *input_l3 = ram_malloc(NETWORK_INPUT_BYTES);

    if (!input_l3) {
        printf("PARITY ERROR: unable to allocate input in L3\n");
        pmsis_exit(-1);
    }

    const size_t input_size =
        load_file_to_ram(input_l3, TINY_RACER_PARITY_INPUT_FILE);
    if (input_size != NETWORK_INPUT_BYTES) {
        printf("PARITY ERROR: %s is %u bytes; expected %u\n",
               TINY_RACER_PARITY_INPUT_FILE, (unsigned int)input_size,
               (unsigned int)NETWORK_INPUT_BYTES);
        pmsis_exit(-1);
    }

    ram_read(l2_buffer, input_l3, NETWORK_INPUT_BYTES);
    printf("PARITY input_crc32=%08lx\n",
           (unsigned long)crc32CalculateBuffer(l2_buffer,
                                               NETWORK_INPUT_BYTES));

    pi_task_block(&network_done);
    network_run_async_cl(l2_buffer, l2_buffer_size, l2_buffer, 0, 1,
                         &cluster, &network_done);
    pi_task_wait_on(&network_done);

    printf("PARITY corner_crc32=%08lx danger_crc32=%08lx output_crc32=%08lx\n",
           (unsigned long)crc32CalculateBuffer(l2_buffer,
                                               CORNER_HEATMAP_BYTES),
           (unsigned long)crc32CalculateBuffer(
               (const uint8_t *)l2_buffer + DANGER_MAP_OFFSET,
               DANGER_MAP_BYTES),
           (unsigned long)crc32CalculateBuffer(l2_buffer,
                                               GAP8_OUTPUT_BYTES));
    pmsis_exit(0);
}
#endif

CO_FN_DECLARE(inference_task);
CO_FN_DECLARE(streamer_rx_task);

/* Copy/pack input before inference because DORY overwrites its workspace. */
static void copy_network_input(const frame_t *frame) {
#ifdef GAP8_TEMPORAL_NETWORK
    uint8_t *packed = (uint8_t *)l2_buffer;
    if (!previous_frame_valid) {
        memcpy(previous_frame, frame->buffer, sizeof(previous_frame));
        previous_frame_valid = true;
    }
    for (size_t pixel = 0; pixel < IMAGE_WIDTH * IMAGE_HEIGHT; ++pixel) {
        packed[3 * pixel] = previous_frame[pixel];
        packed[3 * pixel + 1] = frame->buffer[pixel];
        packed[3 * pixel + 2] = ((int)frame->buffer[pixel] -
                                 (int)previous_frame[pixel] + 255) / 2;
    }
    memcpy(previous_frame, frame->buffer, sizeof(previous_frame));
#else
    memcpy(l2_buffer,
           frame->buffer + NETWORK_INPUT_TOP * IMAGE_WIDTH,
           IMAGE_WIDTH * NETWORK_INPUT_HEIGHT);
#endif
}

/* Display the native left/center/right collision probabilities. */
static void overlay_collision(uint8_t *frame, const float probability[3]) {
    for (int y = 0; y < IMAGE_HEIGHT; ++y) {
        for (int x = 0; x < IMAGE_WIDTH; ++x) {
            int sector = x * 3 / IMAGE_WIDTH;
            float scale = 1.0f - 0.75f * probability[sector];
            frame[y * IMAGE_WIDTH + x] =
                (uint8_t)(frame[y * IMAGE_WIDTH + x] * scale + 0.5f);
        }
    }
}

static void draw_corner_marker(uint8_t *frame, float x, float y) {
    const int center_x = (int)(x + 0.5f);
    const int center_y = (int)(y + 0.5f);

    for (int offset = -3; offset <= 3; ++offset) {
        const int horizontal_x = center_x + offset;
        const int vertical_y = center_y + offset;

        if (horizontal_x >= 0 && horizontal_x < IMAGE_WIDTH &&
            center_y >= 0 && center_y < IMAGE_HEIGHT) {
            frame[center_y * IMAGE_WIDTH + horizontal_x] = 255;
        }
        if (center_x >= 0 && center_x < IMAGE_WIDTH &&
            vertical_y >= 0 && vertical_y < IMAGE_HEIGHT) {
            frame[vertical_y * IMAGE_WIDTH + center_x] = 255;
        }
    }
}

/*
 * The legacy streamer metadata has only four float slots. For this temporary
 * visualization ABI, each slot carries one corner as a linear pixel position:
 *
 *     encoded = y * IMAGE_WIDTH + x
 *
 * The receiver can recover it with x = encoded % IMAGE_WIDTH and
 * y = encoded / IMAGE_WIDTH. The four slots are ordered TL, TR, BR, BL.
 */
static float encode_corner(float x, float y) {
    if (x < 0 || y < 0) return -1.0f;
    return (int)(y + 0.5f) * IMAGE_WIDTH + (int)(x + 0.5f);
}

CO_FN_BEGIN(camera_callback, frame_t *, camera_frame)
{
    static PI_FC_L1 inference_args_t inference_args;
    static PI_FC_L1 co_event_t inference_done;
    static PI_FC_L1 co_event_t streamer_tx_done;

    /* Sample validity before packing initializes the previous frame. */
    inference_args.temporal_valid = previous_frame_valid;
    copy_network_input(camera_frame);

    inference_args = (inference_args_t) {
        .stm32_timestamp = latest_state.timestamp,
        .source_timestamp_ms = camera_frame->frame_timestamp / 1000u,
        .temporal_valid = inference_args.temporal_valid,
        .camera_frame = camera_frame,
    };
    co_fn_push_start(&inference_ctx, inference_task, &inference_args,
                     co_event_init(&inference_done));
    CO_WAIT(&inference_done);

    /*
     * inference_task has finished modifying this frame. Only now may the
     * streamer read it.
     */
    streamer_send_frame_async(
        &streamer,
        camera_frame,
        &latest_state, state_timestamp,
        &latest_tof, tof_timestamp,
        &latest_inference,
        co_event_init(&streamer_tx_done)
    );
    CO_WAIT(&streamer_tx_done);
}
CO_FN_END()

CO_FN_BEGIN(inference_task, inference_args_t *, inference_args)
{
    static PI_FC_L1 co_event_t network_done;
    static PI_FC_L1 co_event_t collision_tx_done;
    static PI_FC_L1 uint16_t collision_sequence;
    static PI_L2 espnet_collision_packet_t collision_packet;
    static PI_FC_L1 float corners[CORNER_COORD_COUNT];
    static PI_FC_L1 espnet_decoded_t decoded;
    static PI_FC_L1 frame_t *camera_frame;

    camera_frame = inference_args->camera_frame;

    trace_set(TRACE_USER_0, true);
    network_run_async_cl(l2_buffer, l2_buffer_size, l2_buffer, 0, 1,
                         &cluster, co_event_init(&network_done));
    CO_WAIT(&network_done);
    trace_set(TRACE_USER_0, false);

    espnet_decode((const uint8_t *)l2_buffer, &decoded);
    overlay_collision(camera_frame->buffer, decoded.collision);
    /* Stream viewer expects TL, TR, BR, BL; model order is LT, RT, LB, RB. */
    const int stream_order[4] = {0, 1, 3, 2};
    for (int corner = 0; corner < CORNER_COUNT; ++corner) {
        int source = stream_order[corner];
        corners[2 * corner] = decoded.visible[source % 2] ?
            decoded.corners[2 * source] : -1.0f;
        corners[2 * corner + 1] = decoded.visible[source % 2] ?
            decoded.corners[2 * source + 1] : -1.0f;
        if (decoded.visible[source % 2])
            draw_corner_marker(camera_frame->buffer,
                               corners[2 * corner], corners[2 * corner + 1]);
    }
    printf("ESPNET collision_milli=%d,%d,%d affordance_milli=%d,%d,%d visibility_milli=%d,%d\n",
           (int)(1000 * decoded.collision[0]), (int)(1000 * decoded.collision[1]),
           (int)(1000 * decoded.collision[2]), (int)(1000 * decoded.affordance[0]),
           (int)(1000 * decoded.affordance[1]), (int)(1000 * decoded.affordance[2]),
           (int)(1000 * decoded.visibility[0]), (int)(1000 * decoded.visibility[1]));

    latest_inference = (inference_stamped_msg_t) {
        .stm32_timestamp = inference_args->stm32_timestamp,
        .x = encode_corner(corners[0], corners[1]),
        .y = encode_corner(corners[2], corners[3]),
        .z = encode_corner(corners[4], corners[5]),
        .phi = encode_corner(corners[6], corners[7]),
    };

    collision_sequence = espnet_collision_next_sequence(collision_sequence);
    espnet_collision_send_async(&uart, &collision_packet,
        inference_args->source_timestamp_ms, collision_sequence,
        decoded.collision, inference_args->temporal_valid,
        co_event_init(&collision_tx_done));
    CO_WAIT(&collision_tx_done);
}
CO_FN_END()

static void streamer_rx_start(void) {
    co_fn_push_start(&streamer_rx_ctx, streamer_rx_task, NULL, NULL);
}

CO_FN_BEGIN(streamer_rx_task, void *, arg)
{
    static PI_L2 offboard_buffer_t offboard_buffer;
    static PI_FC_L1 streamer_buffer_t offboard_buffer_rx;
    static PI_FC_L1 co_event_t done_task;

    (void)arg;

    while (true) {
        streamer_buffer_init(&offboard_buffer_rx, &offboard_buffer,
                             sizeof(offboard_buffer));
        streamer_receive_buffer_async(&streamer, &offboard_buffer_rx,
                                      co_event_init(&done_task));
        CO_WAIT(&done_task);

        if (offboard_buffer_rx.type != STREAMER_TYPE_INFERENCE) {
            printf("discarded streamer buffer type %d (expected %d)\n",
                   offboard_buffer_rx.type, STREAMER_TYPE_INFERENCE);
            continue;
        }

        streamer_stats_frame_completed(&streamer, &offboard_buffer.stats);
    }
}
CO_FN_END()

CO_FN_BEGIN(uart_callback, uart_msg_t *, message)
{
    if (memcmp(message->header, UART_STATE_MSG_HEADER,
               UART_HEADER_LENGTH) == 0) {
        latest_state = message->state;
        state_timestamp = message->recv_timestamp;
    } else if (memcmp(message->header, UART_TOF_MSG_HEADER,
                      UART_HEADER_LENGTH) == 0) {
        latest_tof = message->tof;
        tof_timestamp = message->recv_timestamp;
    }
}
CO_FN_END()

static void main_task(void) {
    soc_init();

#ifndef TINY_RACER_PARITY_TEST
    uart_init(&uart);
    uart_protocol_init(&uart_protocol, &uart, uart_callback);

    camera_init(&camera, camera_callback);

    cpx_init(&cpx);
    streamer_init(&streamer, &camera, &cpx);
    streamer_alloc_frames(&streamer, &camera);
#endif

    cluster_init(&cluster);
    mem_init();
    network_initialize();

    l2_buffer_size = NETWORK_L2_WORKSPACE_SIZE;
    l2_buffer = pi_l2_malloc(l2_buffer_size);
    VERBOSE_PRINT("Network:\t\t\t%s, %dB @ L2, 0x%08x\n",
                  l2_buffer ? "OK" : "Failed",
                  l2_buffer_size, l2_buffer);
    if (!l2_buffer) {
        pmsis_exit(-1);
    }

#ifdef ESPNET_STARTUP_TEST
    printf("ESPNET STARTUP PASS: camera, streamer, five graphs and workspace allocated\n");
    pmsis_exit(0);
#endif
#ifdef TINY_RACER_PARITY_TEST
    run_parity_test();
#else
    trace_init();

    VERBOSE_PRINT("\n\t *** Initialization done ***\n\n");

    uart_protocol_start(&uart_protocol);
    cpx_start(&cpx);
    streamer_rx_start();
    camera_start(&camera);

    while (true) {
        camera_watchdog_poll(&camera);
        pi_yield();
    }
#endif
}

int main(void) {
    VERBOSE_PRINT("\n\n\t *** PMSIS Kickoff ***\n\n");
    return pmsis_kickoff((void *)main_task);
}
