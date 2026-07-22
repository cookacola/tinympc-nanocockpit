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
#include "mem.h"
#include "network.h"          // gate8-async API, network_run_async_cl
#include "flow_obstacle_uart.h"

#include <pmsis.h>
#include <bsp/ram.h>
#include "crc32.h"
#include <stdbool.h>
#include <string.h>
#include <math.h>

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

typedef struct { uint32_t stm32_timestamp; } inference_args_t;
static PI_FC_L1 co_fn_ctx_t inference_ctx;

#ifdef FLOW_OBSTACLE_CAMERA_TEST
#define FLOW_SECTORS          FLOW_OBS_SECT_MAX
#define FLOW_HALF_W           (IMG_W / 2)
#define FLOW_HALF_H           (IMG_H_CAM / 2)
#define FLOW_MAX_FEATURES     48
#define FLOW_FEATURE_STEP     6
#define FLOW_FEATURE_BORDER   10
#define FLOW_MIN_FEATURE_DIST 8
#define FLOW_ST_SCORE_THRESH  1200.0f
#define FLOW_LK_WIN_R         3
#define FLOW_LK_ITERS         3
#define FLOW_LK_ERR_THRESH    18.0f
#define FLOW_MIN_SAMPLES      3
#define FLOW_FX_PX            140.0f
#define FLOW_ASSUMED_VX_MPS   0.20f
#define FLOW_MIN_DT_S         0.005f
#define FLOW_MAX_INV_DEPTH    8.0f
#define FLOW_SEND_PERIOD_US   200000u

typedef struct {
  float x;
  float y;
  float score;
} flow_feature_t;

static PI_L2 uint8_t flow_prev_frame[IMG_W * IMG_H_CAM];
static PI_L2 uint8_t flow_prev_half[FLOW_HALF_W * FLOW_HALF_H];
static PI_L2 uint8_t flow_cur_half[FLOW_HALF_W * FLOW_HALF_H];
static PI_L2 flow_feature_t flow_features[FLOW_MAX_FEATURES];
static PI_L2 flow_obstacle_payload_t flow_camera_payload;
static PI_FC_L1 bool flow_have_prev = false;
static PI_FC_L1 uint32_t flow_prev_ts_us = 0;

static inline float f_abs(float v) {
  return v < 0.0f ? -v : v;
}

static float bilinear_sample(const uint8_t *img, int w, int h, float x, float y) {
  int xi = (int)x;
  int yi = (int)y;
  if (xi < 0 || yi < 0 || xi >= w - 1 || yi >= h - 1) {
    return 0.0f;
  }

  float ax = x - (float)xi;
  float ay = y - (float)yi;
  float v00 = (float)img[yi * w + xi];
  float v10 = (float)img[yi * w + xi + 1];
  float v01 = (float)img[(yi + 1) * w + xi];
  float v11 = (float)img[(yi + 1) * w + xi + 1];
  return (1.0f - ax) * (1.0f - ay) * v00 +
         ax * (1.0f - ay) * v10 +
         (1.0f - ax) * ay * v01 +
         ax * ay * v11;
}

static void build_half_pyramid(const uint8_t *src, uint8_t *dst) {
  for (int y = 0; y < FLOW_HALF_H; y++) {
    for (int x = 0; x < FLOW_HALF_W; x++) {
      int sx = x * 2;
      int sy = y * 2;
      int sum = src[sy * IMG_W + sx] +
                src[sy * IMG_W + sx + 1] +
                src[(sy + 1) * IMG_W + sx] +
                src[(sy + 1) * IMG_W + sx + 1];
      dst[y * FLOW_HALF_W + x] = (uint8_t)((sum + 2) / 4);
    }
  }
}

static float shi_tomasi_score(const uint8_t *img, int w, int x, int y) {
  int sxx = 0;
  int syy = 0;
  int sxy = 0;

  for (int yy = -2; yy <= 2; yy++) {
    for (int xx = -2; xx <= 2; xx++) {
      int p = (y + yy) * w + x + xx;
      int gx = (int)img[p + 1] - (int)img[p - 1];
      int gy = (int)img[p + w] - (int)img[p - w];
      sxx += gx * gx;
      syy += gy * gy;
      sxy += gx * gy;
    }
  }

  float trace = (float)(sxx + syy);
  float diff = (float)(sxx - syy);
  float sxyf = (float)sxy;
  float disc = sqrtf(diff * diff + 4.0f * sxyf * sxyf);
  return 0.5f * (trace - disc);
}

