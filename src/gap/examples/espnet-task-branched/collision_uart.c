#include "collision_uart.h"

void espnet_collision_send_async(uart_t *uart,
    espnet_collision_packet_t *packet, uint32_t source_timestamp_ms,
    uint16_t sequence, const float probability[3], bool temporal_valid,
    pi_task_t *done_task) {
    espnet_collision_pack(packet, source_timestamp_ms, sequence,
                          probability, temporal_valid);
    uart_write_async(uart, packet, sizeof(*packet), done_task);
}
