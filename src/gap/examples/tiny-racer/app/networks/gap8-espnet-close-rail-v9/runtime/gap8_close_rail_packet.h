#ifndef GAP8_CLOSE_RAIL_PACKET_H
#define GAP8_CLOSE_RAIL_PACKET_H

#include <stdbool.h>
#include <stdint.h>

#define GAP8_CLOSE_RAIL_V6_HEADER "\x90\x19\x08\x3d"
#define GAP8_CLOSE_RAIL_V6_HEADER_LEN 4
#define GAP8_CLOSE_RAIL_HAS_COLLISION (1u << 0)
#define GAP8_CLOSE_RAIL_HAS_RECOVERY (1u << 1)

typedef struct __attribute__((packed)) {
  uint32_t source_timestamp_ms;
  uint16_t sequence;
  uint16_t flags;
  float collision_probability[3];
  float rail_present_probability[3];
  float pass_right_probability[3];
} Gap8CloseRailV6Payload;

typedef struct __attribute__((packed)) {
  uint8_t header[GAP8_CLOSE_RAIL_V6_HEADER_LEN];
  Gap8CloseRailV6Payload payload;
  uint32_t checksum;
} Gap8CloseRailV6Packet;

_Static_assert(sizeof(Gap8CloseRailV6Packet) == 52,
               "GAP8 close-rail v6 packet ABI changed");

bool gap8CloseRailV6Pack(Gap8CloseRailV6Packet *packet,
                         uint32_t source_timestamp_ms, uint16_t sequence,
                         const float collision_probability[3],
                         const float rail_present_probability[3],
                         const float pass_right_probability[3]);

#endif
