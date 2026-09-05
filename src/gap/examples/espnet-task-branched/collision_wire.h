#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include "crc32.h"

/* ESPNet sector telemetry only. Multi-byte fields are little-endian. */
#define ESPNET_COLLISION_HEADER_BYTES {0x90, 0x19, 0x08, 0x43}
typedef struct __attribute__((packed)) {
    uint32_t source_timestamp_ms;
    uint16_t frame_sequence;
    uint16_t probability_q15[3]; /* left, center, right; 32768 == 1.0 */
    uint8_t valid;
    uint8_t reserved;
} espnet_collision_payload_t;

typedef struct __attribute__((packed)) {
    uint8_t header[4];
    espnet_collision_payload_t payload;
    uint32_t checksum;
} espnet_collision_packet_t;
_Static_assert(sizeof(espnet_collision_payload_t) == 14, "collision payload ABI");
_Static_assert(sizeof(espnet_collision_packet_t) == 22, "collision packet ABI");
_Static_assert(offsetof(espnet_collision_packet_t, checksum) == 18, "CRC offset");

static inline uint16_t espnet_collision_next_sequence(uint16_t previous) {
    return previous == UINT16_MAX ? 1 : (uint16_t)(previous + 1);
}

/* Call only after the preceding UART DMA has completed. */
static inline void espnet_collision_pack(espnet_collision_packet_t *packet,
        uint32_t source_timestamp_ms, uint16_t sequence,
        const float probability[3], bool temporal_valid) {
    static const uint8_t header[4] = ESPNET_COLLISION_HEADER_BYTES;
    memset(packet, 0, sizeof(*packet));
    memcpy(packet->header, header, sizeof(header));
    packet->payload.source_timestamp_ms = source_timestamp_ms;
    packet->payload.frame_sequence = sequence;
    bool valid = temporal_valid;
    for (unsigned i = 0; i < 3; ++i)
        valid = valid && isfinite(probability[i]);
    packet->payload.valid = valid ? 1 : 0;
    if (valid) {
        for (unsigned i = 0; i < 3; ++i) {
            float value = probability[i];
            if (value < 0.0f) value = 0.0f;
            if (value > 1.0f) value = 1.0f;
            packet->payload.probability_q15[i] =
                (uint16_t)(value * 32768.0f + 0.5f);
        }
    }
    packet->checksum = crc32CalculateBuffer(packet,
        offsetof(espnet_collision_packet_t, checksum));
}
