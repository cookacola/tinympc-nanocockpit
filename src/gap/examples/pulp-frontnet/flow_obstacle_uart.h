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
#define FLOW_OBS_MSG_HEADER "\x90\x19\x8\x34"

typedef struct __attribute__((packed)) {
  float azimuth_rad;
  float inv_depth;
  float ttc_s;
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

void flow_obstacle_send_async(uart_t *uart,
                              const flow_obstacle_payload_t *payload,
                              pi_task_t *done_task);

void flow_obstacle_make_test_payload(flow_obstacle_payload_t *payload,
                                     uint32_t gap8_ts_us,
                                     uint32_t stm32_ts_echo,
                                     float dt_s);

#endif /* FLOW_OBSTACLE_UART_H */
