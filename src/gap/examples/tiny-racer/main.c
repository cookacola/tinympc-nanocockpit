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
#include "coroutine.h"
#include "cpx/cpx.h"
#include "crc32.h"
#include "debug.h"
#include "gap8_perception_output.h"
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

#define IMAGE_WIDTH              160
#define IMAGE_HEIGHT             160
#define NETWORK_INPUT_HEIGHT     120
#define NETWORK_INPUT_TOP        20
#define NETWORK_INPUT_BYTES      (IMAGE_WIDTH * NETWORK_INPUT_HEIGHT)

#define CORNER_COUNT             4
#define CORNER_COORD_COUNT       (2 * CORNER_COUNT)
/* Layer 2 peaks at 76,800 B input + 115,200 B output + 768 B weights. */
#define NETWORK_L2_WORKSPACE_SIZE 200000

#ifdef TINY_RACER_PARITY_TEST
#endif

_Static_assert(CAMERA_CROP_WIDTH == IMAGE_WIDTH,
               "Tiny Racer expects 160-pixel camera rows");
_Static_assert(CAMERA_CROP_HEIGHT == IMAGE_HEIGHT,
               "Tiny Racer expects a 160x160 camera crop");
_Static_assert(GAP8_OUTPUT_BYTES ==
                   PERCEPTION_GRID_WIDTH * PERCEPTION_GRID_HEIGHT *
                       PERCEPTION_OUTPUT_CHANNELS,
               "Unexpected sequential network output layout");

static uart_t uart;
static uart_protocol_t uart_protocol;
static camera_t camera;
#if defined(STREAMER_ENABLE)
static cpx_t cpx;
static streamer_t streamer;
#endif
static pi_device_t cluster;

static PI_FC_L1 state_msg_t latest_state;
static PI_FC_L1 uint32_t state_timestamp;
static PI_FC_L1 tof_msg_t latest_tof;
static PI_FC_L1 uint32_t tof_timestamp;
#if defined(STREAMER_ENABLE)
static PI_L2 inference_stamped_msg_t latest_inference;
static PI_L2 streamer_sequential_output_t latest_sequential;
#endif

static void *l2_buffer;
static size_t l2_buffer_size;

static PI_FC_L1 co_fn_ctx_t inference_ctx;
#if defined(STREAMER_ENABLE)
static PI_FC_L1 co_fn_ctx_t streamer_rx_ctx;
#endif

typedef struct {
    uint32_t stm32_timestamp;
    uint32_t input_crc32;
    frame_t *camera_frame;
} inference_args_t;

#ifdef TINY_RACER_PARITY_TEST
/*
 * Deterministic, one-shot DORY replay for comparing against the host ONNX
 * graph. PARITY_INPUT supplies this exact 160x120 uint8 tensor through
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

    printf("PARITY output_crc32=%08lx\n",
           (unsigned long)crc32CalculateBuffer(l2_buffer,
                                               GAP8_OUTPUT_BYTES));
    pmsis_exit(0);
}
#endif

CO_FN_DECLARE(inference_task);
#if defined(STREAMER_ENABLE)
CO_FN_DECLARE(streamer_rx_task);
#endif

/*
 * The network sees the middle 160x120 portion of the 160x160 camera frame.
 * Copy it before inference because the DORY workspace overwrites its input.
 */
static void copy_network_input(const frame_t *frame) {
    memcpy(l2_buffer,
           frame->buffer + NETWORK_INPUT_TOP * IMAGE_WIDTH,
           IMAGE_WIDTH * NETWORK_INPUT_HEIGHT);
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
    return y * IMAGE_WIDTH + x;
}

CO_FN_BEGIN(camera_callback, frame_t *, camera_frame)
{
    static PI_FC_L1 inference_args_t inference_args;
    static PI_FC_L1 co_event_t inference_done;
#if defined(STREAMER_ENABLE)
    static PI_FC_L1 co_event_t streamer_tx_done;
#endif

    copy_network_input(camera_frame);

    inference_args = (inference_args_t) {
        .stm32_timestamp = latest_state.timestamp,
        .input_crc32 = crc32CalculateBuffer(l2_buffer, NETWORK_INPUT_BYTES),
        .camera_frame = camera_frame,
    };
    co_fn_push_start(&inference_ctx, inference_task, &inference_args,
                     co_event_init(&inference_done));
    CO_WAIT(&inference_done);

#if defined(STREAMER_ENABLE)
    /* The streamer reads the camera buffer asynchronously, so retain this
     * frame until CPX has sent it.  This mode intentionally depends on a
     * healthy NINA/host link; it is not enabled for flight firmware. */
    streamer_send_frame_async(
        &streamer,
        camera_frame,
        &latest_state, state_timestamp,
        &latest_tof, tof_timestamp,
        &latest_inference,
        co_event_init(&streamer_tx_done)
    );
    CO_WAIT(&streamer_tx_done);
#endif
}
CO_FN_END()

