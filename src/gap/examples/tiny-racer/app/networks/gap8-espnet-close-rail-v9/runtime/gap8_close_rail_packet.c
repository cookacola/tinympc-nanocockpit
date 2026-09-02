#include "gap8_close_rail_packet.h"

#include "crc32.h"

#include <math.h>
#include <string.h>

static bool probabilityVectorValid(const float values[3]) {
  for (int sector = 0; sector < 3; ++sector) {
    if (!isfinite(values[sector]) || values[sector] < 0.0f ||
        values[sector] > 1.0f) {
      return false;
    }
  }
  return true;
}

bool gap8CloseRailV6Pack(Gap8CloseRailV6Packet *packet,
                         uint32_t source_timestamp_ms, uint16_t sequence,
                         const float collision_probability[3],
                         const float rail_present_probability[3],
                         const float pass_right_probability[3]) {
  if (packet == NULL || sequence == 0u ||
      !probabilityVectorValid(collision_probability) ||
      !probabilityVectorValid(rail_present_probability) ||
      !probabilityVectorValid(pass_right_probability)) {
    return false;
  }
  memset(packet, 0, sizeof(*packet));
  memcpy(packet->header, GAP8_CLOSE_RAIL_V6_HEADER,
         GAP8_CLOSE_RAIL_V6_HEADER_LEN);
  packet->payload.source_timestamp_ms = source_timestamp_ms;
  packet->payload.sequence = sequence;
  packet->payload.flags = GAP8_CLOSE_RAIL_HAS_COLLISION |
                          GAP8_CLOSE_RAIL_HAS_RECOVERY;
  memcpy(packet->payload.collision_probability, collision_probability,
         sizeof(packet->payload.collision_probability));
  memcpy(packet->payload.rail_present_probability, rail_present_probability,
         sizeof(packet->payload.rail_present_probability));
  memcpy(packet->payload.pass_right_probability, pass_right_probability,
         sizeof(packet->payload.pass_right_probability));
  packet->checksum = crc32CalculateBuffer(
      packet, sizeof(*packet) - sizeof(packet->checksum));
  return true;
}
