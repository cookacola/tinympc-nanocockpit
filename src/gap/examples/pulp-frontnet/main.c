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
#include "uart_protocol.h"
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
/* DORY's directional-allocation peak is 153600 B (layer 1); keep 6.4 kB
 * headroom. The old example reserved 380 kB, which prevented coexistence
 * with camera and optical-flow buffers on GAP8's 512 kB L2. */
#define L2_BUF_SIZE  160000

// 1 = print per-inference corners over JTAG. Set 0 for a flash-boot deploy:
// there is no console then and printf can pollute the gate8 UART.
#define GATE8_DEBUG_PRINT 0

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
_Static_assert(sizeof(gate8_msg_t) == 44, "gate packet ABI changed");

static uart_t      uart;
static uart_protocol_t uart_protocol;
static camera_t    camera;
static pi_device_t cluster;     // shared cluster, opened once, reused by the net
static void       *l2_buffer;   // net input, scratch and output
static size_t      l2_buffer_size;
static PI_L2 gate8_msg_t gate8_tx_msg;
/* UART DMA must never reference the inference-owned pending buffer: a cluster
 * completion can publish the next result while the FC waits for TX completion. */
static PI_L2 gate8_msg_t gate8_uart_msg;
static PI_FC_L1 volatile bool gate8_tx_pending = false;
static PI_FC_L1 uint32_t gate8_tx_dropped = 0;
static PI_FC_L1 uint32_t uart_queued_count = 0;
static PI_FC_L1 uint32_t uart_completed_count = 0;
/* GAP SDK 3.8's asynchronous UART completion has no transfer-status result. */
static PI_FC_L1 uint32_t uart_error_count = 0;
static PI_FC_L1 volatile bool uart_tx_busy = false;
static PI_FC_L1 volatile bool uart_tx_is_flow = false;
static PI_FC_L1 uint32_t cnn_invocation_count = 0;
static PI_FC_L1 uint32_t cnn_completion_count = 0;
static PI_FC_L1 uint32_t cnn_last_us = 0;
static PI_FC_L1 uint32_t cnn_max_us = 0;
static PI_FC_L1 volatile bool inference_busy = false;
static PI_FC_L1 uint32_t flow_snapshot_count = 0;
static PI_FC_L1 uint32_t flow_processed_count = 0;
static PI_FC_L1 uint32_t flow_transmitted_count = 0;
static PI_FC_L1 uint32_t flow_snapshot_dropped = 0;
static PI_FC_L1 uint32_t flow_tx_dropped = 0;
static PI_FC_L1 state_msg_t latest_state;
static PI_FC_L1 uint32_t latest_state_rx_us = 0;
static PI_FC_L1 uint32_t state_rx_count = 0;
static PI_FC_L1 uint32_t state_rx_stale = 0;

typedef struct { uint32_t stm32_timestamp; } inference_args_t;
static PI_FC_L1 co_fn_ctx_t inference_ctx;

CO_FN_BEGIN(uart_state_callback, uart_msg_t *, message)
{
  if (memcmp(message->header, UART_STATE_MSG_HEADER, UART_HEADER_LENGTH) == 0) {
    latest_state = message->state;
    latest_state_rx_us = message->recv_timestamp;
    state_rx_count++;
  }
}
CO_FN_END()

/*
 * Translate a GAP8 camera timestamp into the STM32 millisecond tick domain.
 * State packets carry both the producer tick and their GAP8 receive time.  The
 * UART latency is small and nearly constant; extrapolating the latest tick to
 * the exposure timestamp removes the much larger packet-processing delay on
 * the return path.  STM32 performs the final interpolation from its state
 * history, so this value is only a clock-domain correspondence, not a pose.
 */
static uint32_t stm32_tick_at_gap8_time(uint32_t gap8_ts_us) {
  if (state_rx_count == 0u) {
    state_rx_stale++;
    return 0u;
  }
  const int32_t age_us = (int32_t)(gap8_ts_us - latest_state_rx_us);
  if (age_us < -20000 || age_us > 100000) {
    state_rx_stale++;
    return 0u;
  }
  const int32_t delta_ms = age_us >= 0 ?
      (age_us + 500) / 1000 : (age_us - 500) / 1000;
  return latest_state.timestamp + (uint32_t)delta_ms;
}

#if defined(FLOW_OBSTACLE_ENABLE) || defined(FLOW_OBSTACLE_CAMERA_TEST)
#define FLOW_SECTORS          FLOW_OBS_SECT_MAX
#define FLOW_HALF_W           (IMG_W / 2)
#define FLOW_HALF_H           (IMG_H_CAM / 2)
#ifndef FLOW_MAX_FEATURES
#define FLOW_MAX_FEATURES     27
#endif
#if (FLOW_MAX_FEATURES != 27) && (FLOW_MAX_FEATURES != 36)
#error "FLOW_MAX_FEATURES must be the evaluated 27- or 36-feature configuration"
#endif
#define FLOW_FEATURES_PER_SECTOR (FLOW_MAX_FEATURES / FLOW_SECTORS)
#define FLOW_FEATURE_STEP     6
#define FLOW_FEATURE_BORDER   10
#define FLOW_FEATURE_GRID_W \
  ((IMG_W - 2 * FLOW_FEATURE_BORDER + FLOW_FEATURE_STEP - 1) / FLOW_FEATURE_STEP)
#define FLOW_FEATURE_GRID_H \
  ((IMG_H_CAM - 2 * FLOW_FEATURE_BORDER + FLOW_FEATURE_STEP - 1) / FLOW_FEATURE_STEP)
#define FLOW_MIN_FEATURE_DIST 8
#define FLOW_ST_SCORE_THRESH_PROXY 600u
#define FLOW_LK_WIN_R         2
#define FLOW_LK_HALF_ITERS    2
#define FLOW_LK_FULL_ITERS    1
#define FLOW_LK_ERR_THRESH    18.0f
#define FLOW_FB_ERR_THRESH_PX 0.75f
#define FLOW_MIN_SAMPLES      2
#define FLOW_FX_PX            89.15584f
#define FLOW_FY_PX            89.46082f
#define FLOW_CX_PX            81.10381f
#define FLOW_CY_PX            73.34730f
#define FLOW_K1              -0.01764488f
#define FLOW_K2               0.09941325f
#define FLOW_P1               0.00544322f
#define FLOW_P2              -0.00604001f
#define FLOW_K3              -0.19001899f
#define FLOW_MIN_DT_S         0.005f
#define FLOW_MAX_RAD_S        20.0f
#ifndef VISION_PERIOD_US
#define VISION_PERIOD_US      65000u
#endif
#define FLOW_SMOOTH_ALPHA     0.35f
#define FLOW_HOLD_UPDATES     3
#define FLOW_HOLD_CONF_DECAY  0.60f
#define DIAGNOSTICS_PERIOD_US 30000000u
#ifndef FLOW_BUILD_ID
#define FLOW_BUILD_ID "unknown"
#endif
#ifndef FLOW_DIAGNOSTIC_CAPTURE
#define FLOW_DIAGNOSTIC_CAPTURE 0
#endif

typedef struct {
  float x;
  float y;
  float score;
} flow_feature_t;

typedef struct {
  float x;
  float y;
  float nx;
  float ny;
  float lk_error;
  float fb_error;
  float score;
} flow_track_debug_t;

