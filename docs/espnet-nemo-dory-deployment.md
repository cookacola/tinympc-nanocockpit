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
make clean NETWORK_NAME=gap8-espnet-dory-student-v2-hybrid-qat
make build NETWORK_NAME=gap8-espnet-dory-student-v2-hybrid-qat
make all NETWORK_NAME=gap8-espnet-dory-student-v2-hybrid-qat
```

`make build` is the non-destructive compile/link check. `make all` also creates
the flash image and programs the attached AI-deck according to the GAP SDK
configuration.

## Safety note

The packaged network is `gap8-espnet-dory-student-v2-hybrid-qat`. It uses the
QAT encoder and danger branch together with the better-calibrated PTQ corner
and gate-mask branches. All 52 generated DORY layers passed their per-layer
NEMO-to-GVSOC checksums, and the final NanoCockpit firmware compiled and
linked successfully.

On the held-out integer test set, the packaged network obtained obstacle
AUROC 0.907, AP 0.460, recall 0.992, and danger-map IoU 0.693. Its collision
false-positive rate remains 0.472 at the validation-selected threshold. Gate
IoU is 0.578; synthetic and real-flight mean corner errors are 7.03 px and
22.64 px, respectively. The complete thresholds, metrics, and quantization
constants are in the package `manifest.json`.

A short NEMO QAT follow-up was also rejected: held-out collision AP improved
to 0.659, but recall fell to 0.888, danger-map IoU collapsed to 0, and the gate
mask activated on essentially every negative pixel. It is not packaged.

The packaged firmware remains a deployment *candidate*, not a flight-approved
model. Its generated-C parity and build gates have passed, but the remaining
false-positive rate and real-flight corner tail error require a live-camera
stream check and tethered flight validation. Verify camera exposure and crop,
then test the validation-selected threshold under controlled lighting before
free flight. A successful build and GVSOC checksum are not safety
certification.
