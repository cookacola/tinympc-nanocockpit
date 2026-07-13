/*
 * main.c
 * Charles Chen <cc4919@columbia.edu>
 *
 * Cooperative gate8 deploy. Based on the NanoCockpit pulp-frontnet example, but
 * driving the gate8-async network through the forked async entry
 * network_run_async_cl, so the camera capture and the cluster inference coexist
 * under the lib/camera CO_FN framework. Pipeline per frame: capture and crop to
 * 160x160 done by lib/camera, resize to 96x160, run gate8, dequant to 8 corners,
 * send a gate8 message to the STM32 over UART. No Wi-Fi streamer, send only.
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

#include "config.h"
#include "coroutine.h"
#include "camera.h"
#include "cluster.h"
#include "debug.h"
#include "soc.h"
#include "time.h"
#include "trace.h"
#include "uart.h"
#include "uart_protocol.h"    // state_msg_t / tof_msg_t / inference_stamped_msg_t
#include "cpx/cpx.h"          // CPX transport for the WiFi streamer
#include "streamer.h"         // camera-frame WiFi streamer (for host corner viewer)
#include "mem.h"
#include "network.h"          // gate8-async API, network_run_async_cl

#include <pmsis.h>
#include <bsp/ram.h>
#include "crc32.h"
#include <stdbool.h>
#include <string.h>

#define IMG_W        160
#define IMG_H_CAM    160          // cropped sensor frame from lib/camera
#define IMG_H_NET    96           // net rows, 15360/160
#define N_CORNERS    8            // network output count
#define L2_BUF_SIZE  380000       // net input, scratch and output

// 1 = print per-inference corners over JTAG. Set 0 for a flash-boot deploy:
// there is no console then and printf can pollute the gate8 UART.
#define GATE8_DEBUG_PRINT 1

/* FLOAT = INT*EPS + BIAS. Order TL,TR,BR,BL. From gate8-async output_dequant. */
#define GATE8_EPS  1.61093718e-04f
static const float GATE8_BIAS[N_CORNERS] = {
  0.767035f, 0.421470f, 0.818451f, 0.712229f,
  0.845009f, 0.705781f, 0.806453f, 0.380487f
};

#define GATE8_MSG_HEADER "\x90\x19\x8\x33"
typedef struct __attribute__((packed)) {
  uint32_t stm32_timestamp;     // 0 in v1, no state forward
  float    corner[N_CORNERS];   // dequantized corners, pixel-scale image coords
} gate8_payload_t;
typedef struct __attribute__((packed)) {
  uint8_t  header[4];
  gate8_payload_t p;
  uint32_t checksum;            // CRC32 over header+payload
} gate8_msg_t;

static uart_t      uart;
static camera_t    camera;
static pi_device_t cluster;     // shared cluster, opened once, reused by the net
static void       *l2_buffer;   // net input, scratch and output
static size_t      l2_buffer_size;
static PI_L2 gate8_msg_t latest_msg;

// WiFi image streamer (for tools/gate8_corner_viewer.py). Runs alongside the onboard
// gate8 inference: the STM32 still gets corners over UART; the streamer only ships the
// raw camera frame over CPX so the host can re-run the net and overlay corners. The
// state/tof/inference metadata is unused here (no STM32->GAP8 link in this app), so it
// is left zeroed -- the host viewer recomputes corners from the image.
static cpx_t      cpx;
static streamer_t streamer;
static PI_FC_L1 state_msg_t latest_state;              // zeroed (no incoming state)
static PI_FC_L1 tof_msg_t   latest_tof;                // zeroed (no incoming tof)
static PI_L2 inference_stamped_msg_t latest_inference; // zeroed (corners done host-side)
static PI_FC_L1 co_fn_ctx_t streamer_rx_ctx;

typedef struct { uint32_t stm32_timestamp; } inference_args_t;
static PI_FC_L1 co_fn_ctx_t inference_ctx;

/* Vertical INTER_AREA resize 160 -> 96, width unchanged. scale 160/96 = 5/3,
 * weights periodic over 3 out-rows per 5 in-rows, 32 blocks. Round-to-nearest,
 * matching cv2.resize INTER_AREA. */
static void resize_v_160_to_96(const uint8_t *in, uint8_t *out) {
  for (int k = 0; k < 32; k++) {
    const uint8_t *i0=in+(5*k+0)*IMG_W, *i1=in+(5*k+1)*IMG_W, *i2=in+(5*k+2)*IMG_W;
    const uint8_t *i3=in+(5*k+3)*IMG_W, *i4=in+(5*k+4)*IMG_W;
    uint8_t *o0=out+(3*k+0)*IMG_W, *o1=out+(3*k+1)*IMG_W, *o2=out+(3*k+2)*IMG_W;
    for (int c = 0; c < IMG_W; c++) {
      o0[c] = (uint8_t)((3*i0[c] + 2*i1[c]           + 2) / 5);
      o1[c] = (uint8_t)((1*i1[c] + 3*i2[c] + 1*i3[c] + 2) / 5);
      o2[c] = (uint8_t)((2*i3[c] + 3*i4[c]           + 2) / 5);
    }
  }
}

CO_FN_DECLARE(inference_task);

/* Runs once per camera frame, delivered by lib/camera as a cropped 160x160 frame.
 * Waits for the previous inference to finish using l2_buffer, resizes this frame
 * into the net input, then launches inference. The frame is consumed by the time
 * this returns, so lib/camera can recycle it while inference runs async. */