static PI_L2 uint8_t flow_prev_frame[IMG_W * IMG_H_CAM];
static PI_L2 uint8_t flow_cur_frame[IMG_W * IMG_H_CAM];
static PI_L2 uint8_t flow_prev_half[FLOW_HALF_W * FLOW_HALF_H];
static PI_L2 uint8_t flow_cur_half[FLOW_HALF_W * FLOW_HALF_H];
static PI_L2 flow_feature_t flow_features[FLOW_MAX_FEATURES];
static PI_L2 flow_track_debug_t flow_debug_tracks[FLOW_MAX_FEATURES];
static PI_FC_L1 int flow_debug_track_count = 0;
static PI_FC_L1 bool flow_debug_pair_dumped = false;
static PI_L2 uint32_t flow_feature_scores[FLOW_FEATURE_GRID_W * FLOW_FEATURE_GRID_H];
static PI_L2 flow_obstacle_payload_t flow_camera_payload;
static PI_L2 flow_obstacle_payload_t flow_tx_payload;
static PI_L2 flow_track_payload_t flow_track_camera_payload;
static PI_L2 flow_track_payload_t flow_track_tx_payload;
static PI_FC_L1 bool flow_have_prev = false;
static volatile bool flow_snapshot_pending = false;
static PI_FC_L1 volatile bool flow_tx_pending = false;
static PI_FC_L1 volatile bool flow_track_tx_pending = false;
static PI_FC_L1 uint32_t cnn_frame_dropped = 0;
static PI_FC_L1 uint16_t flow_wire_seq = 1;
static PI_FC_L1 uint32_t flow_snapshot_ts_us = 0;
static PI_FC_L1 uint32_t flow_prev_ts_us = 0;
static PI_FC_L1 float flow_smooth_x[FLOW_SECTORS];
static PI_FC_L1 float flow_smooth_y[FLOW_SECTORS];
static PI_FC_L1 float flow_smooth_sigma[FLOW_SECTORS];
static PI_FC_L1 float flow_smooth_conf[FLOW_SECTORS];
static PI_FC_L1 uint8_t flow_hold_count[FLOW_SECTORS];
static PI_FC_L1 bool flow_filter_initialized = false;

static void flow_undistort_normalized(float u, float v,
                                      float *x_out, float *y_out) {
  const float xd = (u - FLOW_CX_PX) / FLOW_FX_PX;
  const float yd = (v - FLOW_CY_PX) / FLOW_FY_PX;
  float x = xd;
  float y = yd;
  for (int iter = 0; iter < 5; iter++) {
    const float r2 = x * x + y * y;
    const float radial =
        1.0f + FLOW_K1 * r2 + FLOW_K2 * r2 * r2 +
        FLOW_K3 * r2 * r2 * r2;
    const float dx = 2.0f * FLOW_P1 * x * y +
                     FLOW_P2 * (r2 + 2.0f * x * x);
    const float dy = FLOW_P1 * (r2 + 2.0f * y * y) +
                     2.0f * FLOW_P2 * x * y;
    x = (xd - dx) / radial;
    y = (yd - dy) / radial;
  }
  *x_out = x;
  *y_out = y;
}

static uint16_t flow_quantize_uq(float value, float scale) {
  float scaled = value * scale;
  if (scaled <= 0.0f) return 0;
  if (scaled >= 65535.0f) return 65535;
  return (uint16_t)(scaled + 0.5f);
}

