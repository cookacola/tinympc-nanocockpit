#ifndef OLGMD_WIRE_H
#define OLGMD_WIRE_H

#include <stdint.h>

#define OLGMD_HEADER_SIZE 4
#define TINYRACER_THREAT_V1_HEADER "\x90\x19\x08\x40"
#define OLGMD_THREAT_HEADER TINYRACER_THREAT_V1_HEADER
#define OLGMD_DIAGNOSTIC_HEADER "\x90\x19\x08\x42"

typedef struct __attribute__((packed)) {
  uint32_t source_timestamp_ms;
  uint16_t frame_sequence;
  uint8_t imminent_threat;
  uint8_t reserved;
} olgmd_threat_payload_t;

typedef struct __attribute__((packed)) {
  uint8_t header[OLGMD_HEADER_SIZE];
  olgmd_threat_payload_t payload;
  uint32_t checksum;
} olgmd_threat_packet_t;

_Static_assert(sizeof(olgmd_threat_payload_t) == 8,
               "threat payload ABI changed");
_Static_assert(sizeof(olgmd_threat_packet_t) == 16,
               "threat packet ABI changed");

/* Independent diagnostic packet; the legacy threat/control ABI is unchanged. */
typedef struct __attribute__((packed)) {
  uint32_t source_timestamp_ms;
  uint16_t frame_sequence;
  uint16_t membrane_q15;
  uint8_t spike_count;
  uint8_t imminent_threat;
  uint8_t valid;
  uint8_t reserved;
} olgmd_diagnostic_payload_t;

typedef struct __attribute__((packed)) {
  uint8_t header[OLGMD_HEADER_SIZE];
  olgmd_diagnostic_payload_t payload;
  uint32_t checksum;
} olgmd_diagnostic_packet_t;

_Static_assert(sizeof(olgmd_diagnostic_payload_t) == 12,
               "diagnostic payload ABI changed");
_Static_assert(sizeof(olgmd_diagnostic_packet_t) == 20,
               "diagnostic packet ABI changed");

#endif
