# GAP8 sequential QAT DORY package

This is the generated DORY GAP8 application for the fresh two-flight
(`flight_06`, `flight_07`) sequential QAT model. It is packaged for selection
by Tiny Racer via `NETWORK_NAME=gap8-sequential-bothflights-qat-dory`.

## ABI

- Input: unsigned HM01B0 grayscale, NCHW `[1, 1, 120, 160]` (19,200 bytes).
- Output: unsigned HWC `[15, 20, 12]` (3,600 bytes).
- Output interpretation: `logical_score = uint8 * 0.0631349534 - 6.0`.
- Channels 0–3 are ordered corner heatmaps; 4–7 are fixed-normal clearance
  scores; 8–11 are directional confidence scores.

`src/`, `inc/`, `hex/`, `Makefile`, and `gap8_vars.mk` are the direct DORY
output. `artifacts/` carries the source integer ONNX and export reports.

## Build

With the GAP SDK and RISC-V toolchain configured, run from the Tiny Racer
example directory:

```bash
make NETWORK_NAME=gap8-sequential-bothflights-qat-dory CORE=8 build image
```

The Tiny Racer adapter excludes DORY's standalone `gap8_main.c` and maps the
generated `gap8_network_*` API to the standard `network_*` interface.
Tiny Racer decodes the canonical 12-channel ABI on GAP8: integer corner argmax
and ambiguity checks, plus spatially averaged fixed-normal offsets and
confidence scores. The resulting STM32 half-space/TinyMPC integration remains
outside this GAP8 package.

The Tiny Racer adapter canonicalizes the deployed terminal corner pairs from
the observed DORY order `TR, TL, BL, BR` to the model contract
`TL, TR, BR, BL`. The directional clearance and confidence channels retain
their exported order.

Gate validation accepts three corner heatmaps with peak score at least `-0.5`
and ambiguity at least `0.12`. Three-corner candidates retain the 50 px² area
floor and allow an 8:1 side ratio; four-corner candidates retain the stricter
100 px² and 6:1 checks. The v12 streamer's former padding bytes carry the
rejection reason and confident-corner mask without changing its wire size.

## Validation status

- NEMO integer versus ONNX Runtime: exact parity, 0/3,600 differing elements.
- DORY frontend/lowering: passed; 24 fused layers, 28,152,000 MACs, estimated
  peak L1 tile 36,417 B (below the 64 kB GAP8 limit). The folded BN affine
  calculation uses DORY's 64-bit intermediate because layer 12 exceeds signed
  int32 on the validated fixture.
- Tiny Racer GAP8 build: passed with 101,860 B L2 use (19.43%).
- GVSOC: not passed. The installed launcher aborts before application output;
  this is distinct from the successful 24-layer hardware CRC diagnostic for
  the same 64-bit DORY path.
