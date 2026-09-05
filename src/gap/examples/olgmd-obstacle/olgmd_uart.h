#ifndef OLGMD_UART_H
#define OLGMD_UART_H

#include "olgmd_wire.h"
#include "uart.h"

void olgmd_send_threat_async(
    uart_t *uart, const olgmd_threat_payload_t *payload,
    pi_task_t *done_task);

void olgmd_send_diagnostic_async(
    uart_t *uart, const olgmd_diagnostic_payload_t *payload,
    pi_task_t *done_task);

#endif
