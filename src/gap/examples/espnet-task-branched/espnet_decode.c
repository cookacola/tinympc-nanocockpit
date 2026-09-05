#include "espnet_decode.h"
#include "espnet_output.h"
#include <math.h>
static float sigmoid(float x) { return 1.0f / (1.0f + expf(-x)); }
void espnet_decode(const uint8_t *packed, espnet_decoded_t *decoded) {
    float gate[5], maximum = -1e30f, total = 0;
    for (int i = 0; i < 3; ++i)
        decoded->collision[i] = sigmoid(packed[ESPNET_COLLISION_OFFSET + i] *
            ESPNET_COLLISION_EPSILON - espnet_collision_offset[i]);
    for (int i = 0; i < 5; ++i) {
        gate[i] = packed[ESPNET_GATE_OFFSET + i] *
            ESPNET_GATE_EPSILON - espnet_gate_offset[i];
        if (i < 3 && gate[i] > maximum) maximum = gate[i];
    }
    for (int i = 0; i < 3; ++i) {
        decoded->affordance[i] = expf(gate[i] - maximum);
        total += decoded->affordance[i];
    }
    for (int i = 0; i < 3; ++i) decoded->affordance[i] /= total;
    for (int i = 0; i < 2; ++i) {
        decoded->visibility[i] = sigmoid(gate[i + 3]);
        decoded->visible[i] = decoded->visibility[i] >= 0.5f;
    }
    for (int c = 0; c < 4; ++c) {
        int peak = 0;
        for (int p = 1; p < 400; ++p)
            if (packed[p * 4 + c] > packed[peak * 4 + c]) peak = p;
        decoded->corners[2 * c] = (peak % 20) * (159.0f / 19.0f);
        decoded->corners[2 * c + 1] = (peak / 20) * (159.0f / 19.0f);
    }
}
