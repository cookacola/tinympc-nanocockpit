/*
 * Neural camera pipeline for the GAP8 AI-deck.
 *
 * Each camera frame is copied into the selected network input, inferred on
 * the cluster, and sent to the STM32 as gate corners and (when available) a
 * compact perception map. UART owns a dedicated L2 buffer while transmitting.
 */

#include "config.h"
#include "camera.h"
#include "cluster.h"
#include "coroutine.h"
#include "crc32.h"
#include "mem.h"
#include "network.h"
#include "soc.h"
#include "trace.h"
#include "uart.h"
#ifdef GAP8_MULTITASK_NETWORK
#include "gap8_perception_output.h"
#include "perception_map_uart.h"
#endif
#ifdef STREAMER_ENABLE
#include "cpx/cpx.h"
#include "streamer.h"
#include "uart_protocol.h"
#endif

#include <pmsis.h>
#include <stdbool.h>
#include <string.h>

#define IMG_W 160
#define IMG_H_CAM 160
#ifdef GAP8_STDC_PAIR_NETWORK
#define IMG_H_NET 120
#elif defined(GAP8_MULTITASK_NETWORK)
#define IMG_H_NET 160
#else
#define IMG_H_NET 96
#endif
#define N_CORNERS 8
#ifdef GAP8_STDC_SHARED_NETWORK
#define L2_BUF_SIZE 180000
#else
#define L2_BUF_SIZE 160000
#endif

#define GATE8_EPS 1.61093718e-04f
static const float GATE8_BIAS[N_CORNERS] = {
  0.767035f, 0.421470f, 0.818451f, 0.712229f,
  0.845009f, 0.705781f, 0.806453f, 0.380487f
};

#define GATE8_MSG_HEADER "\x90\x19\x8\x33"
typedef struct __attribute__((packed)) {
  uint32_t stm32_timestamp;
  float corner[N_CORNERS];
} gate8_payload_t;
typedef struct __attribute__((packed)) {
  uint8_t header[4];
  gate8_payload_t p;
  uint32_t checksum;
} gate8_msg_t;
_Static_assert(sizeof(gate8_msg_t) == 44, "gate packet ABI changed");

static uart_t uart;
static camera_t camera;
static pi_device_t cluster;
static void *l2_buffer;
static size_t l2_buffer_size;
static PI_L2 gate8_msg_t gate8_tx_msg;
static PI_L2 gate8_msg_t gate8_uart_msg;
static PI_FC_L1 volatile bool gate8_tx_pending;
static PI_FC_L1 volatile bool uart_tx_busy;
static PI_FC_L1 volatile bool inference_busy;

#ifdef GAP8_MULTITASK_NETWORK
static PI_L2 uint8_t obstacle_presence_map_20[400];
static PI_L2 uint8_t inverse_range_map_20[400];
static PI_L2 uint8_t uncertainty_map_20[400];
static PI_L2 uint8_t gate_opening_map_20[400];
static PI_L2 uint8_t corner_confidence[4];
static PI_L2 perception_map_payload_t perception_map_tx_payload;
static PI_FC_L1 volatile bool perception_map_tx_pending;
static PI_FC_L1 uint16_t perception_map_sequence = 1;
#endif

#ifdef STREAMER_ENABLE
static cpx_t cpx;
static streamer_t streamer;
static uart_protocol_t uart_protocol;
static PI_FC_L1 state_msg_t latest_state;
static PI_FC_L1 tof_msg_t latest_tof;
static PI_FC_L1 uint32_t latest_state_rx_us;
static PI_FC_L1 uint32_t latest_tof_rx_us;
static PI_L2 inference_stamped_msg_t latest_stream_inference;
static PI_FC_L1 co_fn_ctx_t streamer_rx_ctx;

CO_FN_BEGIN(uart_callback, uart_msg_t *, message)
{
  if (memcmp(message->header, UART_STATE_MSG_HEADER, UART_HEADER_LENGTH) == 0) {
    latest_state = message->state;
    latest_state_rx_us = message->recv_timestamp;
  } else if (memcmp(message->header, UART_TOF_MSG_HEADER,
                    UART_HEADER_LENGTH) == 0) {
    latest_tof = message->tof;
    latest_tof_rx_us = message->recv_timestamp;
  }
}
CO_FN_END()

