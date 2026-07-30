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

#define CORNER_COUNT             4
#define CORNER_COORD_COUNT       (2 * CORNER_COUNT)
#define CORNER_HEATMAP_BYTES     4800
#define DANGER_MAP_WIDTH         10
#define DANGER_MAP_HEIGHT        8
#define DANGER_MAP_BYTES         (DANGER_MAP_WIDTH * DANGER_MAP_HEIGHT)

/* Quantization parameters and operating threshold from this network's
 * manifest.json. The danger head emits uint8 quantized logits, not direct
 * probabilities. */
#define DANGER_QUANT_EPSILON     0.34079399704933167f
#define DANGER_QUANT_OFFSET      16.512078149414062f
#define DANGER_QUANT_BIAS        -0.012773600406944752f
#define DANGER_PROBABILITY_THRESHOLD 0.07227228581905365f
#define DANGER_MAX_DARKENING     0.75f

#define NETWORK_L2_WORKSPACE_SIZE 180000

#ifdef TINY_RACER_PARITY_TEST
#define NETWORK_INPUT_BYTES       (IMAGE_WIDTH * NETWORK_INPUT_HEIGHT)
#endif

_Static_assert(CAMERA_CROP_WIDTH == IMAGE_WIDTH,
               "Tiny Racer expects 160-pixel camera rows");
_Static_assert(CAMERA_CROP_HEIGHT == IMAGE_HEIGHT,
               "Tiny Racer expects a 160x160 camera crop");
_Static_assert(GAP8_OUTPUT_BYTES ==
                   CORNER_HEATMAP_BYTES + DANGER_MAP_BYTES,
               "Unexpected Tiny Racer network output layout");

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

static PI_FC_L1 co_fn_ctx_t inference_ctx;
static PI_FC_L1 co_fn_ctx_t streamer_rx_ctx;

typedef struct {
    uint32_t stm32_timestamp;
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

    printf("PARITY corner_crc32=%08lx danger_crc32=%08lx output_crc32=%08lx\n",
           (unsigned long)crc32CalculateBuffer(l2_buffer,
                                               CORNER_HEATMAP_BYTES),
           (unsigned long)crc32CalculateBuffer(
               (const uint8_t *)l2_buffer + CORNER_HEATMAP_BYTES,
               DANGER_MAP_BYTES),
           (unsigned long)crc32CalculateBuffer(l2_buffer,
                                               GAP8_OUTPUT_BYTES));
    pmsis_exit(0);
}
#endif

CO_FN_DECLARE(inference_task);
CO_FN_DECLARE(streamer_rx_task);

/*
 * The network sees the middle 160x120 portion of the 160x160 camera frame.
 * Copy it before inference because the DORY workspace overwrites its input.
 */
static void copy_network_input(const frame_t *frame) {
    memcpy(l2_buffer,
           frame->buffer + NETWORK_INPUT_TOP * IMAGE_WIDTH,
           IMAGE_WIDTH * NETWORK_INPUT_HEIGHT);
}

/*
 * The danger head produces one quantized-logit uint8 value for each cell of
 * a 10x8 grid. Dequantize it into a probability, then darken each
 * corresponding 16x15 camera region by at most 75 percent above the
 * calibrated danger threshold.
 * The top and bottom 20 camera rows are left alone because the network did
 * not observe them.
 */
static void overlay_danger_map(uint8_t *frame, const uint8_t *danger_map) {
    const int cell_width = IMAGE_WIDTH / DANGER_MAP_WIDTH;
    const int cell_height = NETWORK_INPUT_HEIGHT / DANGER_MAP_HEIGHT;

    for (int map_y = 0; map_y < DANGER_MAP_HEIGHT; ++map_y) {
        for (int map_x = 0; map_x < DANGER_MAP_WIDTH; ++map_x) {
            const float quantized_danger =
                (float)danger_map[map_y * DANGER_MAP_WIDTH + map_x];
            const float logit = quantized_danger * DANGER_QUANT_EPSILON
                              - DANGER_QUANT_OFFSET + DANGER_QUANT_BIAS;
            const float probability = 1.0f / (1.0f + expf(-logit));
            const float danger_strength = probability <= DANGER_PROBABILITY_THRESHOLD
                ? 0.0f
                : (probability - DANGER_PROBABILITY_THRESHOLD)
                    / (1.0f - DANGER_PROBABILITY_THRESHOLD);
            const uint16_t shade = (uint16_t)(255.0f * DANGER_MAX_DARKENING
                                               * danger_strength + 0.5f);
            const uint16_t scale = 255u - shade;
            const int first_x = map_x * cell_width;
            const int first_y =
                NETWORK_INPUT_TOP + map_y * cell_height;

            for (int y = first_y; y < first_y + cell_height; ++y) {
                uint8_t *row = frame + y * IMAGE_WIDTH;
                for (int x = first_x; x < first_x + cell_width; ++x) {
                    row[x] =
                        (uint8_t)(((uint16_t)row[x] * scale + 127u) / 255u);
                }
            }
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
    return y * IMAGE_WIDTH + x;
}

CO_FN_BEGIN(camera_callback, frame_t *, camera_frame)
{
    static PI_FC_L1 inference_args_t inference_args;
    static PI_FC_L1 co_event_t inference_done;
    static PI_FC_L1 co_event_t streamer_tx_done;

    copy_network_input(camera_frame);

    inference_args = (inference_args_t) {
        .stm32_timestamp = latest_state.timestamp,
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
    static PI_FC_L1 float corners[CORNER_COORD_COUNT];
    static PI_FC_L1 uint8_t corner_confidence[CORNER_COUNT];
    static PI_FC_L1 frame_t *camera_frame;
    static PI_FC_L1 const uint8_t *danger_map;

    camera_frame = inference_args->camera_frame;

    trace_set(TRACE_USER_0, true);
    network_run_async_cl(l2_buffer, l2_buffer_size, l2_buffer, 0, 1,
                         &cluster, co_event_init(&network_done));
    CO_WAIT(&network_done);
    trace_set(TRACE_USER_0, false);

    gap8_decode_corner_argmax((const uint8_t *)l2_buffer, corners,
                              corner_confidence);
    (void)gap8_validate_or_recover_gate(corners, corner_confidence);

    danger_map = (const uint8_t *)l2_buffer + CORNER_HEATMAP_BYTES;
    overlay_danger_map(camera_frame->buffer, danger_map);
    for (int corner = 0; corner < CORNER_COUNT; ++corner) {
        draw_corner_marker(camera_frame->buffer,
                           corners[2 * corner],
                           corners[2 * corner + 1]);
    }

    latest_inference = (inference_stamped_msg_t) {
        .stm32_timestamp = inference_args->stm32_timestamp,
        .x = encode_corner(corners[0], corners[1]),
        .y = encode_corner(corners[2], corners[3]),
        .z = encode_corner(corners[4], corners[5]),
        .phi = encode_corner(corners[6], corners[7]),
    };

    /*
     * UART inference transmission is intentionally disabled during neural
     * network bring-up. The annotated frame and temporary corner metadata are
     * sent only through the CPX streamer.
     */
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

    uart_init(&uart);
    uart_protocol_init(&uart_protocol, &uart, uart_callback);

    camera_init(&camera, camera_callback);

    cpx_init(&cpx);
    streamer_init(&streamer, &camera, &cpx);
    streamer_alloc_frames(&streamer, &camera);

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
    cpx_start(&cpx);
    streamer_rx_start();
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
