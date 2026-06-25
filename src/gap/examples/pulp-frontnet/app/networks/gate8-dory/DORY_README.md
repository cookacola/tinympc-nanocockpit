# gate8 — DORY GAP8 network package (8-corner FrontNet)

Complete DORY-generated GAP8 network for the 8-corner gate detector (PULP-FrontNet
160x32 with an 8-output corner-regression head). Generated on the cluster; build/run
**gvsoc on the Mac** (this package targets `PULP.PULP_gvsoc`; no GAP SDK was needed to
*generate* it). The embedded per-layer checksums are bit-exact against the NEMO golden,
so a gvsoc run that passes its checksums == bit-exact match to NEMO.

## Provenance / how to regenerate
- DORY: `pulp-platform/dory` master @ **`add0d9c1be889b5f802b2606ced8c59acff8aa02`**
  (`v1.0-305-gadd0d9c`), submodules `dory_examples` + `pulp-nn` (GAP8 backend) initialized.
- Frontend = **NEMO** (consumes the NEMO 0.0.7 opset-9 integer-deployable ONNX).
- Env: conda `dory` (py3.10) with `onnx 1.22, numpy 2.2.6, ortools 9.15, mako 1.3.12`.
- Config: `config_NEMO_gate8.json` = `{BNRelu_bits: 64, onnx_file: Frontnet.onnx, "code reserved space": 132000}`
  (BNRelu_bits 64 matches the shipped `config_NEMO_NAS_Frontnet.json`; our BN lambda range
  ±7.6M confirms 64-bit is required).
- Generate command (run from the dory repo root `~/dory`):
  ```
  python network_generate.py NEMO PULP.PULP_gvsoc \
      ./gate8_dory/config_NEMO_gate8.json \
      --app_dir <out> --verbose_level Check_all+Perf_final
  ```
  The `gate8_dory/` dir held `Frontnet.onnx` + the NEMO golden (`input.txt`,
  `out_layer0..8.txt`); DORY reads the golden from there to embed the runtime checksums.

## Network facts (for firmware)
- Input: 1×96×160 grayscale uint8, eps_in = 1.0 (integer value == pixel). `inputs.hex`
  is byte-exact to the NEMO `input.txt`.
- **Output count = 8** int32 (`activations_out_size[8] = 32` bytes). Order = IPPE_SQUARE:
  `[TL_x,TL_y,TR_x,TR_y,BR_x,BR_y,BL_x,BL_y]`. Do NOT reorder.
- Dequant (see `output_dequant.txt`): `FLOAT[i] = INT[i]*eps_out + bias[i]`,
  eps_out = 1.61093718e-04, biases = [0.767035, 0.421470, 0.818451, 0.712229,
  0.845009, 0.705781, 0.806453, 0.380487].
- 9 DORY nodes = `BNReluConvolution0, Pooling1, BNReluConvolution2..7, FullyConnected8`
  (1:1 with NEMO `out_layer0..8`).
- Per-layer requant (from `inc/network.h`): `out_mult_vector = {1×9}`,
  `out_shift_vector = {23,0,24,24,24,24,24,24,0}`.
- Per-layer expected output checksums (`activations_out_checksum`): `900814, 396883,
  97145, 86992, 56267, 58907, 18380, 56867, 2069` — layer0 (900814) and the layer8 FC
  outputs match the independently-computed golden anchors.

## Contents
- `src/` — `network.c`, `main.c`, per-layer `BNReluConvolution*/Pooling1/FullyConnected8.c`, `pulp_nn_*` kernels.
- `inc/` — matching headers + `network.h` (all the constant arrays above).
- `hex/` — `BNReluConvolution{0,2..7}_weights.hex`, `FullyConnected8_weights.hex`, `inputs.hex`.
- `Makefile`, `vars.mk` — gvsoc/GAP8 build.
- `config_NEMO_gate8.json`, `output_dequant.txt`, `nemo_golden/` — recipe + dequant + the golden used.