static bool feature_far_enough(const flow_feature_t *features, int n, float x, float y) {
  for (int i = 0; i < n; i++) {
    float dx = features[i].x - x;
    float dy = features[i].y - y;
    if (dx * dx + dy * dy < (float)(FLOW_MIN_FEATURE_DIST * FLOW_MIN_FEATURE_DIST)) {
      return false;
    }
  }
  return true;
}

static int select_shi_tomasi_features(const uint8_t *img, flow_feature_t *features) {
  int n = 0;
  for (int y = FLOW_FEATURE_BORDER; y < IMG_H_CAM - FLOW_FEATURE_BORDER; y += FLOW_FEATURE_STEP) {
    for (int x = FLOW_FEATURE_BORDER; x < IMG_W - FLOW_FEATURE_BORDER; x += FLOW_FEATURE_STEP) {
      float score = shi_tomasi_score(img, IMG_W, x, y);
      if (score < FLOW_ST_SCORE_THRESH || !feature_far_enough(features, n, (float)x, (float)y)) {
        continue;
      }

      int insert = n;
      while (insert > 0 && features[insert - 1].score < score) {
        if (insert < FLOW_MAX_FEATURES) {
          features[insert] = features[insert - 1];
        }
        insert--;
      }
      if (insert < FLOW_MAX_FEATURES) {
        features[insert].x = (float)x;
        features[insert].y = (float)y;
        features[insert].score = score;
        if (n < FLOW_MAX_FEATURES) {
          n++;
        }
      }
    }
  }
  return n;
}

static bool lk_track_level(const uint8_t *prev, const uint8_t *cur,
                           int w, int h, float px, float py,
                           float *cx, float *cy, float *out_err) {
  if (px < FLOW_LK_WIN_R + 1 || py < FLOW_LK_WIN_R + 1 ||
      px >= w - FLOW_LK_WIN_R - 2 || py >= h - FLOW_LK_WIN_R - 2) {
    return false;
  }

  for (int iter = 0; iter < FLOW_LK_ITERS; iter++) {
    if (*cx < FLOW_LK_WIN_R + 1 || *cy < FLOW_LK_WIN_R + 1 ||
        *cx >= w - FLOW_LK_WIN_R - 2 || *cy >= h - FLOW_LK_WIN_R - 2) {
      return false;
    }

    float gxx = 0.0f;
    float gyy = 0.0f;
    float gxy = 0.0f;
    float bx = 0.0f;
    float by = 0.0f;
    float err = 0.0f;
    int samples = 0;

    for (int yy = -FLOW_LK_WIN_R; yy <= FLOW_LK_WIN_R; yy++) {
      for (int xx = -FLOW_LK_WIN_R; xx <= FLOW_LK_WIN_R; xx++) {
        float qx = px + (float)xx;
        float qy = py + (float)yy;
        float rx = *cx + (float)xx;
        float ry = *cy + (float)yy;
        float i0 = bilinear_sample(prev, w, h, qx, qy);
        float i1 = bilinear_sample(cur, w, h, rx, ry);
        float ix = 0.5f * (bilinear_sample(prev, w, h, qx + 1.0f, qy) -
                           bilinear_sample(prev, w, h, qx - 1.0f, qy));
        float iy = 0.5f * (bilinear_sample(prev, w, h, qx, qy + 1.0f) -
                           bilinear_sample(prev, w, h, qx, qy - 1.0f));
        float it = i0 - i1;

        gxx += ix * ix;
        gyy += iy * iy;
        gxy += ix * iy;
        bx += ix * it;
        by += iy * it;
        err += f_abs(it);
        samples++;
      }
    }

    float det = gxx * gyy - gxy * gxy;
    if (det < 1.0e-3f) {
      return false;
    }

    float du = (gyy * bx - gxy * by) / det;
    float dv = (gxx * by - gxy * bx) / det;
    *cx += du;
    *cy += dv;
    *out_err = err / (float)samples;

    if (du * du + dv * dv < 0.0025f) {
      break;
    }
  }

  return *out_err <= FLOW_LK_ERR_THRESH;
}

