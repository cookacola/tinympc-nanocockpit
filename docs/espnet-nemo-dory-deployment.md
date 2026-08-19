# Two-frame ESPNet NEMO/DORY deployment

This is the GAP8 deployment path for the two-frame perception model. It follows
the PuLP-DroNet split: train in PyTorch, quantize with NEMO, generate GAP8 C with
DORY, and build the generated network into NanoCockpit firmware.

## Runtime contract

- Camera: calibrated HM01B0 grayscale, 160 x 160, without synthetic fisheye
  distortion.
- Input: two consecutive frames from the same live camera stream, interleaved
  as HWC `uint8` channels `[previous, current]`. The first inference repeats the
  first frame; subsequent inferences use a one-frame ring buffer.
- Outputs: four 40 x 40 corner heatmaps, one 40 x 40 gate mask, and one 10 x 10
  obstacle-danger map.
- Gate corners: the midpoint between each inner and outer physical corner,
  matching the real-flight labels.

The network-specific quantization constants, validation metrics, tensor shapes,
and provenance are stored beside the generated network in `manifest.json`.

## Reproduce the release

Training and release generation are Slurm jobs. The release job explicitly uses
the existing Python environments: `/home/cchen/isaacsim-env` for PyTorch and
ONNX Runtime, `frontnet` for NEMO, and `dory` for DORY.

```sh
cd /home/cchen/tinympc-gate-texture
student_job=$(sbatch --parsable \
  --dependency=afterok:<safety-constrained-espnet-job> \
  gap8_perception/run_espnet_dory_student.slurm)

cd /home/cchen/gap8-drone-racing-perception
sbatch --dependency=afterok:${student_job} \
  gap8_perception/run_espnet_dory_deployment.slurm
```

The deployment job refuses to package a release unless all four DORY graphs
parse and tile for GAP8, each graph passes its NEMO-to-GVSOC final-layer
checksum, integer ONNX is evaluated against held-out obstacle and gate labels,
and the final NanoCockpit firmware links successfully.

## Build and flash an existing release

Set up the same GAP SDK and RISC-V toolchain used by the release job:

```sh
source /home/cchen/miniconda3/etc/profile.d/conda.sh
conda activate frontnet
export PATH=/home/cchen/gap_riscv_toolchain_ubuntu_18/bin:$PATH
export GAP_RISCV_GCC_TOOLCHAIN=/home/cchen/gap_riscv_toolchain_ubuntu_18
export GAP_RISCV_GCC_TOOLCHAIN_BASE=
set +u
source /home/cchen/gap_sdk_dory/configs/ai_deck.sh
set -u

cd src/gap/examples/tiny-racer
make clean NETWORK_NAME=gap8-espnet-dory-student-v1
make build NETWORK_NAME=gap8-espnet-dory-student-v1
make all NETWORK_NAME=gap8-espnet-dory-student-v1
```

`make build` is the non-destructive compile/link check. `make all` also creates
the flash image and programs the attached AI-deck according to the GAP SDK
configuration.

## Safety note

The generated firmware is a deployment *candidate*, not a flight-approved
model. The first PTQ student preserved collision recall only by producing a
held-out false-positive rate of 0.653 (AP 0.282), well below the full ESPNet
teacher. Its package is retained so the NEMO/DORY/GAP8 integration can be
reproduced and debugged, but it must not be selected for free flight.

A short NEMO QAT follow-up was also rejected: held-out collision AP improved
to 0.659, but recall fell to 0.888, danger-map IoU collapsed to 0, and the gate
mask activated on essentially every negative pixel. It is not packaged.

Generated-C parity is also incomplete. Corner and gate-mask graphs pass GVSOC
checksums, but the danger branch diverges at its stride-2 depthwise layer and
the encoder does not terminate in GVSOC. The workflow therefore refuses to
create a deployment-ready network directory from this student.

The recommended perception checkpoint remains the safety-selected full ESPNet
teacher. Promote a compact firmware package only after its validation-selected
threshold passes the held-out obstacle metrics and its NEMO, GVSOC, and live
camera outputs agree. Then verify camera exposure and crop and run a tethered
test before free flight. A successful build is not safety certification.