CO_FN_BEGIN(inference_task, inference_args_t *, inference_args)
{
    static PI_FC_L1 co_event_t network_done;
    static PI_FC_L1 float corners[CORNER_COORD_COUNT];
    static PI_FC_L1 float corner_peaks[CORNER_COUNT];
    static PI_FC_L1 float corner_ambiguity[CORNER_COUNT];
    static PI_FC_L1 float clearance_m[CORNER_COUNT];
    static PI_FC_L1 float clearance_confidence[CORNER_COUNT];
    static PI_FC_L1 frame_t *camera_frame;
    static PI_FC_L1 int gate_valid;
    static PI_FC_L1 uint8_t gate_rejection_reason;
    static PI_FC_L1 uint8_t confident_corner_mask;
    static PI_FC_L1 uint16_t sequential_sequence;
    static PI_FC_L1 sequential_obstacle_msg_t sequential_uart;
    static PI_FC_L1 co_event_t uart_tx_done;

    camera_frame = inference_args->camera_frame;

    trace_set(TRACE_USER_0, true);
    network_run_async_cl(l2_buffer, l2_buffer_size, l2_buffer, 0, 1,
                         &cluster, co_event_init(&network_done));
    CO_WAIT(&network_done);
    trace_set(TRACE_USER_0, false);

    gap8_decode_sequential_output((const uint8_t *)l2_buffer, corners,
                                  corner_peaks, corner_ambiguity,
                                  clearance_m, clearance_confidence);
    gate_valid = gap8_validate_gate_candidate(corners, corner_peaks,
                                              corner_ambiguity,
                                              &gate_rejection_reason,
                                              &confident_corner_mask);

    for (int corner = 0; corner < CORNER_COUNT; ++corner) {
        corners[2 * corner + 1] += NETWORK_INPUT_TOP;
        if (gate_valid && (confident_corner_mask & (1U << corner))) {
            draw_corner_marker(camera_frame->buffer,
                               corners[2 * corner],
                               corners[2 * corner + 1]);
        }
    }

#if defined(STREAMER_ENABLE)
    latest_sequential = (streamer_sequential_output_t) {
        .gate_valid = gate_valid ? 1 : 0,
        .gate_rejection_reason = gate_rejection_reason,
        .confident_corner_mask = confident_corner_mask,
        .input_crc32 = inference_args->input_crc32,
        .output_crc32 = crc32CalculateBuffer(l2_buffer, GAP8_OUTPUT_BYTES),
    };
    memcpy(latest_sequential.corner_peak_scores, corner_peaks,
           sizeof(corner_peaks));
    memcpy(latest_sequential.corner_ambiguity, corner_ambiguity,
           sizeof(corner_ambiguity));
    memcpy(latest_sequential.clearance_m, clearance_m,
           sizeof(clearance_m));
    memcpy(latest_sequential.clearance_confidence, clearance_confidence,
           sizeof(clearance_confidence));
    streamer_set_sequential_output(&streamer, &latest_sequential);

    latest_inference = (inference_stamped_msg_t) {
        .stm32_timestamp = inference_args->stm32_timestamp,
        /* Always expose heatmap candidates to the diagnostic streamer. The
         * explicit gate_valid field remains the acceptance decision. */
        .x = encode_corner(corners[0], corners[1]),
        .y = encode_corner(corners[2], corners[3]),
        .z = encode_corner(corners[4], corners[5]),
        .phi = encode_corner(corners[6], corners[7]),
    };
#endif

    ++sequential_sequence;
    if (sequential_sequence == 0) ++sequential_sequence;
    sequential_uart = (sequential_obstacle_msg_t) {
        .stm32_timestamp = inference_args->stm32_timestamp,
        .sequence = sequential_sequence,
        .gate_valid = gate_valid ? 1 : 0,
    };
    memcpy(sequential_uart.clearance_m, clearance_m, sizeof(clearance_m));
    memcpy(sequential_uart.confidence, clearance_confidence,
           sizeof(clearance_confidence));
    uart_protocol_send_sequential_async(&uart_protocol, &sequential_uart,
                                        co_event_init(&uart_tx_done));
    CO_WAIT(&uart_tx_done);

    /*
     * Gate-corner visualization remains on the diagnostic CPX stream. The
     * four fixed-normal clearance/confidence pairs are sent to STM32 over the
     * CRC-protected sequential UART packet above.
     */
}
CO_FN_END()

#if defined(STREAMER_ENABLE)
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
#endif

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

    uart_init(&uart);
    uart_protocol_init(&uart_protocol, &uart, uart_callback);

    camera_init(&camera, camera_callback);

#if defined(STREAMER_ENABLE)
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

#ifdef TINY_RACER_PARITY_TEST
    run_parity_test();
#endif

    trace_init();

    VERBOSE_PRINT("\n\t *** Initialization done ***\n\n");

    uart_protocol_start(&uart_protocol);
#if defined(STREAMER_ENABLE)
    cpx_start(&cpx);
    streamer_rx_start();
#endif
    camera_start(&camera);

    while (true) {
        camera_watchdog_poll(&camera);
        pi_yield();
    }
}

int main(void) {
    VERBOSE_PRINT("\n\n\t *** PMSIS Kickoff ***\n\n");
    return pmsis_kickoff((void *)main_task);
}
