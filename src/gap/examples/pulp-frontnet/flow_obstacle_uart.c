/*
 * flow_obstacle_uart.c
 * GAP8 -> STM32 obstacle-flow sector UART framing.
 */
#include "flow_obstacle_uart.h"

#include "crc32.h"
#include "time.h"

#include <math.h>
#include <string.h>

static PI_L2 flow_obstacle_msg_t s_msg;

void flow_obstacle_send_async(uart_t *uart,
                              const flow_obstacle_payload_t *payload,
                              pi_task_t *done_task) {
  memcpy(s_msg.header, FLOW_OBS_MSG_HEADER, sizeof(s_msg.header));
  memcpy(&s_msg.p, payload, sizeof(s_msg.p));
  if (s_msg.p.n_sectors > FLOW_OBS_SECT_MAX) {
    s_msg.p.n_sectors = FLOW_OBS_SECT_MAX;
  }
  s_msg.checksum = crc32CalculateBuffer(&s_msg, sizeof(s_msg) - sizeof(s_msg.checksum));
  uart_write_async(uart, &s_msg, sizeof(s_msg), done_task);
}

void flow_obstacle_make_test_payload(flow_obstacle_payload_t *payload,
                                     uint32_t gap8_ts_us,
                                     uint32_t stm32_ts_echo,
                                     float dt_s) {
  memset(payload, 0, sizeof(*payload));
  payload->gap8_ts_us = gap8_ts_us ? gap8_ts_us : time_get_us();
  payload->stm32_ts_echo = stm32_ts_echo;
  payload->dt_s = dt_s;
  payload->n_sectors = 3;
  payload->flags = 0;

  payload->sector[0].azimuth_rad = -0.35f;
  payload->sector[0].inv_depth = 1.0f / 2.2f;
  payload->sector[0].ttc_s = 2.0f;
  payload->sector[0].confidence = 0.55f;

  payload->sector[1].azimuth_rad = 0.0f;
  payload->sector[1].inv_depth = 1.0f / 1.6f;
  payload->sector[1].ttc_s = 1.4f;
  payload->sector[1].confidence = 0.85f;

  payload->sector[2].azimuth_rad = 0.35f;
  payload->sector[2].inv_depth = 1.0f / 2.5f;
  payload->sector[2].ttc_s = 2.3f;
  payload->sector[2].confidence = 0.50f;
}
