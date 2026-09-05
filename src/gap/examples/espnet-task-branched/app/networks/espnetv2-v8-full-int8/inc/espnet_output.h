#ifndef ESPNET_OUTPUT_H
#define ESPNET_OUTPUT_H
#include <stdint.h>
#define ESPNET_INPUT_BYTES 76800
#define ESPNET_FEATURE_BYTES 51200
#define ESPNET_CORNER_BYTES 1600
#define ESPNET_GATE_OFFSET 1600
#define ESPNET_COLLISION_OFFSET 1605
#define ESPNET_OUTPUT_BYTES 1608
#define ESPNET_WORKSPACE_BYTES 260000
/* Input HWC [160,160,3]: previous, current, (current-previous+255)/2.
 * Corners HWC [20,20,4]: LT, RT, LB, RB.
 * Gate: none, opening_left, opening_right, left_rail, right_rail.
 * Collision: left, center, right (native model order).
 * Teacher logit = uint8 * EPSILON - offset[channel].
 * offset below folds NeMO offset - learned_bias + teacher_logit_offset.
 */
#define ESPNET_CORNER_EPSILON 0.04822007566690445f
static const float espnet_corner_offset[4] = {0.7123202811241149f, 0.9575483933448792f, 2.7920050343036653f, 3.4544905737400056f};
#define ESPNET_GATE_EPSILON 0.04027872160077095f
static const float espnet_gate_offset[5] = {6.50242458114624f, 7.469318254089355f, 8.130596978759765f, 5.095180852508545f, 5.33536611328125f};
#define ESPNET_COLLISION_EPSILON 0.1041213646531105f
static const float espnet_collision_offset[3] = {10.90048013458252f, 14.667605264282226f, 12.854562623596191f};
#endif
