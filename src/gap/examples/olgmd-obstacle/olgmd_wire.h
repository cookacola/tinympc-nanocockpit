#ifndef OLGMD_WIRE_H
#define OLGMD_WIRE_H

#include <stdint.h>

#define OLGMD_HEADER_SIZE 4
#define TINYRACER_THREAT_V1_HEADER "\x90\x19\x08\x40"
#define OLGMD_THREAT_HEADER TINYRACER_THREAT_V1_HEADER

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

#endif
