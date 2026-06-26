/*
 * main.c
 * Charles Chen <cc4919@columbia.edu>
 *
 * Preliminary v1 firmware. Blocking pure-producer loop: camera -> resize ->
 * gate8 inference -> dequant -> send corners to the STM32 over UART. No CO_FN.
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

#include "mem.h"
#include "network.h"          // gate8-dory network API
#include "config.h"           // HIMAX_FORMAT must be 2/HALF, plus CAMERA_CAPTURE_* and CAMERA_CROP_*
#include "camera/himax.h"     // himax_t driver
#include "camera.h"           // frame_t, only buffer and buffer_size used
#include "crc32.h"

#include <pmsis.h>
#include <bsp/ram.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

#define IMG_W            160
#define IMG_H_CAM        160          // cropped sensor frame
#define IMG_H_NET        96           // net rows, 15360/160
#define N_CORNERS        8            // network output count
#define L2_BUF_SIZE      380000       // same as gate8-dory main

// Diagnostic mode. 0 = camera capture, the real pipeline. 1 = camera init but
// no capture, memset input. 2 = no camera, memset input.
#define GATE8_CAM_MODE 0

/* FLOAT = INT*EPS + BIAS. Order TL,TR,BR,BL. From output_dequant.txt. */
#define GATE8_EPS  1.61093718e-04f
static const float GATE8_BIAS[N_CORNERS] = {
  0.767035f, 0.421470f, 0.818451f, 0.712229f,
  0.845009f, 0.705781f, 0.806453f, 0.380487f
};

#define GATE8_MSG_HEADER "\x90\x19\x8\x33"   // gate8 message header
typedef struct __attribute__((packed)) {
  uint32_t stm32_timestamp;     // 0 in v1, no state forward
  float    corner[N_CORNERS];   // dequantized corners, normalized image coords
} gate8_payload_t;
typedef struct __attribute__((packed)) {
  uint8_t  header[4];
  gate8_payload_t p;
  uint32_t checksum;            // CRC32 over header+payload
} gate8_msg_t;

/* Vertical INTER_AREA resize 160 -> 96, width unchanged. scale 160/96 = 5/3,
 * so weights are periodic over 3 out-rows per 5 in-rows, 32 blocks. The +2 then
 * divide-by-5 is round-to-nearest, matching cv2.resize INTER_AREA. */
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

/* Camera: lib/camera/himax driver, blocking, HALF 162x162 per config.h. */
static himax_t himax;

static int camera_start_blocking(void) {
  if (himax_init(&himax) != 0) return -1;   // open camera device and MCLK timer
  himax_configure(&himax);                  // registers from config.h
  // Do not leave streaming on. Toggle start/stop per capture so the camera uDMA
  // is idle during network_run.
  return 0;
}

/* One blocking capture into buf. Streaming is started for the capture and
 * stopped right after so the camera DMA does not run during inference. */
static void camera_capture_blocking(uint8_t *buf, size_t buf_size) {
  frame_t frame = { .buffer = buf, .buffer_size = buf_size };  // other fields unused
  pi_task_t done;
  // Arm the capture before starting the stream, per lib/camera.c. Starting first
  // with no buffer armed corrupts the CPI uDMA state.
  himax_capture_async(&himax, &frame, pi_task_block(&done));
  himax_start(&himax);
  pi_task_wait_on(&done);
  himax_stop(&himax);
}

/* Crop raw 162-wide sensor frame to 160x160, mirroring lib/camera.c. */
static void camera_crop_to_160(const uint8_t *raw, uint8_t *out160) {
  const int cap_w = CAMERA_CAPTURE_WIDTH;
  for (int r = 0; r < CAMERA_CROP_HEIGHT; r++) {
    const uint8_t *src = raw + (CAMERA_CROP_TOP + r) * cap_w + CAMERA_CROP_LEFT;
    memcpy(out160 + r * CAMERA_CROP_WIDTH, src, CAMERA_CROP_WIDTH);
  }
}

/* UART to STM32. */
static struct pi_device uart;

