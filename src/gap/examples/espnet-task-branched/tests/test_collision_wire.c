#include "collision_wire.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    espnet_collision_packet_t packet;
    const float p[3] = {0.0f, 0.5f, 1.0f};
    espnet_collision_pack(&packet, 0x12345678u, 0x1234u, p, true);
    const uint8_t expected[18] = {
        0x90,0x19,0x08,0x43,0x78,0x56,0x34,0x12,0x34,0x12,
        0,0,0,0x40,0,0x80,1,0};
    assert(memcmp(&packet, expected, sizeof(expected)) == 0);
    assert(packet.checksum == crc32CalculateBuffer(expected, sizeof(expected)));
    assert(crc32CalculateBuffer("123456789", 9) == 0xcbf43926u);
    const float clamp[3] = {-0.1f, 1.1f, 0.5f / 32768.0f};
    espnet_collision_pack(&packet, 1, 1, clamp, true);
    assert(packet.payload.probability_q15[0] == 0);
    assert(packet.payload.probability_q15[1] == 32768);
    assert(packet.payload.probability_q15[2] == 1);
    for (unsigned sector = 0; sector < 3; ++sector) {
        float invalid[3] = {0.1f, 0.5f, 0.9f};
        invalid[sector] = NAN;
        espnet_collision_pack(&packet, 1, 2, invalid, true);
        assert(packet.payload.valid == 0);
        for (unsigned i = 0; i < 3; ++i)
            assert(packet.payload.probability_q15[i] == 0);
        invalid[sector] = INFINITY;
        espnet_collision_pack(&packet, 1, 2, invalid, true);
        assert(packet.payload.valid == 0);
    }
    espnet_collision_pack(&packet, 1, 1, p, false);
    assert(packet.payload.valid == 0 && packet.payload.reserved == 0);
    for (unsigned i = 0; i < 3; ++i)
        assert(packet.payload.probability_q15[i] == 0);
    assert(espnet_collision_next_sequence(0) == 1);
    assert(espnet_collision_next_sequence(65534) == 65535);
    assert(espnet_collision_next_sequence(65535) == 1);
    puts("ESPNet collision wire tests PASS");
}
