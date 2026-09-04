#include "crc32.h"
#include "olgmd_wire.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  olgmd_threat_packet_t threat = {0};
  memcpy(threat.header, OLGMD_THREAT_HEADER, OLGMD_HEADER_SIZE);
  threat.payload.source_timestamp_ms = 0x11223344u;
  threat.payload.frame_sequence = 0x5566u;
  threat.payload.imminent_threat = 1u;
  threat.checksum = crc32CalculateBuffer(
      &threat, sizeof(threat) - sizeof(threat.checksum));

  const uint8_t prefix[] = {
      0x90, 0x19, 0x08, 0x40, 0x44, 0x33,
      0x22, 0x11, 0x66, 0x55, 0x01, 0x00,
  };
  assert(sizeof(threat) == 16u);
  assert(offsetof(olgmd_threat_packet_t, payload) == 4u);
  assert(offsetof(olgmd_threat_packet_t, checksum) == 12u);
  assert(memcmp(&threat, prefix, sizeof(prefix)) == 0);
  assert(threat.checksum == 0x416de1dcu);
  printf("oLGMD obstacle wire test passed\n");
  return 0;
}