CO_FN_BEGIN(camera_callback, frame_t *, camera_frame)
{
  static PI_FC_L1 bool started = false;
  static PI_FC_L1 inference_args_t iargs;
  static PI_FC_L1 co_event_t inference_done;
  static PI_FC_L1 co_event_t streamer_tx_done;

  if (started) {
    while (!co_event_is_done(&inference_done)) {
      CO_WAIT(&inference_done);
    }
    while (!co_event_is_done(&streamer_tx_done)) {
      CO_WAIT(&streamer_tx_done);
    }
  }
  started = true;

  // Resize copies the frame into l2_buffer first, so inference (on l2_buffer) and the
  // streamer (on the original camera_frame) read disjoint buffers -- no conflict.
  resize_v_160_to_96(camera_frame->buffer, (uint8_t *)l2_buffer);

  iargs.stm32_timestamp = 0;
  co_fn_push_start(&inference_ctx, inference_task, &iargs, co_event_init(&inference_done));

  // Ship the full 160x160 frame over WiFi. Wait for completion before returning so the
  // frame isn't recycled mid-send (metadata is unused -> zeroed statics).
  streamer_send_frame_async(&streamer, camera_frame,
                            &latest_state, 0, &latest_tof, 0, &latest_inference,
                            co_event_init(&streamer_tx_done));
  CO_WAIT(&streamer_tx_done);
}
CO_FN_END()

CO_FN_BEGIN(inference_task, inference_args_t *, args)
{
  static PI_FC_L1 co_event_t done;
  static PI_FC_L1 float corners[N_CORNERS];

  trace_set(TRACE_USER_0, true);
  network_run_async_cl(l2_buffer, l2_buffer_size, l2_buffer, 0, 1, &cluster, co_event_init(&done));
  CO_WAIT(&done);
  trace_set(TRACE_USER_0, false);

  const int32_t *raw = (const int32_t *) l2_buffer;
  for (int i = 0; i < N_CORNERS; i++) {
    corners[i] = raw[i] * GATE8_EPS + GATE8_BIAS[i];
  }

  memcpy(latest_msg.header, GATE8_MSG_HEADER, 4);
  latest_msg.p.stm32_timestamp = args->stm32_timestamp;
  memcpy(latest_msg.p.corner, corners, sizeof(latest_msg.p.corner));
  latest_msg.checksum = crc32CalculateBuffer(&latest_msg, sizeof(latest_msg) - sizeof(latest_msg.checksum));

  uart_write_async(&uart, &latest_msg, sizeof(latest_msg), co_event_init(&done));
  CO_WAIT(&done);

#if GATE8_DEBUG_PRINT
  static PI_FC_L1 uint32_t n = 0;
  printf("gate8[%lu]: %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f\n", n,
         corners[0], corners[1], corners[2], corners[3],
         corners[4], corners[5], corners[6], corners[7]);
  n++;
#endif
}
CO_FN_END()

// Drain inbound streamer buffers (the viewer's per-frame replies), so CPX buffers
// don't fill and stall the stream. In onboard-inference mode the offboard-inference
// relay is compiled out; we just receive and mark the frame completed for RTT stats.
CO_FN_DECLARE(streamer_rx_task);

static void streamer_rx_start(void) {
  co_fn_push_start(&streamer_rx_ctx, streamer_rx_task, NULL, NULL);
}

CO_FN_BEGIN(streamer_rx_task, void *, arg)
{
  static PI_L2    offboard_buffer_t offboard_buffer;
  static PI_FC_L1 streamer_buffer_t offboard_buffer_rx;
  static PI_FC_L1 co_event_t done_task;

  while (true) {
    streamer_buffer_init(&offboard_buffer_rx, &offboard_buffer, sizeof(offboard_buffer));
    streamer_receive_buffer_async(&streamer, &offboard_buffer_rx, co_event_init(&done_task));
    CO_WAIT(&done_task);

    if (offboard_buffer_rx.type != STREAMER_TYPE_INFERENCE) {
      continue;
    }
    streamer_stats_frame_completed(&streamer, &offboard_buffer.stats);
  }
}
CO_FN_END()

static void main_task(void) {
  soc_init();
  uart_init(&uart);
  camera_init(&camera, camera_callback);

  // Streamer owns the camera frame buffers (so it can hold one while sending), so use
  // streamer_alloc_frames instead of camera_init_frames_alloc.
  cpx_init(&cpx);
  streamer_init(&streamer, &camera, &cpx);
  streamer_alloc_frames(&streamer, &camera);

  cluster_init(&cluster);

  mem_init();
  network_initialize();

  l2_buffer_size = L2_BUF_SIZE;
  l2_buffer = pi_l2_malloc(l2_buffer_size);
  if (!l2_buffer) {
    printf("ERROR: L2 alloc failed\n");
    pmsis_exit(-1);
  }

  trace_init();
#if GATE8_DEBUG_PRINT
  printf("gate8 deploy+stream: init done, starting camera\n");
#endif
  camera_start(&camera);
  cpx_start(&cpx);
  streamer_rx_start();

  while (true) {
    pi_yield();
  }

  pmsis_exit(0);
}

int main(void) {
  return pmsis_kickoff((void *)main_task);
}