static int16_t flow_quantize_sq(float value, float scale) {
  float scaled = value * scale;
  if (scaled <= -32768.0f) return -32768;
  if (scaled >= 32767.0f) return 32767;
  return (int16_t)(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
}

static void flow_make_track_payload(uint32_t frame_timestamp,
                                    uint32_t stm32_ts_echo,
                                    float dt_s,
                                    uint8_t flags) {
  memset(&flow_track_camera_payload, 0, sizeof(flow_track_camera_payload));
  flow_track_camera_payload.gap8_ts_us = frame_timestamp;
  flow_track_camera_payload.stm32_ts_echo = stm32_ts_echo;
  const float dt_us = dt_s * 1000000.0f;
  flow_track_camera_payload.dt_us =
      dt_us >= 65535.0f ? 65535u : (uint16_t)(dt_us + 0.5f);
  flow_track_camera_payload.version = FLOW_TRACK_WIRE_VERSION;
  flow_track_camera_payload.flags = flags;

  /* Keep spatial coverage when a 36-feature build exceeds the 32-track wire
   * bound. First retain the four strongest tracks in each of eight columns,
   * then use any remaining slots for the strongest unselected tracks. */
  uint8_t selected[FLOW_MAX_FEATURES] = {0};
  int selected_count = 0;
  for (int band = 0; band < 8 && selected_count < FLOW_TRACK_MAX; band++) {
    for (int slot = 0; slot < 4 && selected_count < FLOW_TRACK_MAX; slot++) {
      int best = -1;
      float best_score = -1.0f;
      for (int i = 0; i < flow_debug_track_count; i++) {
        const int track_band =
            ((int)flow_debug_tracks[i].x * 8) / IMG_W;
        if (!selected[i] && track_band == band &&
            flow_debug_tracks[i].score > best_score) {
          best = i;
          best_score = flow_debug_tracks[i].score;
        }
      }
      if (best < 0) break;
      selected[best] = 1;
      flow_track_wire_t *wire =
          &flow_track_camera_payload.track[selected_count++];
      const flow_track_debug_t *track = &flow_debug_tracks[best];
      wire->u_q4 = flow_quantize_uq(track->x, 16.0f);
      wire->v_q4 = flow_quantize_uq(track->y, 16.0f);
      wire->du_q8 = flow_quantize_sq(track->nx - track->x, 256.0f);
      wire->dv_q8 = flow_quantize_sq(track->ny - track->y, 256.0f);
      wire->lk_err_q8 = flow_quantize_uq(track->lk_error, 256.0f);
      wire->fb_err_q8 = flow_quantize_uq(track->fb_error, 256.0f);
    }
  }
  while (selected_count < FLOW_TRACK_MAX &&
         selected_count < flow_debug_track_count) {
    int best = -1;
    float best_score = -1.0f;
    for (int i = 0; i < flow_debug_track_count; i++) {
      if (!selected[i] && flow_debug_tracks[i].score > best_score) {
        best = i;
        best_score = flow_debug_tracks[i].score;
      }
    }
    if (best < 0) break;
    selected[best] = 1;
    flow_track_wire_t *wire =
        &flow_track_camera_payload.track[selected_count++];
    const flow_track_debug_t *track = &flow_debug_tracks[best];
    wire->u_q4 = flow_quantize_uq(track->x, 16.0f);
    wire->v_q4 = flow_quantize_uq(track->y, 16.0f);
    wire->du_q8 = flow_quantize_sq(track->nx - track->x, 256.0f);
    wire->dv_q8 = flow_quantize_sq(track->ny - track->y, 256.0f);
    wire->lk_err_q8 = flow_quantize_uq(track->lk_error, 256.0f);
    wire->fb_err_q8 = flow_quantize_uq(track->fb_error, 256.0f);
  }
  flow_track_camera_payload.count = (uint8_t)selected_count;
}

typedef struct {
  uint32_t sequence;
  uint32_t frame_ts_us;
  uint32_t frame_dt_us;
  uint32_t total_us;
  uint32_t pyramid_us;
  uint32_t select_us;
  uint32_t track_us;
  uint32_t aggregate_us;
  uint32_t corner_max_score;
  uint16_t selected;
  uint16_t accepted;
  uint16_t rejected;
  uint16_t mean_error_milli;
  uint16_t max_error_milli;
  uint16_t valid_sector_mask;
} flow_diagnostics_t;

static PI_FC_L1 flow_diagnostics_t flow_diag;

typedef struct {
  uint32_t count;
  uint64_t total_sum_us;
  uint64_t total_sum_sq_us;
  uint32_t total_min_us;
  uint32_t total_max_us;
  uint64_t track_sum_us;
  uint64_t track_sum_sq_us;
  uint32_t track_min_us;
  uint32_t track_max_us;
  uint64_t dt_sum_us;
  uint64_t dt_sum_sq_us;
  uint32_t dt_min_us;
  uint32_t dt_max_us;
  uint32_t selected_sum;
  uint32_t selected_sum_sq;
  uint32_t accepted_sum;
  uint32_t accepted_sum_sq;
  uint32_t error_sum_milli;
  uint64_t error_sum_sq_milli;
  uint32_t error_min_milli;
  uint32_t error_max_milli;
  uint32_t error_count;
  uint32_t valid_sector_sum;
  uint32_t valid_sector_sum_sq;
} flow_profile_t;

static PI_FC_L1 flow_profile_t flow_profile;
static PI_FC_L1 bool flow_profile_skip_next = false;
static PI_FC_L1 uint32_t cnn_profile_count = 0;
static PI_FC_L1 uint64_t cnn_profile_sum_us = 0;
static PI_FC_L1 uint64_t cnn_profile_sum_sq_us = 0;
static PI_FC_L1 uint32_t cnn_profile_min_us = UINT32_MAX;
static PI_FC_L1 uint32_t cnn_profile_max_us = 0;
static PI_FC_L1 bool cnn_profile_skip_next = false;

/*
 * Per-frame JTAG prints perturb the alternating flow/CNN schedule. Keep
 * bounded 250 us histograms and report conservative bin upper bounds for
 * p95/p99 in each heartbeat window. The final bin catches overruns.
 */
#define PROFILE_HIST_BIN_US 250u
#define PROFILE_HIST_BINS   256u
static PI_FC_L1 uint16_t flow_total_hist[PROFILE_HIST_BINS];
static PI_FC_L1 uint16_t cnn_time_hist[PROFILE_HIST_BINS];

static void profile_hist_add(uint16_t *hist, uint32_t duration_us) {
  uint32_t bin = duration_us / PROFILE_HIST_BIN_US;
  if (bin >= PROFILE_HIST_BINS) {
    bin = PROFILE_HIST_BINS - 1u;
  }
  if (hist[bin] != UINT16_MAX) {
    hist[bin]++;
  }
}

static uint32_t profile_hist_percentile_us(const uint16_t *hist,
                                           uint32_t count,
                                           uint32_t percentile) {
  if (count == 0u) {
    return 0u;
  }
  const uint32_t target = (count * percentile + 99u) / 100u;
  uint32_t cumulative = 0u;
  for (uint32_t bin = 0u; bin < PROFILE_HIST_BINS; bin++) {
    cumulative += hist[bin];
    if (cumulative >= target) {
      return (bin + 1u) * PROFILE_HIST_BIN_US;
    }
  }
  return PROFILE_HIST_BINS * PROFILE_HIST_BIN_US;
}

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

static uint32_t corner_score_proxy(const uint8_t *img, int w, int x, int y) {
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

  /*
   * det / trace is a monotonic, conservative approximation of the smaller
   * structure-tensor eigenvalue. Dividing by the highest power of two below
   * trace avoids both GAP8 software floating point and integer division.
   */
  const uint32_t trace = (uint32_t)(sxx + syy);
  const int64_t det = (int64_t)sxx * (int64_t)syy -
                      (int64_t)sxy * (int64_t)sxy;
  if (trace == 0u || det <= 0) {
    return 0u;
  }
  const uint32_t shift = 31u - (uint32_t)__builtin_clz(trace);
  return (uint32_t)((uint64_t)det >> shift);
}

static int select_shi_tomasi_features(const uint8_t *img, flow_feature_t *features) {
  /* Score each grid point once. The previous implementation recomputed the
   * same 5x5 structure tensor for every requested slot in a sector. */
  int grid_i = 0;
  uint32_t max_score = 0;
  for (int y = FLOW_FEATURE_BORDER; y < IMG_H_CAM - FLOW_FEATURE_BORDER;
       y += FLOW_FEATURE_STEP) {
    for (int x = FLOW_FEATURE_BORDER; x < IMG_W - FLOW_FEATURE_BORDER;
         x += FLOW_FEATURE_STEP) {
      const uint32_t score = corner_score_proxy(img, IMG_W, x, y);
      flow_feature_scores[grid_i++] = score;
      if (score > max_score) {
        max_score = score;
      }
    }
  }
  flow_diag.corner_max_score = max_score;

  int n = 0;
  for (int sector = 0; sector < FLOW_SECTORS; sector++) {
    const int x0 = sector * IMG_W / FLOW_SECTORS;
    const int x1 = (sector + 1) * IMG_W / FLOW_SECTORS;
    for (int slot = 0; slot < FLOW_FEATURES_PER_SECTOR && n < FLOW_MAX_FEATURES; slot++) {
      uint32_t best_score = FLOW_ST_SCORE_THRESH_PROXY;
      int best_x = -1;
      int best_y = -1;
      int score_i = 0;
      for (int y = FLOW_FEATURE_BORDER; y < IMG_H_CAM - FLOW_FEATURE_BORDER;
           y += FLOW_FEATURE_STEP) {
        for (int x = FLOW_FEATURE_BORDER; x < IMG_W - FLOW_FEATURE_BORDER;
             x += FLOW_FEATURE_STEP, score_i++) {
          if (x < x0 || x >= x1) {
            continue;
          }
          uint32_t score = flow_feature_scores[score_i];
          if (score > best_score) {
            best_score = score;
            best_x = x;
            best_y = y;
          }
        }
      }
      if (best_x < 0) {
        break;
      }
      features[n].x = (float)best_x;
      features[n].y = (float)best_y;
      features[n].score = (float)best_score;
      n++;

      /*
       * Suppress neighboring grid candidates once, using integer coordinates.
       * This is equivalent to checking every candidate against every prior
       * feature, but avoids the repeated software floating-point work.
       */
      score_i = 0;
      for (int y = FLOW_FEATURE_BORDER; y < IMG_H_CAM - FLOW_FEATURE_BORDER;
           y += FLOW_FEATURE_STEP) {
        const int dy = y - best_y;
        for (int x = FLOW_FEATURE_BORDER; x < IMG_W - FLOW_FEATURE_BORDER;
             x += FLOW_FEATURE_STEP, score_i++) {
          const int dx = x - best_x;
          if (dx * dx + dy * dy <
              FLOW_MIN_FEATURE_DIST * FLOW_MIN_FEATURE_DIST) {
            flow_feature_scores[score_i] = 0u;
          }
        }
      }
    }
  }
  return n;
}

static float robust_near_sample(float *values, int n) {
  /* Select the near half (largest absolute expansion/parallax), then take
   * its signed median. n <= 5, so insertion-like sorting is cheaper and
   * smaller than pulling a general sort into GAP8 code. */
  for (int i = 0; i < n - 1; i++) {
    for (int j = i + 1; j < n; j++) {
      if (f_abs(values[j]) > f_abs(values[i])) {
        float tmp = values[i]; values[i] = values[j]; values[j] = tmp;
      }
    }
  }
  int keep = (n + 1) / 2;
  if (keep < FLOW_MIN_SAMPLES) keep = FLOW_MIN_SAMPLES;
  for (int i = 0; i < keep - 1; i++) {
    for (int j = i + 1; j < keep; j++) {
      if (values[j] < values[i]) {
        float tmp = values[i]; values[i] = values[j]; values[j] = tmp;
      }
    }
  }
  return values[keep / 2];
}

static float flow_sample_sigma(const float *values, int n,
                               float center, float dt_s) {
  float absolute_deviation = 0.0f;
  for (int i = 0; i < n; i++) {
    absolute_deviation += f_abs(values[i] - center);
  }
  const float observed = absolute_deviation / (float)n / dt_s;
  /* A quarter-pixel full-resolution floor captures interpolation and
   * calibration error even when the few selected tracks agree exactly. */
  const float quantization_floor = 0.25f / (FLOW_FX_PX * dt_s);
  return observed > quantization_floor ? observed : quantization_floor;
}

#if FLOW_DIAGNOSTIC_CAPTURE
static void flow_dump_frame_hex(const char *name, const uint8_t *frame) {
  printf("FLOWCAP_FRAME,%s,%u,", name, IMG_W * IMG_H_CAM);
  for (int i = 0; i < IMG_W * IMG_H_CAM; i++) {
    printf("%02x", frame[i]);
  }
  printf("\n");
}

static void flow_dump_diagnostic_pair(uint32_t prev_ts_us,
                                      uint32_t cur_ts_us) {
  printf("FLOWCAP_BEGIN,1,%lu,%lu,%d,%d,%d\n",
         prev_ts_us, cur_ts_us, IMG_W, IMG_H_CAM,
         flow_debug_track_count);
  flow_dump_frame_hex("prev", flow_prev_frame);
  flow_dump_frame_hex("cur", flow_cur_frame);
  for (int i = 0; i < flow_debug_track_count; i++) {
    const flow_track_debug_t *track = &flow_debug_tracks[i];
    printf("FLOWCAP_TRACK,%d,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f\n",
           i, track->x, track->y, track->nx, track->ny,
           track->lk_error, track->fb_error);
  }
  printf("FLOWCAP_END,1\n");
}
#endif

static bool lk_track_level(const uint8_t *prev, const uint8_t *cur,
                           int w, int h, float px, float py,
                           float *cx, float *cy, float *out_err,
                           int iterations) {
  static PI_FC_L1 float patch_i0[(2 * FLOW_LK_WIN_R + 1) *
                                 (2 * FLOW_LK_WIN_R + 1)];
  static PI_FC_L1 float patch_ix[(2 * FLOW_LK_WIN_R + 1) *
                                 (2 * FLOW_LK_WIN_R + 1)];
  static PI_FC_L1 float patch_iy[(2 * FLOW_LK_WIN_R + 1) *
                                 (2 * FLOW_LK_WIN_R + 1)];
  if (px < FLOW_LK_WIN_R + 1 || py < FLOW_LK_WIN_R + 1 ||
      px >= w - FLOW_LK_WIN_R - 2 || py >= h - FLOW_LK_WIN_R - 2) {
    return false;
  }

  /* The reference patch and its gradients are constant across Gauss-Newton
   * iterations. Cache them once instead of performing five bilinear samples
   * per pixel on every iteration. */
  int patch_i = 0;
  const int pxi = (int)px;
  const int pyi = (int)py;
  for (int yy = -FLOW_LK_WIN_R; yy <= FLOW_LK_WIN_R; yy++) {
    for (int xx = -FLOW_LK_WIN_R; xx <= FLOW_LK_WIN_R; xx++, patch_i++) {
      const int p = (pyi + yy) * w + pxi + xx;
      patch_i0[patch_i] = (float)prev[p];
      patch_ix[patch_i] =
        0.5f * ((float)prev[p + 1] - (float)prev[p - 1]);
      patch_iy[patch_i] =
        0.5f * ((float)prev[p + w] - (float)prev[p - w]);
    }
  }

  for (int iter = 0; iter < iterations; iter++) {
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

    patch_i = 0;
    for (int yy = -FLOW_LK_WIN_R; yy <= FLOW_LK_WIN_R; yy++) {
      for (int xx = -FLOW_LK_WIN_R; xx <= FLOW_LK_WIN_R; xx++, patch_i++) {
        float rx = *cx + (float)xx;
        float ry = *cy + (float)yy;
        float i1 = bilinear_sample(cur, w, h, rx, ry);
        float ix = patch_ix[patch_i];
        float iy = patch_iy[patch_i];
        float it = patch_i0[patch_i] - i1;

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
                             const uint8_t *prev_half,
                             const uint8_t *cur_half,
                             float x, float y, float *nx, float *ny,
                             float *err) {
  float px_half = x * 0.5f;
  float py_half = y * 0.5f;
  float cx_half = px_half;
  float cy_half = py_half;

  if (!lk_track_level(prev_half, cur_half, FLOW_HALF_W, FLOW_HALF_H,
                      px_half, py_half, &cx_half, &cy_half, err,
                      FLOW_LK_HALF_ITERS)) {
    return false;
  }

  float cx_full = x + 2.0f * (cx_half - px_half);
  float cy_full = y + 2.0f * (cy_half - py_half);
  if (!lk_track_level(prev, cur, IMG_W, IMG_H_CAM, x, y,
                      &cx_full, &cy_full, err, FLOW_LK_FULL_ITERS)) {
    return false;
  }
  *nx = cx_full;
  *ny = cy_full;
  return true;
}

static void flow_filter_payload(flow_obstacle_payload_t *payload) {
  if (!flow_filter_initialized) {
    memset(flow_smooth_x, 0, sizeof(flow_smooth_x));
    memset(flow_smooth_y, 0, sizeof(flow_smooth_y));
    memset(flow_smooth_sigma, 0, sizeof(flow_smooth_sigma));
    memset(flow_smooth_conf, 0, sizeof(flow_smooth_conf));
    for (int i = 0; i < FLOW_SECTORS; i++) {
      flow_hold_count[i] = 0;
    }
    flow_filter_initialized = true;
  }

  for (int i = 0; i < FLOW_SECTORS; i++) {
    const bool valid = payload->sector[i].confidence > 0.0f;
    if (valid) {
      if (flow_smooth_conf[i] <= 0.0f) {
        flow_smooth_x[i] = payload->sector[i].flow_x_rad_s;
        flow_smooth_y[i] = payload->sector[i].flow_y_rad_s;
        flow_smooth_sigma[i] = payload->sector[i].flow_sigma_rad_s;
        flow_smooth_conf[i] = payload->sector[i].confidence;
      } else {
        flow_smooth_x[i] =
          FLOW_SMOOTH_ALPHA * payload->sector[i].flow_x_rad_s +
          (1.0f - FLOW_SMOOTH_ALPHA) * flow_smooth_x[i];
        flow_smooth_y[i] =
          FLOW_SMOOTH_ALPHA * payload->sector[i].flow_y_rad_s +
          (1.0f - FLOW_SMOOTH_ALPHA) * flow_smooth_y[i];
        flow_smooth_sigma[i] =
          FLOW_SMOOTH_ALPHA * payload->sector[i].flow_sigma_rad_s +
          (1.0f - FLOW_SMOOTH_ALPHA) * flow_smooth_sigma[i];
        flow_smooth_conf[i] =
          FLOW_SMOOTH_ALPHA * payload->sector[i].confidence +
          (1.0f - FLOW_SMOOTH_ALPHA) * flow_smooth_conf[i];
      }
      flow_hold_count[i] = FLOW_HOLD_UPDATES;
    } else if (flow_hold_count[i] > 0) {
      flow_hold_count[i]--;
      flow_smooth_conf[i] *= FLOW_HOLD_CONF_DECAY;
    } else {
      flow_smooth_x[i] = 0.0f;
      flow_smooth_y[i] = 0.0f;
      flow_smooth_sigma[i] = 0.0f;
      flow_smooth_conf[i] = 0.0f;
    }

    payload->sector[i].flow_x_rad_s = flow_smooth_x[i];
    payload->sector[i].flow_y_rad_s = flow_smooth_y[i];
    payload->sector[i].flow_sigma_rad_s = flow_smooth_sigma[i];
    payload->sector[i].confidence = flow_smooth_conf[i];
  }
}

static void flow_compute_camera_payload(const uint8_t *cur,
                                        uint32_t frame_timestamp,
                                        flow_obstacle_payload_t *payload) {
  static PI_FC_L1 int flow_count[FLOW_SECTORS];
  static PI_FC_L1 float flow_samples[FLOW_SECTORS][FLOW_FEATURES_PER_SECTOR];
  static PI_FC_L1 float radial_samples[FLOW_SECTORS][FLOW_FEATURES_PER_SECTOR];
  static PI_FC_L1 int radial_count[FLOW_SECTORS];
  const uint32_t total_start_us = time_get_us();
  uint32_t stage_start_us;
  float accepted_error_sum = 0.0f;
  float accepted_error_max = 0.0f;
  int accepted_tracks = 0;
  int rejected_tracks = 0;
  int n_features = 0;
  flow_debug_track_count = 0;

  memset(payload, 0, sizeof(*payload));
  payload->gap8_ts_us = frame_timestamp;
  payload->stm32_ts_echo = stm32_tick_at_gap8_time(frame_timestamp);
  payload->dt_s = FLOW_MIN_DT_S;
  payload->n_sectors = FLOW_SECTORS;
  payload->flags =
      (camera_get_recovery_count(&camera) > 0 ? 1u : 0u) |
      (camera_get_i2c_error_count(&camera) > 0 ? 2u : 0u);
  for (int i = 0; i < FLOW_SECTORS; i++) {
    const float center_x = ((float)i + 0.5f) * ((float)IMG_W / (float)FLOW_SECTORS);
    float center_q;
    float center_p;
    flow_undistort_normalized(center_x, FLOW_CY_PX, &center_q, &center_p);
    (void)center_p;
    payload->sector[i].azimuth_rad = center_q;
  }

  if (flow_have_prev && frame_timestamp > flow_prev_ts_us) {
    payload->dt_s = (frame_timestamp - flow_prev_ts_us) * 1.0e-6f;
    if (payload->dt_s < FLOW_MIN_DT_S) {
      payload->dt_s = FLOW_MIN_DT_S;
    }

    memset(flow_count, 0, sizeof(flow_count));
    memset(radial_count, 0, sizeof(radial_count));

    stage_start_us = time_get_us();
    build_half_pyramid(cur, flow_cur_half);
    flow_diag.pyramid_us = time_get_us() - stage_start_us;

    stage_start_us = time_get_us();
    n_features = select_shi_tomasi_features(flow_prev_frame, flow_features);
    flow_diag.select_us = time_get_us() - stage_start_us;

    stage_start_us = time_get_us();
    for (int k = 0; k < n_features; k++) {
      float nx = flow_features[k].x;
      float ny = flow_features[k].y;
      float err = 0.0f;
      if (!lk_track_pyramid(flow_prev_frame, cur,
                            flow_prev_half, flow_cur_half,
                            flow_features[k].x, flow_features[k].y,
                            &nx, &ny, &err)) {
        rejected_tracks++;
        continue;
      }
      float bx = nx;
      float by = ny;
      float fb_err = 0.0f;
      if (!lk_track_pyramid(cur, flow_prev_frame,
                            flow_cur_half, flow_prev_half,
                            nx, ny, &bx, &by, &fb_err) ||
          (bx - flow_features[k].x) * (bx - flow_features[k].x) +
          (by - flow_features[k].y) * (by - flow_features[k].y) >
              FLOW_FB_ERR_THRESH_PX * FLOW_FB_ERR_THRESH_PX) {
        rejected_tracks++;
        continue;
      }
      const float fb_geometric_error = sqrtf(
          (bx - flow_features[k].x) * (bx - flow_features[k].x) +
          (by - flow_features[k].y) * (by - flow_features[k].y));

      float dx = nx - flow_features[k].x;
      float dy = ny - flow_features[k].y;
      if (dx * dx + dy * dy < 0.0025f) {
        rejected_tracks++;
        continue;
      }
      accepted_tracks++;
      accepted_error_sum += err;
      if (err > accepted_error_max) {
        accepted_error_max = err;
      }
      if (flow_debug_track_count < FLOW_MAX_FEATURES) {
        flow_track_debug_t *track =
            &flow_debug_tracks[flow_debug_track_count++];
        track->x = flow_features[k].x;
        track->y = flow_features[k].y;
        track->nx = nx;
        track->ny = ny;
        track->lk_error = err;
        track->fb_error = fb_geometric_error;
        track->score = flow_features[k].score;
      }

      int sector = ((int)flow_features[k].x * FLOW_SECTORS) / IMG_W;
      if (sector >= FLOW_SECTORS) {
        sector = FLOW_SECTORS - 1;
      }
      float q0, p0, q1, p1;
      flow_undistort_normalized(flow_features[k].x, flow_features[k].y,
                                &q0, &p0);
      flow_undistort_normalized(nx, ny, &q1, &p1);
      if (flow_count[sector] < FLOW_FEATURES_PER_SECTOR) {
        flow_samples[sector][flow_count[sector]] = q1 - q0;
        flow_count[sector]++;
      }
      const float radius2 = q0 * q0 + p0 * p0;
      if (radius2 > 0.01f && radial_count[sector] < FLOW_FEATURES_PER_SECTOR) {
        const float qdot = (q1 - q0) / payload->dt_s;
        const float pdot = (p1 - p0) / payload->dt_s;
        radial_samples[sector][radial_count[sector]++] =
          (q0 * qdot + p0 * pdot) / radius2;
      }
    }
    flow_diag.track_us = time_get_us() - stage_start_us;

    stage_start_us = time_get_us();
    for (int i = 0; i < FLOW_SECTORS; i++) {
      if (flow_count[i] >= FLOW_MIN_SAMPLES) {
        float avg_flow_normalized =
            robust_near_sample(flow_samples[i], flow_count[i]);
        float flow_x_rad_s = avg_flow_normalized / payload->dt_s;
        const float sigma = flow_sample_sigma(
            flow_samples[i], flow_count[i], avg_flow_normalized,
            payload->dt_s);
        if (flow_x_rad_s > FLOW_MAX_RAD_S) {
          flow_x_rad_s = FLOW_MAX_RAD_S;
        } else if (flow_x_rad_s < -FLOW_MAX_RAD_S) {
          flow_x_rad_s = -FLOW_MAX_RAD_S;
        }
        payload->sector[i].flow_x_rad_s = flow_x_rad_s;
        payload->sector[i].flow_sigma_rad_s = sigma;
        payload->sector[i].flow_y_rad_s =
          radial_count[i] >= FLOW_MIN_SAMPLES ?
          robust_near_sample(radial_samples[i], radial_count[i]) : 0.0f;
        const float support = (float)flow_count[i] /
                              (float)FLOW_FEATURES_PER_SECTOR;
        const float signal = f_abs(flow_x_rad_s);
        const float snr_quality = signal * signal /
                                  (signal * signal + sigma * sigma + 1.0e-6f);
        payload->sector[i].confidence = support * snr_quality;
      } else {
        payload->sector[i].flow_x_rad_s = 0.0f;
        payload->sector[i].flow_y_rad_s = 0.0f;
        payload->sector[i].confidence = 0.0f;
      }
    }

    flow_filter_payload(payload);
    flow_diag.aggregate_us = time_get_us() - stage_start_us;
  } else {
    flow_diag.pyramid_us = 0;
    flow_diag.select_us = 0;
    flow_diag.track_us = 0;
    flow_diag.aggregate_us = 0;
    flow_diag.corner_max_score = 0;
  }

  flow_make_track_payload(frame_timestamp, payload->stm32_ts_echo,
                          payload->dt_s, payload->flags);

#if FLOW_DIAGNOSTIC_CAPTURE
  if (flow_have_prev && !flow_debug_pair_dumped) {
    memcpy(flow_cur_frame, cur, IMG_W * IMG_H_CAM);
    flow_dump_diagnostic_pair(flow_prev_ts_us, frame_timestamp);
    flow_debug_pair_dumped = true;
  }
#endif
  memcpy(flow_prev_frame, cur, IMG_W * IMG_H_CAM);
  build_half_pyramid(flow_prev_frame, flow_prev_half);
  flow_prev_ts_us = frame_timestamp;
  flow_have_prev = true;

  flow_diag.sequence++;
  flow_diag.frame_ts_us = frame_timestamp;
  flow_diag.frame_dt_us = (uint32_t)(payload->dt_s * 1000000.0f);
  flow_diag.total_us = time_get_us() - total_start_us;
  flow_diag.selected = (uint16_t)n_features;
  flow_diag.accepted = (uint16_t)accepted_tracks;
  flow_diag.rejected = (uint16_t)rejected_tracks;
  flow_diag.mean_error_milli = accepted_tracks > 0 ?
    (uint16_t)(accepted_error_sum * 1000.0f / (float)accepted_tracks) : 0;
  flow_diag.max_error_milli = (uint16_t)(accepted_error_max * 1000.0f);
  flow_diag.valid_sector_mask = 0;
  for (int i = 0; i < FLOW_SECTORS; i++) {
    if (payload->sector[i].confidence > 0.0f) {
      flow_diag.valid_sector_mask |= (uint16_t)(1u << i);
    }
  }

  if (flow_profile_skip_next) {
    flow_profile_skip_next = false;
  } else if (flow_diag.sequence > 1) {
    flow_profile.count++;
    profile_hist_add(flow_total_hist, flow_diag.total_us);
    flow_profile.total_sum_us += flow_diag.total_us;
    flow_profile.total_sum_sq_us +=
      (uint64_t)flow_diag.total_us * flow_diag.total_us;
    if (flow_diag.total_us < flow_profile.total_min_us ||
        flow_profile.total_min_us == 0) {
      flow_profile.total_min_us = flow_diag.total_us;
    }
    if (flow_diag.total_us > flow_profile.total_max_us) {
      flow_profile.total_max_us = flow_diag.total_us;
    }
    flow_profile.track_sum_us += flow_diag.track_us;
    flow_profile.track_sum_sq_us +=
      (uint64_t)flow_diag.track_us * flow_diag.track_us;
    if (flow_diag.track_us < flow_profile.track_min_us ||
        flow_profile.track_min_us == 0) {
      flow_profile.track_min_us = flow_diag.track_us;
    }
    if (flow_diag.track_us > flow_profile.track_max_us) {
      flow_profile.track_max_us = flow_diag.track_us;
    }
    flow_profile.dt_sum_us += flow_diag.frame_dt_us;
    flow_profile.dt_sum_sq_us +=
      (uint64_t)flow_diag.frame_dt_us * flow_diag.frame_dt_us;
    if (flow_diag.frame_dt_us < flow_profile.dt_min_us ||
        flow_profile.dt_min_us == 0) {
      flow_profile.dt_min_us = flow_diag.frame_dt_us;
    }
    if (flow_diag.frame_dt_us > flow_profile.dt_max_us) {
      flow_profile.dt_max_us = flow_diag.frame_dt_us;
    }
    flow_profile.selected_sum += flow_diag.selected;
    flow_profile.selected_sum_sq += flow_diag.selected * flow_diag.selected;
    flow_profile.accepted_sum += flow_diag.accepted;
    flow_profile.accepted_sum_sq += flow_diag.accepted * flow_diag.accepted;
    if (flow_diag.accepted > 0) {
      flow_profile.error_sum_milli += flow_diag.mean_error_milli;
      flow_profile.error_sum_sq_milli +=
        (uint64_t)flow_diag.mean_error_milli * flow_diag.mean_error_milli;
      if (flow_diag.mean_error_milli < flow_profile.error_min_milli ||
          flow_profile.error_min_milli == 0) {
        flow_profile.error_min_milli = flow_diag.mean_error_milli;
      }
      if (flow_diag.mean_error_milli > flow_profile.error_max_milli) {
        flow_profile.error_max_milli = flow_diag.mean_error_milli;
      }
      flow_profile.error_count++;
    }
    const uint32_t valid_sectors =
      (uint32_t)__builtin_popcount((unsigned)flow_diag.valid_sector_mask);
    flow_profile.valid_sector_sum += valid_sectors;
    flow_profile.valid_sector_sum_sq += valid_sectors * valid_sectors;
  }
}

static void flow_snapshot_frame_from_callback(const frame_t *camera_frame) {
  if (flow_snapshot_pending) {
    flow_snapshot_dropped++;
    return;
  }

  memcpy(flow_cur_frame, camera_frame->buffer, IMG_W * IMG_H_CAM);
  flow_snapshot_ts_us = camera_frame->frame_timestamp;
  flow_snapshot_count++;
  flow_snapshot_pending = true;
}

static void flow_background_poll(void) {
  if (!flow_snapshot_pending) {
    return;
  }

  bool had_prev = flow_have_prev;
  flow_compute_camera_payload(flow_cur_frame, flow_snapshot_ts_us, &flow_camera_payload);
  flow_processed_count++;
  flow_snapshot_pending = false;

  if (had_prev) {
    const uint16_t sequence = flow_wire_seq++;
    if (flow_wire_seq == 0) {
      flow_wire_seq = 1;
    }
    if (flow_tx_pending) {
      flow_tx_dropped++;
    } else {
      memcpy(&flow_tx_payload, &flow_camera_payload, sizeof(flow_tx_payload));
      flow_tx_payload.reserved = sequence;
      flow_tx_pending = true;
    }
    if (flow_track_tx_pending) {
      flow_tx_dropped++;
    } else {
      memcpy(&flow_track_tx_payload, &flow_track_camera_payload,
             sizeof(flow_track_tx_payload));
      flow_track_tx_payload.sequence = sequence;
      flow_track_tx_pending = true;
    }
  }
}

#endif

static void vision_uart_done(void *arg) {
  (void)arg;
  uart_completed_count++;
#if defined(FLOW_OBSTACLE_ENABLE) || defined(FLOW_OBSTACLE_CAMERA_TEST)
  if (uart_tx_is_flow) {
    flow_transmitted_count++;
  }
#endif
  uart_tx_busy = false;
}

static void vision_uart_service(void) {
  static pi_task_t done_task;

  /* Keep serialization off the camera/compute event loop. The L2 transmit
   * buffers remain owned by UART until this completion callback runs. */
  if (uart_tx_busy) {
    return;
  }

  if (gate8_tx_pending) {
    memcpy(&gate8_uart_msg, &gate8_tx_msg, sizeof(gate8_uart_msg));
    gate8_tx_pending = false;
    uart_tx_is_flow = false;
    uart_tx_busy = true;
    uart_queued_count++;
    uart_write_async(&uart, &gate8_uart_msg, sizeof(gate8_uart_msg),
                     pi_task_callback(&done_task, vision_uart_done, NULL));
    return;
  }
#if defined(FLOW_OBSTACLE_ENABLE) || defined(FLOW_OBSTACLE_CAMERA_TEST)
  if (flow_track_tx_pending) {
    flow_track_tx_pending = false;
    uart_tx_is_flow = true;
    uart_tx_busy = true;
    uart_queued_count++;
    flow_track_send_async(
      &uart, &flow_track_tx_payload,
      pi_task_callback(&done_task, vision_uart_done, NULL));
    return;
  }
  if (flow_tx_pending) {
    flow_tx_pending = false;
    uart_tx_is_flow = true;
    uart_tx_busy = true;
    uart_queued_count++;
    flow_obstacle_send_async(
      &uart, &flow_tx_payload,
      pi_task_callback(&done_task, vision_uart_done, NULL));
  }
#endif
}

#ifdef FLOW_OBSTACLE_TEST_ONLY
static void flow_obstacle_test_only_loop(void) {
  static PI_L2 flow_obstacle_payload_t flow_payload;
  static pi_task_t done_task;
  uint32_t n = 0;

  printf("flow obstacle UART-only test: sending synthetic sectors\n");
  while (true) {
    flow_obstacle_make_test_payload(&flow_payload, time_get_us(), 0, 0.1f);
    uart_queued_count++;
    flow_obstacle_send_async(&uart, &flow_payload, pi_task_block(&done_task));
    pi_task_wait_on(&done_task);
    uart_completed_count++;
    flow_transmitted_count++;
    if ((n++ % 10) == 0) {
      printf("flowObs test sent=%lu uart=%lu/%lu/%lu\n", n,
             uart_queued_count, uart_completed_count, uart_error_count);
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
#if !defined(FLOW_OBSTACLE_CAMERA_TEST) && !defined(CAMERA_CAPTURE_TEST_ONLY)
  static PI_FC_L1 bool started = false;
  static PI_FC_L1 inference_args_t iargs;
  static PI_FC_L1 co_event_t inference_done;
#endif

#ifdef CAMERA_CAPTURE_TEST_ONLY
  (void)camera_frame;
#elif defined(FLOW_OBSTACLE_CAMERA_TEST)
  static PI_FC_L1 uint32_t next_flow_ts_us = 0;
  if (next_flow_ts_us == 0 ||
      (int32_t)(camera_frame->frame_timestamp - next_flow_ts_us) >= 0) {
    if (next_flow_ts_us == 0) {
      next_flow_ts_us = camera_frame->frame_timestamp;
    }
    do {
      next_flow_ts_us += VISION_PERIOD_US;
    } while ((int32_t)(camera_frame->frame_timestamp - next_flow_ts_us) >= 0);
    flow_snapshot_frame_from_callback(camera_frame);
  }
#elif defined(FLOW_OBSTACLE_ENABLE)
  /*
   * Select frames on an accumulated 15 Hz deadline. Acquired camera frames
   * are not perfectly periodic, so parity-based alternation undersampled both
   * workloads. LK runs on the FC while CNN inference runs on the cluster.
   */
  static PI_FC_L1 uint32_t next_vision_ts_us = 0;
  if (next_vision_ts_us == 0 ||
      (int32_t)(camera_frame->frame_timestamp - next_vision_ts_us) >= 0) {
    if (next_vision_ts_us == 0) {
      next_vision_ts_us = camera_frame->frame_timestamp;
    }
    do {
      next_vision_ts_us += VISION_PERIOD_US;
    } while ((int32_t)(camera_frame->frame_timestamp - next_vision_ts_us) >= 0);

    if (started) {
      while (!co_event_is_done(&inference_done)) {
        CO_WAIT(&inference_done);
      }
    }
    flow_snapshot_frame_from_callback(camera_frame);
    resize_v_160_to_96(camera_frame->buffer, (uint8_t *)l2_buffer);
    iargs.stm32_timestamp = camera_frame->frame_timestamp;
    inference_busy = true;
    co_fn_push_start(&inference_ctx, inference_task, &iargs,
                     co_event_init(&inference_done));
    started = true;
  }
#else
  if (started) {
    while (!co_event_is_done(&inference_done)) {
      CO_WAIT(&inference_done);
    }
  }
  resize_v_160_to_96(camera_frame->buffer, (uint8_t *)l2_buffer);
  iargs.stm32_timestamp = camera_frame->frame_timestamp;
  inference_busy = true;
  co_fn_push_start(&inference_ctx, inference_task, &iargs,
                   co_event_init(&inference_done));
  started = true;
#endif
}
CO_FN_END()

CO_FN_BEGIN(inference_task, inference_args_t *, args)
{
  static PI_FC_L1 co_event_t done;
  static PI_FC_L1 float corners[N_CORNERS];
  static PI_FC_L1 uint32_t inference_start_us;

  trace_set(TRACE_USER_0, true);
  cnn_invocation_count++;
  inference_start_us = time_get_us();
  network_run_async_cl(l2_buffer, l2_buffer_size, l2_buffer, 0, 1, &cluster, co_event_init(&done));
  CO_WAIT(&done);
  cnn_last_us = time_get_us() - inference_start_us;
  if (cnn_last_us > cnn_max_us) {
    cnn_max_us = cnn_last_us;
  }
  cnn_completion_count++;
  if (cnn_profile_skip_next) {
    cnn_profile_skip_next = false;
  } else if (cnn_completion_count > 1) {
    cnn_profile_count++;
    profile_hist_add(cnn_time_hist, cnn_last_us);
    cnn_profile_sum_us += cnn_last_us;
    cnn_profile_sum_sq_us += (uint64_t)cnn_last_us * cnn_last_us;
    if (cnn_last_us < cnn_profile_min_us) {
      cnn_profile_min_us = cnn_last_us;
    }
    if (cnn_last_us > cnn_profile_max_us) {
      cnn_profile_max_us = cnn_last_us;
    }
  }
  trace_set(TRACE_USER_0, false);

  const int32_t *raw = (const int32_t *) l2_buffer;
  for (int i = 0; i < N_CORNERS; i++) {
    corners[i] = raw[i] * GATE8_EPS + GATE8_BIAS[i];
  }

  if (gate8_tx_pending) {
    gate8_tx_dropped++;
  } else {
    memcpy(gate8_tx_msg.header, GATE8_MSG_HEADER, 4);
    gate8_tx_msg.p.stm32_timestamp = args->stm32_timestamp;
    memcpy(gate8_tx_msg.p.corner, corners, sizeof(gate8_tx_msg.p.corner));
    gate8_tx_msg.checksum =
      crc32CalculateBuffer(&gate8_tx_msg, sizeof(gate8_tx_msg) -
                           sizeof(gate8_tx_msg.checksum));
    gate8_tx_pending = true;
  }
  inference_busy = false;

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

static const char *camera_stage_name(camera_stage_e stage) {
  switch (stage) {
    case CAMERA_STAGE_STOPPED:      return "stopped";
    case CAMERA_STAGE_WAIT_CAPTURE: return "capture";
    case CAMERA_STAGE_CROP:         return "crop";
    case CAMERA_STAGE_CONSUME:      return "consume";
    default:                        return "unknown";
  }
}

static uint32_t profile_stddev(uint64_t sum, uint64_t sum_sq, uint32_t count) {
  if (count == 0) {
    return 0;
  }
  const float mean = (float)sum / (float)count;
  const float mean_sq = (float)sum_sq / (float)count;
  const float variance = mean_sq > mean * mean ?
    mean_sq - mean * mean : 0.0f;
  return (uint32_t)(sqrtf(variance) + 0.5f);
}

static void diagnostics_heartbeat(void) {
  static PI_FC_L1 uint32_t last_print_us = 0;
  static PI_FC_L1 uint32_t last_flow_diag_sequence = 0;
  const uint32_t now = time_get_us();
  if (now - last_print_us < DIAGNOSTICS_PERIOD_US) {
    return;
  }
  if (inference_busy || uart_tx_busy || flow_snapshot_pending) {
    return;
  }
  last_print_us = now;

  printf("hb build=%s t_us=%lu cam=%lu hw=%u stage=%s age_ms=%lu rec=%lu i2c=%lu feat=%u"
         " target_fps=%u period_us=%u"
         " flow=%lu/%lu/%lu fd=%lu/%lu"
         " uart=%lu/%lu/%lu cnn=%lu/%lu/%lu/%lu\n",
         FLOW_BUILD_ID,
         now,
         camera_get_completed_capture_count(&camera),
         camera_get_hardware_frame_count(&camera),
         camera_stage_name(camera.stage),
         (now - camera.last_capture_us) / 1000u,
         camera_get_recovery_count(&camera),
         camera_get_i2c_error_count(&camera),
         (unsigned)FLOW_MAX_FEATURES,
         (unsigned)(HIMAX_FRAME_RATE + 0.5f),
         (unsigned)VISION_PERIOD_US,
         flow_snapshot_count, flow_processed_count, flow_transmitted_count,
         flow_snapshot_dropped, flow_tx_dropped,
         uart_queued_count, uart_completed_count, uart_error_count,
         cnn_invocation_count, cnn_completion_count, cnn_last_us, cnn_max_us);
#if defined(FLOW_OBSTACLE_ENABLE) || defined(FLOW_OBSTACLE_CAMERA_TEST)
  if (flow_diag.sequence != last_flow_diag_sequence) {
    last_flow_diag_sequence = flow_diag.sequence;
    printf("flowdiag,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%u,%u,%u,%u,%u,0x%03x\n",
           flow_diag.sequence, flow_diag.frame_ts_us, flow_diag.frame_dt_us,
           flow_diag.total_us, flow_diag.pyramid_us, flow_diag.select_us,
           flow_diag.track_us, flow_diag.aggregate_us,
           flow_diag.corner_max_score,
           (unsigned)flow_diag.selected, (unsigned)flow_diag.accepted,
           (unsigned)flow_diag.rejected, (unsigned)flow_diag.mean_error_milli,
           (unsigned)flow_diag.max_error_milli,
           (unsigned)flow_diag.valid_sector_mask);
  }
  if (flow_profile.count > 0 && cnn_profile_count > 0) {
    printf("profile,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,"
           "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
           flow_profile.count,
           (uint32_t)(flow_profile.total_sum_us / flow_profile.count),
           profile_stddev(flow_profile.total_sum_us,
                          flow_profile.total_sum_sq_us, flow_profile.count),
           flow_profile.total_min_us, flow_profile.total_max_us,
           (uint32_t)(flow_profile.track_sum_us / flow_profile.count),
           profile_stddev(flow_profile.track_sum_us,
                          flow_profile.track_sum_sq_us, flow_profile.count),
           flow_profile.track_min_us, flow_profile.track_max_us,
           (uint32_t)(flow_profile.dt_sum_us / flow_profile.count),
           profile_stddev(flow_profile.dt_sum_us,
                          flow_profile.dt_sum_sq_us, flow_profile.count),
           flow_profile.dt_min_us, flow_profile.dt_max_us,
           flow_profile.selected_sum / flow_profile.count,
           profile_stddev(flow_profile.selected_sum,
                          flow_profile.selected_sum_sq, flow_profile.count),
           flow_profile.accepted_sum / flow_profile.count,
           profile_stddev(flow_profile.accepted_sum,
                          flow_profile.accepted_sum_sq, flow_profile.count),
           flow_profile.error_count > 0 ?
             flow_profile.error_sum_milli / flow_profile.error_count : 0,
           profile_stddev(flow_profile.error_sum_milli,
                          flow_profile.error_sum_sq_milli,
                          flow_profile.error_count),
           flow_profile.error_min_milli, flow_profile.error_max_milli,
           flow_profile.valid_sector_sum / flow_profile.count,
           profile_stddev(flow_profile.valid_sector_sum,
                          flow_profile.valid_sector_sum_sq,
                          flow_profile.count),
           cnn_profile_count,
           (uint32_t)(cnn_profile_sum_us / cnn_profile_count),
           profile_stddev(cnn_profile_sum_us,
                          cnn_profile_sum_sq_us, cnn_profile_count),
           cnn_profile_min_us, cnn_profile_max_us);
    printf("quantile,%lu,%lu,%lu,%lu,%lu,%lu\n",
           flow_profile.count,
           profile_hist_percentile_us(flow_total_hist,
                                      flow_profile.count, 95u),
           profile_hist_percentile_us(flow_total_hist,
                                      flow_profile.count, 99u),
           cnn_profile_count,
           profile_hist_percentile_us(cnn_time_hist,
                                      cnn_profile_count, 95u),
           profile_hist_percentile_us(cnn_time_hist,
                                      cnn_profile_count, 99u));
  }
  memset(&flow_profile, 0, sizeof(flow_profile));
  memset(flow_total_hist, 0, sizeof(flow_total_hist));
  flow_profile_skip_next = true;
  cnn_profile_count = 0;
  cnn_profile_sum_us = 0;
  cnn_profile_sum_sq_us = 0;
  cnn_profile_min_us = UINT32_MAX;
  cnn_profile_max_us = 0;
  memset(cnn_time_hist, 0, sizeof(cnn_time_hist));
  cnn_profile_skip_next = true;
#endif
}

static void main_task(void) {
  soc_init();
  uart_init(&uart);
  uart_protocol_init(&uart_protocol, &uart, uart_state_callback);

#ifdef FLOW_OBSTACLE_TEST_ONLY
  flow_obstacle_test_only_loop();
#else
  camera_init(&camera, camera_callback);
  camera_init_frames_alloc(&camera);
#if !defined(FLOW_OBSTACLE_CAMERA_TEST) && !defined(CAMERA_CAPTURE_TEST_ONLY)
  cluster_init(&cluster);

#ifdef CNN_TIMING_NO_WEIGHTS
  printf("WARNING: CNN timing-only mode; predictions are invalid\n");
  mem_init_ram_only();
  network_initialize_timing_only();
#else
  mem_init();
  network_initialize();
#endif

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
#elif defined(FLOW_OBSTACLE_CAMERA_TEST)
  trace_init();
  printf("flow camera test: move laterally by hand in front of a textured obstacle\n");
  printf("flow camera test: sector angular flow is exposed as STM32 flowObsRx logs\n");
#else
  trace_init();
  printf("camera capture-only test\n");
#endif
  camera_start(&camera);
  uart_protocol_start(&uart_protocol);

  while (true) {
#if defined(FLOW_OBSTACLE_ENABLE) || defined(FLOW_OBSTACLE_CAMERA_TEST)
    flow_background_poll();
#endif
    vision_uart_service();
    /* A GAP8 cluster inference can keep the FC from servicing a pending CPI
     * completion for longer than the camera timeout. That is expected
     * backpressure, not a dead sensor; reconfiguring Himax here corrupted the
     * following inference/capture cycle. */
    if (!inference_busy) {
      camera_watchdog_poll(&camera);
    }
    diagnostics_heartbeat();
    pi_yield();
  }
#endif

  pmsis_exit(0);
}

int main(void) {
  return pmsis_kickoff((void *)main_task);
}
