# GAP8 sequential QAT DORY package

This is the generated DORY GAP8 application for the fresh two-flight
(`flight_06`, `flight_07`) sequential QAT model. It is intentionally kept as
a standalone generated network package for AI-deck testing; it is **not** wired
into the existing Frontnet controller decoder.

## ABI

- Input: unsigned HM01B0 grayscale, NCHW `[1, 1, 120, 160]` (19,200 bytes).
- Output: unsigned HWC `[15, 20, 12]` (3,600 bytes).
- Output interpretation: `logical_score = uint8 * 0.1130785942 - 6.0`.
- Channels 0–3 are ordered corner heatmaps; 4–7 are fixed-normal clearance
  scores; 8–11 are directional confidence scores.

`src/`, `inc/`, `hex/`, `Makefile`, and `gap8_vars.mk` are the direct DORY
output. `artifacts/` carries the source integer ONNX and export reports.

## Build

With the GAP SDK and RISC-V toolchain configured, run from this directory:

```bash
make CORE=8 build image
```

## Validation status

- NEMO integer versus ONNX Runtime: exact parity, 0/3,600 differing elements.
- DORY frontend/lowering: passed; 24 fused layers, 28,152,000 MACs, estimated
  peak L1 tile 36,289 B (below the 64 kB GAP8 limit).
- GVSOC: not passed. The SDK's `gapy --image` invocation aborted after the
  generated application compiled successfully. This package is supplied for
  physical AI-deck-only testing as requested, not as a completed simulator
  parity release.