static bool lk_track_pyramid(const uint8_t *prev, const uint8_t *cur,
                             float x, float y, float *nx, float *ny,
                             float *err) {
  float px_half = x * 0.5f;
  float py_half = y * 0.5f;
  float cx_half = px_half;
  float cy_half = py_half;

  if (!lk_track_level(flow_prev_half, flow_cur_half, FLOW_HALF_W, FLOW_HALF_H,
                      px_half, py_half, &cx_half, &cy_half, err)) {
    return false;
  }

  *nx = x + 2.0f * (cx_half - px_half);
  *ny = y + 2.0f * (cy_half - py_half);
  return lk_track_level(prev, cur, IMG_W, IMG_H_CAM, x, y, nx, ny, err);
}

static void flow_compute_camera_payload(const frame_t *camera_frame,
                                        flow_obstacle_payload_t *payload) {
  static PI_FC_L1 float flow_sum[FLOW_SECTORS];
  static PI_FC_L1 int flow_count[FLOW_SECTORS];

  memset(payload, 0, sizeof(*payload));
  payload->gap8_ts_us = camera_frame->frame_timestamp;
  payload->dt_s = FLOW_MIN_DT_S;
  payload->n_sectors = FLOW_SECTORS;
  payload->flags = 0;

  if (flow_have_prev && camera_frame->frame_timestamp > flow_prev_ts_us) {
    payload->dt_s = (camera_frame->frame_timestamp - flow_prev_ts_us) * 1.0e-6f;
    if (payload->dt_s < FLOW_MIN_DT_S) {
      payload->dt_s = FLOW_MIN_DT_S;
    }

    memset(flow_sum, 0, sizeof(flow_sum));
    memset(flow_count, 0, sizeof(flow_count));

    const uint8_t *cur = camera_frame->buffer;
    build_half_pyramid(cur, flow_cur_half);
    int n_features = select_shi_tomasi_features(flow_prev_frame, flow_features);
    for (int k = 0; k < n_features; k++) {
      float nx = flow_features[k].x;
      float ny = flow_features[k].y;
      float err = 0.0f;
      if (!lk_track_pyramid(flow_prev_frame, cur, flow_features[k].x, flow_features[k].y,
                            &nx, &ny, &err)) {
        continue;
      }

      float dx = nx - flow_features[k].x;
      if (f_abs(dx) < 0.05f) {
        continue;
      }

      int sector = ((int)flow_features[k].x * FLOW_SECTORS) / IMG_W;
      if (sector >= FLOW_SECTORS) {
        sector = FLOW_SECTORS - 1;
      }
      flow_sum[sector] += f_abs(dx);
      flow_count[sector]++;
    }

    const float assumed_translation_m = FLOW_ASSUMED_VX_MPS * payload->dt_s;
    for (int i = 0; i < FLOW_SECTORS; i++) {
      const float center_x = ((float)i + 0.5f) * ((float)IMG_W / (float)FLOW_SECTORS);
      payload->sector[i].azimuth_rad = (center_x - ((float)IMG_W * 0.5f)) / FLOW_FX_PX;

      if (flow_count[i] >= FLOW_MIN_SAMPLES && assumed_translation_m > 1.0e-5f) {
        float avg_flow_px = flow_sum[i] / (float)flow_count[i];
        float inv_depth = avg_flow_px / (FLOW_FX_PX * assumed_translation_m);
        if (inv_depth > FLOW_MAX_INV_DEPTH) {
          inv_depth = FLOW_MAX_INV_DEPTH;
        }
        payload->sector[i].inv_depth = inv_depth;
        payload->sector[i].ttc_s = avg_flow_px > 0.1f ? FLOW_FX_PX * payload->dt_s / avg_flow_px : 99.0f;
        payload->sector[i].confidence = (float)flow_count[i] / 32.0f;
        if (payload->sector[i].confidence > 1.0f) {
          payload->sector[i].confidence = 1.0f;
        }
      } else {
        payload->sector[i].inv_depth = 0.0f;
        payload->sector[i].ttc_s = 99.0f;
        payload->sector[i].confidence = 0.0f;
      }
    }
  }

  memcpy(flow_prev_frame, camera_frame->buffer, IMG_W * IMG_H_CAM);
  build_half_pyramid(flow_prev_frame, flow_prev_half);
  flow_prev_ts_us = camera_frame->frame_timestamp;
  flow_have_prev = true;
}

