#include "perception_map_uart.h"

#include "crc32.h"

#include <string.h>

static PI_L2 perception_map_msg_t s_map_msg;

static void set_nibble(uint8_t *packed, int index, uint8_t value) {
  const int byte = index >> 1;
  const uint8_t nibble = value & 0x0f;
  if (index & 1) {
    packed[byte] = (packed[byte] & 0x0f) | (uint8_t)(nibble << 4);
  } else {
    packed[byte] = (packed[byte] & 0xf0) | nibble;
  }
}

static uint8_t max_2x2_u4(const uint8_t map[400], int ox, int oy) {
  uint8_t maximum = 0;
  for (int dy = 0; dy < 2; ++dy) {
    for (int dx = 0; dx < 2; ++dx) {
      const uint8_t value = map[(2 * oy + dy) * 20 + 2 * ox + dx];
      if (value > maximum) maximum = value;
    }
  }
  return maximum >> 4;
}

static uint8_t min_2x2_u4(const uint8_t map[400], int ox, int oy) {
  uint8_t minimum = 255;
  for (int dy = 0; dy < 2; ++dy) {
    for (int dx = 0; dx < 2; ++dx) {
      const uint8_t value = map[(2 * oy + dy) * 20 + 2 * ox + dx];
      if (value < minimum) minimum = value;
    }
  }
  return minimum >> 4;
}

void perception_map_pack(perception_map_payload_t *payload,
                         const uint8_t obstacle_presence_20[400],
                         const uint8_t inverse_range_20[400],
                         const uint8_t uncertainty_20[400],
                         const uint8_t gate_opening_20[400],
                         uint32_t gap8_ts_us,
                         uint32_t stm32_ts_echo,
                         uint16_t sequence) {
  memset(payload, 0, sizeof(*payload));
  payload->gap8_ts_us = gap8_ts_us;
  payload->stm32_ts_echo = stm32_ts_echo;
  payload->sequence = sequence;
  payload->version = PERCEPTION_MAP_WIRE_VERSION;
  payload->width = PERCEPTION_MAP_W;
  payload->height = PERCEPTION_MAP_H;
  for (int y = 0; y < PERCEPTION_MAP_H; ++y) {
    for (int x = 0; x < PERCEPTION_MAP_W; ++x) {
      const int cell = y * PERCEPTION_MAP_W + x;
      set_nibble(payload->packed_u4, 4 * cell,
                 max_2x2_u4(obstacle_presence_20, x, y));
      set_nibble(payload->packed_u4, 4 * cell + 1,
                 max_2x2_u4(inverse_range_20, x, y));
      set_nibble(payload->packed_u4, 4 * cell + 2,
                 max_2x2_u4(uncertainty_20, x, y));
      set_nibble(payload->packed_u4, 4 * cell + 3,
                 min_2x2_u4(gate_opening_20, x, y));
    }
  }
}

void perception_map_send_async(uart_t *uart,
                               const perception_map_payload_t *payload,
                               pi_task_t *done_task) {
  memcpy(s_map_msg.header, PERCEPTION_MAP_MSG_HEADER,
         sizeof(s_map_msg.header));
  memcpy(&s_map_msg.p, payload, sizeof(s_map_msg.p));
  s_map_msg.checksum = crc32CalculateBuffer(
      &s_map_msg, sizeof(s_map_msg) - sizeof(s_map_msg.checksum));
  uart_write_async(uart, &s_map_msg, sizeof(s_map_msg), done_task);
}
