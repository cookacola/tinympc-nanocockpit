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
  olgmd_diagnostic_packet_t diagnostic = {0};
  memcpy(diagnostic.header, OLGMD_DIAGNOSTIC_HEADER, OLGMD_HEADER_SIZE);
  diagnostic.payload.source_timestamp_ms = 0x11223344u;
  diagnostic.payload.frame_sequence = 0x5566u;
  diagnostic.payload.membrane_q15 = 0x6000u; /* 0.75 */
  diagnostic.payload.spike_count = 3u;
  diagnostic.payload.imminent_threat = 1u;
  diagnostic.payload.valid = 1u;
  diagnostic.checksum = crc32CalculateBuffer(
      &diagnostic, sizeof(diagnostic) - sizeof(diagnostic.checksum));

  const uint8_t diagnostic_bytes[] = {
      0x90, 0x19, 0x08, 0x42, 0x44, 0x33, 0x22, 0x11,
      0x66, 0x55, 0x00, 0x60, 0x03, 0x01, 0x01, 0x00,
      0x7d, 0xe2, 0x30, 0x0c,
  };
  assert(sizeof(diagnostic) == 20u);
  assert(offsetof(olgmd_diagnostic_packet_t, payload) == 4u);
  assert(offsetof(olgmd_diagnostic_packet_t, checksum) == 16u);
  assert(memcmp(&diagnostic, diagnostic_bytes, sizeof(diagnostic_bytes)) == 0);
  assert(diagnostic.checksum == 0x0c30e27du);
  /* A changed activity value must not pass the original packet CRC. */
  diagnostic.payload.spike_count ^= 1u;
  assert(crc32CalculateBuffer(&diagnostic, 16u) != diagnostic.checksum);
  printf("oLGMD obstacle threat and diagnostic wire tests passed\n");
  return 0;
}
