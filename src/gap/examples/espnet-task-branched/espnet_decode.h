#ifndef ESPNET_DECODE_H
#define ESPNET_DECODE_H
#include <stdint.h>
typedef struct {
    float collision[3], affordance[3], visibility[2];
    float corners[8]; /* native LT, RT, LB, RB in 160x160 coordinates */
    uint8_t visible[2];
} espnet_decoded_t;
void espnet_decode(const uint8_t *packed, espnet_decoded_t *decoded);
#endif
