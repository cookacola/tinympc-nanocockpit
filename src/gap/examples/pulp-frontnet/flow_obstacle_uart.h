/*
 * flow_obstacle_uart.h
 * GAP8 -> STM32 obstacle-flow sector wire message.
 */
#ifndef FLOW_OBSTACLE_UART_H
#define FLOW_OBSTACLE_UART_H

#include "uart.h"

#include <pmsis.h>
#include <stdint.h>

#define FLOW_OBS_SECT_MAX 9
#define FLOW_OBS_MSG_HEADER "\x90\x19\x8\x35"
#define FLOW_TRACK_MAX 32
#define FLOW_TRACK_WIRE_VERSION 1
#define FLOW_TRACK_MSG_HEADER "\x90\x19\x8\x36"

typedef struct __attribute__((packed)) {
  float azimuth_rad;
  float flow_x_rad_s;
  float flow_y_rad_s;
  float flow_sigma_rad_s;
  float confidence;
} flow_obstacle_sector_t;

typedef struct __attribute__((packed)) {
  uint32_t gap8_ts_us;
  uint32_t stm32_ts_echo;
  float dt_s;
  uint8_t n_sectors;
  uint8_t flags;
  uint16_t reserved;
  flow_obstacle_sector_t sector[FLOW_OBS_SECT_MAX];
} flow_obstacle_payload_t;

typedef struct __attribute__((packed)) {
  uint8_t header[4];
  flow_obstacle_payload_t p;
  uint32_t checksum;
} flow_obstacle_msg_t;

/* Fixed-point feature tracks preserve correspondence until the STM32 has
 * capture-time motion available. Pixel positions use Q12.4, displacements use
 * signed Q7.8, and the two residuals use unsigned Q8.8 pixels. */
typedef struct __attribute__((packed)) {
  uint16_t u_q4;
  uint16_t v_q4;
  int16_t du_q8;
  int16_t dv_q8;
  uint16_t lk_err_q8;
  uint16_t fb_err_q8;
} flow_track_wire_t;

typedef struct __attribute__((packed)) {
  uint32_t gap8_ts_us;
  uint32_t stm32_ts_echo;
  uint16_t sequence;
  uint16_t dt_us;
  uint8_t version;
  uint8_t count;
  uint8_t flags;
  uint8_t reserved;
  flow_track_wire_t track[FLOW_TRACK_MAX];
} flow_track_payload_t;

typedef struct __attribute__((packed)) {
  uint8_t header[4];
  flow_track_payload_t p;
  uint32_t checksum;
} flow_track_msg_t;

_Static_assert(sizeof(float) == 4, "vision wire protocol requires float32");
_Static_assert(sizeof(flow_obstacle_payload_t) == 196,
               "flow payload ABI changed");
_Static_assert(sizeof(flow_obstacle_msg_t) == 204,
               "flow packet ABI changed");
_Static_assert(sizeof(flow_track_wire_t) == 12,
               "flow track ABI changed");
_Static_assert(sizeof(flow_track_payload_t) == 400,
               "flow track payload ABI changed");
_Static_assert(sizeof(flow_track_msg_t) == 408,
               "flow track packet ABI changed");

void flow_obstacle_send_async(uart_t *uart,
                              const flow_obstacle_payload_t *payload,
                              pi_task_t *done_task);
void flow_track_send_async(uart_t *uart,
                           const flow_track_payload_t *payload,
                           pi_task_t *done_task);

void flow_obstacle_make_test_payload(flow_obstacle_payload_t *payload,
                                     uint32_t gap8_ts_us,
                                     uint32_t stm32_ts_echo,
                                     float dt_s);

#endif /* FLOW_OBSTACLE_UART_H */