CO_FN_BEGIN(streamer_rx_task, void *, arg)
{
  static PI_L2 offboard_buffer_t offboard_buffer;
  static PI_FC_L1 streamer_buffer_t received;
  static PI_FC_L1 co_event_t done;
  (void)arg;
  while (true) {
    streamer_buffer_init(&received, &offboard_buffer, sizeof(offboard_buffer));
    streamer_receive_buffer_async(&streamer, &received, co_event_init(&done));
    CO_WAIT(&done);
    streamer_stats_frame_completed(&streamer, &offboard_buffer.stats);
  }
}
CO_FN_END()
#endif

typedef struct {
  uint32_t gap8_timestamp;
} inference_args_t;
static PI_FC_L1 co_fn_ctx_t inference_ctx;
CO_FN_DECLARE(inference_task);

static void resize_v_160_to_96(const uint8_t *in, uint8_t *out) {
  for (int k = 0; k < 32; ++k) {
    const uint8_t *i0 = in + (5 * k + 0) * IMG_W;
    const uint8_t *i1 = in + (5 * k + 1) * IMG_W;
    const uint8_t *i2 = in + (5 * k + 2) * IMG_W;
    const uint8_t *i3 = in + (5 * k + 3) * IMG_W;
    const uint8_t *i4 = in + (5 * k + 4) * IMG_W;
    uint8_t *o0 = out + (3 * k + 0) * IMG_W;
    uint8_t *o1 = out + (3 * k + 1) * IMG_W;
    uint8_t *o2 = out + (3 * k + 2) * IMG_W;
    for (int c = 0; c < IMG_W; ++c) {
      o0[c] = (uint8_t)((3 * i0[c] + 2 * i1[c] + 2) / 5);
      o1[c] = (uint8_t)((i1[c] + 3 * i2[c] + i3[c] + 2) / 5);
      o2[c] = (uint8_t)((2 * i3[c] + 3 * i4[c] + 2) / 5);
    }
  }
}

static void vision_uart_done(void *arg) {
  (void)arg;
  uart_tx_busy = false;
}

static void vision_uart_service(void) {
  static pi_task_t done_task;
  if (uart_tx_busy) return;

  if (gate8_tx_pending) {
    memcpy(&gate8_uart_msg, &gate8_tx_msg, sizeof(gate8_uart_msg));
    gate8_tx_pending = false;
    uart_tx_busy = true;
    uart_write_async(&uart, &gate8_uart_msg, sizeof(gate8_uart_msg),
                     pi_task_callback(&done_task, vision_uart_done, NULL));
    return;
  }
#ifdef GAP8_MULTITASK_NETWORK
  if (perception_map_tx_pending) {
    perception_map_tx_pending = false;
    uart_tx_busy = true;
    perception_map_send_async(
        &uart, &perception_map_tx_payload,
        pi_task_callback(&done_task, vision_uart_done, NULL));
  }
#endif
}

CO_FN_BEGIN(camera_callback, frame_t *, camera_frame)
{
  static PI_FC_L1 bool started;
  static PI_FC_L1 inference_args_t args;
  static PI_FC_L1 co_event_t inference_done;
#ifdef STREAMER_ENABLE
  static PI_FC_L1 co_event_t streamer_tx_done;
#endif

  if (started) {
    while (!co_event_is_done(&inference_done)) CO_WAIT(&inference_done);
  }
#ifdef GAP8_STDC_PAIR_NETWORK
  memcpy(l2_buffer, camera_frame->buffer + 20 * IMG_W, IMG_W * IMG_H_NET);
#elif defined(GAP8_MULTITASK_NETWORK)
  memcpy(l2_buffer, camera_frame->buffer, IMG_W * IMG_H_NET);
#else
  resize_v_160_to_96(camera_frame->buffer, (uint8_t *)l2_buffer);
#endif
  args.gap8_timestamp = camera_frame->frame_timestamp;
  inference_busy = true;
  co_fn_push_start(&inference_ctx, inference_task, &args,
                   co_event_init(&inference_done));
  started = true;

#ifdef STREAMER_ENABLE
  streamer_send_frame_region_async(
      &streamer, camera_frame, 20, 0, IMG_W, IMG_H_NET,
      &latest_state, latest_state_rx_us, &latest_tof, latest_tof_rx_us,
      &latest_stream_inference, co_event_init(&streamer_tx_done));
  CO_WAIT(&streamer_tx_done);
#endif
}
CO_FN_END()

