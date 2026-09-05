#pragma once
#include "collision_wire.h"
#include "uart.h"

/* packet must be in L2 and stay unchanged until done_task completes. */
void espnet_collision_send_async(uart_t *uart,
    espnet_collision_packet_t *packet, uint32_t source_timestamp_ms,
    uint16_t sequence, const float probability[3], bool temporal_valid,
    pi_task_t *done_task);