#endif

#ifdef FLOW_OBSTACLE_TEST_ONLY
static void flow_obstacle_test_only_loop(void) {
  static PI_L2 flow_obstacle_payload_t flow_payload;
  static pi_task_t done_task;
  uint32_t n = 0;

  printf("flow obstacle UART-only test: sending synthetic sectors\n");
  while (true) {
    flow_obstacle_make_test_payload(&flow_payload, time_get_us(), 0, 0.1f);
    flow_obstacle_send_async(&uart, &flow_payload, pi_task_block(&done_task));
    pi_task_wait_on(&done_task);
    if ((n++ % 10) == 0) {
      printf("flowObs test sent %lu\n", n);
    }
    pi_time_wait_us(100000);
  }
}
#endif

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
#ifdef FLOW_OBSTACLE_CAMERA_TEST
  static PI_FC_L1 co_event_t flow_send_done;
  static PI_FC_L1 uint32_t last_flow_send_us = 0;

  if (flow_have_prev && camera_frame->frame_timestamp - last_flow_send_us < FLOW_SEND_PERIOD_US) {
    CO_RETURN();
  }

  flow_compute_camera_payload(camera_frame, &flow_camera_payload);
  if (flow_have_prev) {
    last_flow_send_us = flow_camera_payload.gap8_ts_us;
    flow_obstacle_send_async(&uart, &flow_camera_payload, co_event_init(&flow_send_done));
    CO_WAIT(&flow_send_done);
  }
#else
  static PI_FC_L1 bool started = false;
  static PI_FC_L1 inference_args_t iargs;
  static PI_FC_L1 co_event_t inference_done;

  if (started) {
    while (!co_event_is_done(&inference_done)) {
      CO_WAIT(&inference_done);
    }
  }
  started = true;

  resize_v_160_to_96(camera_frame->buffer, (uint8_t *)l2_buffer);

  iargs.stm32_timestamp = 0;
  co_fn_push_start(&inference_ctx, inference_task, &iargs, co_event_init(&inference_done));
#endif
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

#ifdef FLOW_OBSTACLE_TEST_UART
  static PI_FC_L1 uint32_t flow_div = 0;
  if ((flow_div++ % 10) == 0) {
    static PI_FC_L1 flow_obstacle_payload_t flow_payload;
    flow_obstacle_make_test_payload(&flow_payload, time_get_us(), args->stm32_timestamp, 1.0f / HIMAX_FRAME_RATE);
    flow_obstacle_send_async(&uart, &flow_payload, co_event_init(&done));
    CO_WAIT(&done);
  }
#endif

#if GATE8_DEBUG_PRINT
  static PI_FC_L1 uint32_t n = 0;
  printf("gate8[%lu]: %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f\n", n,
         corners[0], corners[1], corners[2], corners[3],
         corners[4], corners[5], corners[6], corners[7]);
  n++;
#endif
}
CO_FN_END()

static void main_task(void) {
  soc_init();
  uart_init(&uart);

#ifdef FLOW_OBSTACLE_TEST_ONLY
  flow_obstacle_test_only_loop();
#else
  camera_init(&camera, camera_callback);
  camera_init_frames_alloc(&camera);
#ifndef FLOW_OBSTACLE_CAMERA_TEST
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
  printf("gate8 deploy: init done, starting camera\n");
#endif
#else
  trace_init();
  printf("flow camera test: move laterally by hand in front of a textured obstacle\n");
  printf("flow camera test: metric depth assumes VX=%.2fm/s, so use relative trends first\n", FLOW_ASSUMED_VX_MPS);
  printf("flow camera test: sector values are exposed as STM32 flowObsRx logs\n");
#endif
  camera_start(&camera);

  while (true) {
    pi_yield();
  }
#endif

  pmsis_exit(0);
}

int main(void) {
  return pmsis_kickoff((void *)main_task);
}
