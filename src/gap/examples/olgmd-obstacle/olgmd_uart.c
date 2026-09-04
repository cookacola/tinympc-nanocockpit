#include "olgmd_uart.h"

#include "crc32.h"

#include <string.h>

static PI_L2 olgmd_threat_packet_t threat_packet;

void olgmd_send_threat_async(
    uart_t *uart, const olgmd_threat_payload_t *payload,
    pi_task_t *done_task) {
  memcpy(threat_packet.header, OLGMD_THREAT_HEADER, OLGMD_HEADER_SIZE);
  threat_packet.payload = *payload;
  threat_packet.checksum = crc32CalculateBuffer(
      &threat_packet, sizeof(threat_packet) - sizeof(threat_packet.checksum));
  uart_write_async(uart, &threat_packet, sizeof(threat_packet), done_task);
}