static int uart_open(void) {
  struct pi_uart_conf conf;
  pi_uart_conf_init(&conf);
  conf.baudrate_bps = 115200;               // match the STM32 side
  conf.enable_tx = 1;
  conf.enable_rx = 1;
  pi_open_from_conf(&uart, &conf);
  return pi_uart_open(&uart);
}

static void gate8_send(const float *corners, uint32_t ts) {
  static gate8_msg_t msg;
  memcpy(msg.header, GATE8_MSG_HEADER, 4);
  msg.p.stm32_timestamp = ts;
  memcpy(msg.p.corner, corners, sizeof(msg.p.corner));
  msg.checksum = crc32CalculateBuffer(&msg, sizeof(msg) - sizeof(msg.checksum));
  pi_uart_write(&uart, &msg, sizeof(msg));  // blocking
}

void main_task(void *arg) {
  printf("DBG: main_task start\n");
  mem_init();                  printf("DBG: mem_init ok\n");
  network_initialize();        printf("DBG: network_initialize ok\n");
#if GATE8_CAM_MODE <= 1
  if (camera_start_blocking()) { printf("ERROR: camera init failed\n"); pmsis_exit(-1); }
  printf("DBG: camera init ok\n");
#else
  printf("DBG: camera SKIPPED, mode 2\n");
#endif
  if (uart_open())             { printf("ERROR: uart open failed\n");   pmsis_exit(-1); }
  printf("DBG: uart ok\n");

  void    *l2_buffer = pi_l2_malloc(L2_BUF_SIZE);                                  // net in/out and scratch
  uint8_t *raw       = pi_l2_malloc(CAMERA_CAPTURE_WIDTH * CAMERA_CAPTURE_HEIGHT); // raw sensor frame
  uint8_t *frame160  = pi_l2_malloc(IMG_W * IMG_H_CAM);                            // cropped 160x160
  if (!l2_buffer || !raw || !frame160) { printf("ERROR: L2 alloc failed\n"); pmsis_exit(-1); }
  printf("DBG: buffers ok, entering loop\n");

  uint32_t n = 0;
  while (1) {
#if GATE8_CAM_MODE != 0
    /* modes 1 and 2 feed a constant input, no capture. */
    printf("DBG[%lu]: memset input, mode %d\n", n, GATE8_CAM_MODE);
    memset(l2_buffer, 128, IMG_W * IMG_H_NET);
#else
    printf("DBG[%lu]: capturing\n", n);
    /* capture raw sensor frame, then crop to 160x160 */
    camera_capture_blocking(raw, CAMERA_CAPTURE_WIDTH * CAMERA_CAPTURE_HEIGHT);
    camera_crop_to_160(raw, frame160);
    printf("DBG[%lu]: captured+cropped raw0=%d\n", n, (int)raw[0]);

    /* resize 160x160 to 96x160 into the net input region of l2_buffer */
    resize_v_160_to_96(frame160, (uint8_t *)l2_buffer);
#endif

    /* inference. output aliases l2_buffer, initial_dir 1 */
    network_run(l2_buffer, L2_BUF_SIZE, l2_buffer, 0, 1);
    printf("DBG[%lu]: inferred\n", n);

    /* dequant int32 to corners, normalized image coords */
    const int32_t *out = (const int32_t *)l2_buffer;
    float corners[N_CORNERS];
    for (int i = 0; i < N_CORNERS; i++)
      corners[i] = out[i] * GATE8_EPS + GATE8_BIAS[i];
    printf("DBG[%lu]: corners %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f\n", n,
           corners[0], corners[1], corners[2], corners[3],
           corners[4], corners[5], corners[6], corners[7]);

    /* send to STM32, ts 0 in v1 */
    gate8_send(corners, 0);
    printf("DBG[%lu]: sent %d bytes\n", n, (int)sizeof(gate8_msg_t));
    n++;
  }
}

int main(void) {
  PMU_set_voltage(1200, 0);
  pi_freq_set(PI_FREQ_DOMAIN_FC, 240000000);
  pi_freq_set(PI_FREQ_DOMAIN_CL, 175000000);

  return pmsis_kickoff((void *)main_task);
}
