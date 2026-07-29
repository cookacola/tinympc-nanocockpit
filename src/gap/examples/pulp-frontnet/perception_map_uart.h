/*
 * Compact GAP8 CNN control-map transport.
 *
 * The deployed CNN retains 20x20 maps on GAP8. For the 115200-baud shared
 * UART, each 2x2 block is max-pooled once more and three uint4 values are
 * packed per 10x10 cell: collision probability, inverse range, uncertainty,
 * and gate-opening probability.
 */
#ifndef PERCEPTION_MAP_UART_H
#define PERCEPTION_MAP_UART_H

#include "uart.h"

#include <pmsis.h>
#include <stdint.h>

#define PERCEPTION_MAP_MSG_HEADER "\x90\x19\x8\x37"
#define PERCEPTION_MAP_W 10
#define PERCEPTION_MAP_H 10
#define PERCEPTION_MAP_CHANNELS 4
#define PERCEPTION_MAP_PACKED_BYTES 200
#define PERCEPTION_MAP_WIRE_VERSION 2
#define PERCEPTION_MAP_FLAG_DANGER_RAW_U8 (1u << 0)

typedef struct __attribute__((packed)) {
  uint32_t gap8_ts_us;
  uint32_t stm32_ts_echo;
  uint16_t sequence;
  uint8_t version;
  uint8_t width;
  uint8_t height;
  uint8_t flags;
  uint8_t packed_u4[PERCEPTION_MAP_PACKED_BYTES];
} perception_map_payload_t;

typedef struct __attribute__((packed)) {
  uint8_t header[4];
  perception_map_payload_t p;
  uint32_t checksum;
} perception_map_msg_t;

_Static_assert(sizeof(perception_map_payload_t) == 214,
               "perception map payload ABI changed");
_Static_assert(sizeof(perception_map_msg_t) == 222,
               "perception map packet ABI changed");

void perception_map_pack(perception_map_payload_t *payload,
                         const uint8_t obstacle_presence_20[400],
                         const uint8_t inverse_range_20[400],
                         const uint8_t uncertainty_20[400],
                         const uint8_t gate_opening_20[400],
                         uint32_t gap8_ts_us,
                         uint32_t stm32_ts_echo,
                         uint16_t sequence);
void perception_map_send_async(uart_t *uart,
                               const perception_map_payload_t *payload,
                               pi_task_t *done_task);

#endif
