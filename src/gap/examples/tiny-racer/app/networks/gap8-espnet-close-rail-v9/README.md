# ESPNetV2 collision-v8 / close-rail-v9 INT8 release

This directory is the hardware release of the official collision-priority
ESPNetV2 model. It contains the exact NeMO integer graphs converted by DORY to
PULP-NN C for GAP8:

1. `collision_encoder`: three-channel temporal input to `32x40x40` features.
2. `collision_head`: features to left/center/right collision logits.
3. `gate_encoder`: static-pair input to private close-rail features.
4. `gate_head`: features to three rail-presence and three conditional
   pass-right logits.

The collision path remains the priority output. The gate branch is consumed
only after braking/near-hover and two consistent rail-direction observations.
Its flat output order is:

```text
rail_present_left, rail_present_center, rail_present_right,
pass_right_left, pass_right_center, pass_right_right
```

The rail-presence threshold is `0.45`; pass-right uses `0.50`. The recovery
sector is selected by the highest collision probability. Image right maps to
negative body yaw/lateral direction in the STM32 firmware.

`runtime/gap8_close_rail_packet.*` is the CRC-protected UART v6 wire contract
used to send all nine native probabilities to STM32. The receiver selects the
highest-risk collision sector first and only then consults that sector's rail
outputs; gate recovery therefore cannot outvote collision detection.

## GAP SDK setup

Source the GAP SDK environment for a GAP8 AI-deck before using these targets.
`RULES_DIR`, the RISC-V toolchain, HyperFlash and HyperRAM board definitions
must be available. The generated applications use ReadFS for their weights.

## Reproduce GVSOC validation

```sh
make gvsoc CORE=8
```

This builds and runs all four partitions with DORY checksum validation. The
promotion run passed all 18 partition checks and measured 8,999,158 cycles for
the gate encoder plus head (about 19.45 Hz at 175 MHz).

## Build board images

```sh
make all platform=board CORE=8
```

## Flash a partition validation image

```sh
make flash PARTITION=collision_encoder CORE=8
```

The four partition images are useful for hardware parity and bring-up. The
NanoCockpit camera application should link the generated `src`, `inc`, and
weight lists from each partition into its multi-partition runtime; do not run
an independently flashed head without supplying its feature tensor.

`contracts/` records quantization/decode metadata and the promotion manifest.
Run `make checksums` before release or flashing. Physical AI-deck flight is
still an acceptance step; GVSOC success is not represented as hardware proof.