CO_FN_BEGIN(inference_task, inference_args_t *, args)
{
  static PI_FC_L1 co_event_t done;
  static PI_FC_L1 float corners[N_CORNERS];

  trace_set(TRACE_USER_0, true);
  network_run_async_cl(l2_buffer, l2_buffer_size, l2_buffer, 0, 1, &cluster,
                       co_event_init(&done));
  CO_WAIT(&done);
  trace_set(TRACE_USER_0, false);

#ifdef GAP8_MULTITASK_NETWORK
  gap8_decode_corner_argmax((const uint8_t *)l2_buffer, corners,
                            corner_confidence);
  (void)gap8_validate_or_recover_gate(corners, corner_confidence);
  gap8_pool_control_maps((const uint8_t *)l2_buffer, obstacle_presence_map_20,
                         inverse_range_map_20, uncertainty_map_20,
                         gate_opening_map_20);
  if (!perception_map_tx_pending) {
    perception_map_pack(&perception_map_tx_payload, obstacle_presence_map_20,
                        inverse_range_map_20, uncertainty_map_20,
                        gate_opening_map_20, args->gap8_timestamp, 0,
                        perception_map_sequence++);
    if (perception_map_sequence == 0) perception_map_sequence = 1;
    perception_map_tx_pending = true;
  }
#else
  const int32_t *raw = (const int32_t *)l2_buffer;
  for (int i = 0; i < N_CORNERS; ++i) {
    corners[i] = raw[i] * GATE8_EPS + GATE8_BIAS[i];
  }
#endif

  if (!gate8_tx_pending) {
    memcpy(gate8_tx_msg.header, GATE8_MSG_HEADER, sizeof(gate8_tx_msg.header));
    gate8_tx_msg.p.stm32_timestamp = 0;
    memcpy(gate8_tx_msg.p.corner, corners, sizeof(gate8_tx_msg.p.corner));
    gate8_tx_msg.checksum = crc32CalculateBuffer(
        &gate8_tx_msg, sizeof(gate8_tx_msg) - sizeof(gate8_tx_msg.checksum));
    gate8_tx_pending = true;
  }
  inference_busy = false;
}
CO_FN_END()

static void main_task(void) {
  soc_init();
  uart_init(&uart);
  camera_init(&camera, camera_callback);
#ifdef STREAMER_ENABLE
  uart_protocol_init(&uart_protocol, &uart, uart_callback);
  cpx_init(&cpx);
  streamer_init(&streamer, &camera, &cpx);
  streamer_alloc_frames(&streamer, &camera);
#else
  camera_init_frames_alloc(&camera);
#endif
  cluster_init(&cluster);
  mem_init();
  network_initialize();
  l2_buffer_size = L2_BUF_SIZE;
  l2_buffer = pi_l2_malloc(l2_buffer_size);
  if (!l2_buffer) {
    pmsis_exit(-1);
  }
  trace_init();
#ifdef STREAMER_ENABLE
  uart_protocol_start(&uart_protocol);
  cpx_start(&cpx);
  co_fn_push_start(&streamer_rx_ctx, streamer_rx_task, NULL, NULL);
#endif
  camera_start(&camera);

  while (true) {
    vision_uart_service();
    if (!inference_busy) camera_watchdog_poll(&camera);
    pi_yield();
  }
}

int main(void) {
  return pmsis_kickoff((void *)main_task);
}
