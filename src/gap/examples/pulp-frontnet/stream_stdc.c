/*
 * Streaming-only HM01B0 application for laptop STDC inference.
 *
 * GAP8 captures the normal 160x160 camera frame and sends only the central
 * 160x120 region consumed by shared_dory_frozen_real_v1. No network graph,
 * cluster inference, model weights, or inference workspace is linked or
 * allocated in this build.
 */

#include "camera.h"
#include "coroutine.h"
#include "cpx/cpx.h"
#include "soc.h"
#include "streamer.h"
#include "trace.h"
#include "uart.h"
#include "uart_protocol.h"

#include <pmsis.h>
#include <string.h>

#define STDC_STREAM_WIDTH 160
#define STDC_STREAM_HEIGHT 120
#define STDC_STREAM_Y_OFFSET 20

static uart_t uart;
static uart_protocol_t uart_protocol;
static camera_t camera;
static cpx_t cpx;
static streamer_t streamer;
static PI_FC_L1 state_msg_t latest_state;
static PI_FC_L1 tof_msg_t latest_tof;
static PI_FC_L1 uint32_t latest_state_rx_us;
static PI_FC_L1 uint32_t latest_tof_rx_us;
static PI_L2 inference_stamped_msg_t no_onboard_inference;
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

CO_FN_BEGIN(camera_callback, frame_t *, camera_frame)
{
  static PI_FC_L1 co_event_t stream_done;
  streamer_send_frame_region_async(
      &streamer, camera_frame,
      STDC_STREAM_Y_OFFSET, 0, STDC_STREAM_WIDTH, STDC_STREAM_HEIGHT,
      &latest_state, latest_state_rx_us,
      &latest_tof, latest_tof_rx_us,
      &no_onboard_inference,
      co_event_init(&stream_done));
  CO_WAIT(&stream_done);
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

static void main_task(void) {
  soc_init();
  uart_init(&uart);
  uart_protocol_init(&uart_protocol, &uart, uart_callback);
  camera_init(&camera, camera_callback);
  cpx_init(&cpx);
  streamer_init(&streamer, &camera, &cpx);
  streamer_alloc_frames(&streamer, &camera);
  trace_init();

  uart_protocol_start(&uart_protocol);
  camera_start(&camera);
  cpx_start(&cpx);
  co_fn_push_start(&streamer_rx_ctx, streamer_rx_task, NULL, NULL);

  while (true) {
    camera_watchdog_poll(&camera);
    pi_yield();
  }
}

int main(void) {
  return pmsis_kickoff((void *)main_task);
}
