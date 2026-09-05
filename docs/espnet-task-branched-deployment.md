# ESPNet v8 obstacle and gate deployment

This installs the full quantized two-task ESPNet v8 release in NanoCockpit on
a2r-lab. Source: the immutable
`drone_rl/outputs/espnetv2_imav22_gate_rail/promoted/espnetv2_gap8_task_branched_v8_full_int8`
bundle, rather than the newer collision-only `current_gap8_deployable` pointer.

The self-contained model copy is
`releases/espnetv2_gap8_task_branched_v8_full_int8/`.
`COPY_INTEGRITY.json` records SHA-256 verification of its 384 original files.
Firmware: `src/gap/examples/espnet-task-branched/`.
Generated network: `app/networks/espnetv2-v8-full-int8/` within that target.
Its manifest records provenance, memory, tensor shapes, affine constants,
and generated-file hashes.

## Build and flash after pulling on the laptop

Use the repository's GAP SDK Docker wrapper (Docker must be running).
From the repository root:

```sh
cd src/gap
./gap8.sh examples/espnet-task-branched clean build image
./gap8.sh examples/espnet-task-branched flash
```

The first command builds and packages the model weights without flashing.
The second programs the attached AI-deck through the configured JTAG adapter.
The native `tools/build_espnet_task_branched.sh` wrapper below is specific to
a2r-lab; use `src/gap/gap8.sh` on the laptop.

## Build on a2r-lab

From `/home/cchen/tinympc-nanocockpit`:

```sh
tools/build_espnet_task_branched.sh clean
tools/build_espnet_task_branched.sh build image
```

The wrapper activates the existing frontnet environment, RISC-V toolchain,
and GAP SDK AI-deck configuration. `build image` compiles the firmware and
creates its ReadFS/flash image without programming hardware. No arguments
defaults to `build`. The ELF is
`src/gap/examples/espnet-task-branched/BUILD/GAP8_V2/GCC_RISCV/espnet_task_branched`.

To program an attached AI-deck after building the normal camera target:

```sh
tools/build_espnet_task_branched.sh flash
```

The repository's NINA CPX bridge is required for camera streaming. Start its
existing viewer from an environment with the repository client requirements:

```sh
python tools/tiny_racer_stream_viewer.py -n 192.168.4.1
```

The camera image is shaded by left/center/right collision probability and
annotated with visible rail endpoints. CPX metadata uses the existing viewer's
TL, TR, BR, BL corner order; invalid endpoints are encoded as -1. The viewer
only draws its host-side polygon when all four endpoints are available.
GAP8 stdout reports collision, affordance, and rail visibility probabilities
in thousandths (`ESPNET ..._milli=`).

## Model and runtime contract

- Input: HWC uint8 `[160,160,3]`, channels previous grayscale, current
  grayscale, `(current - previous + 255) // 2`. First capture repeats the
  first frame; subsequent inputs use the previous processed camera frame.
- Execution: collision encoder → collision head, then gate encoder →
  affordance/visibility head → corner head. The original input is retained
  in HyperRAM for the second encoder; both gate heads use the same
  preserved gate feature tensor.
- Packed output: 1600 bytes of HWC `[20,20,4]` corner maps in model order
  LT, RT, LB, RB; five gate bytes (none, opening-left, opening-right,
  visibility-left, visibility-right); three collision bytes (left, center,
  right).
- Decoding: NeMO scale, output offset, learned bias, and teacher offset
  are applied exactly once. Collision and visibility use sigmoid;
  affordance uses softmax. The bring-up overlay uses corner argmax scaled by
  `159/19`, gated by rail visibility ≥ 0.5. It does not recover missing corners.
- One inference at a time; 260000-byte L2 workspace, 52808-byte wrapper
  buffers, preserved 76800-byte input in HyperRAM. The camera uses one buffer
  to leave space for the five-graph runtime.

This is a perception bring-up target. Like the existing Tiny Racer bring-up
target, it streams perception and does not send control commands to STM32.
Collision outputs are sector probabilities, not bounding boxes or a dense
obstacle map. Connecting these outputs to a flight controller requires a
separately defined controller interface.

## Verification completed

The normal board firmware and flash image build successfully. The integrated
GVSOC output CRC32 matches all 1608 reference ONNX output bytes. The C decoder
passes the independent host comparison, and the GVSOC startup allocation
check passes. Artifact hashes and results are recorded in
`releases/espnet-task-branched-validation/deployment_report.json`.

## Reproduce verification

```sh
/home/cchen/isaacsim-env/bin/python tools/verify_espnet_task_branched.py --verify-decoder
tools/build_espnet_task_branched.sh clean
tools/build_espnet_task_branched.sh build image run platform=gvsoc PARITY_TEST=1 \
  PARITY_INPUT=/home/cchen/tinympc-nanocockpit/releases/espnet-task-branched-validation/input_hwc.raw
```

The fixture output is 1608 bytes with CRC32 `ccc00b83`; input CRC32 is
`95d5283f`. The host verifier runs all five integer ONNX partitions and
checks the C decoder against independent NumPy decoding from source metadata.
Logs and reports are in `releases/espnet-task-branched-validation/`.

A separate `STARTUP_TEST=1` GVSOC build initializes the camera, streamer,
all five networks, and inference workspace, then exits. Always clean and
rebuild with neither test flag before programming a device.

## Qualification carried with the model

The source gate branch passed six integer/float parity gates. Collision
parity failed its strict quality thresholds and the original promotion
records a user-authorized quality override. Packaging preserves that
qualification; it does not change the model's accuracy status.
No physical AI-deck execution or flight validation was performed as part
of this installation.
