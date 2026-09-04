# oLGMD obstacle-stop GAP8 application

This is the hardware application for the obstacle-only stopping experiment. It
captures the AI-deck Himax camera at 30 Hz, downsamples each 160x160 grayscale
frame to the oLGMD1 80x80 input internally, runs the fixed-point two-polarity
collision detector on the GAP8 cluster, and transmits one 16-byte threat packet
to the STM32.

There is deliberately no gate network, gate-corner packet, CPX image stream,
steering output, or learned-model weight partition in this application. The
matching STM32 firmware must be built from
`codex/olgmd-obstacle-stop-stm32` with its obstacle-stop profile.

## Detector and wire contract

`olgmd1.c` implements the stages in Gao, Liu, Wang and Fu, *A Computationally
Efficient Neuronal Model for Collision Detection with Contrast
Polarity-Specific Feed-Forward Inhibition*, Biomimetics 9(11), 650 (2024),
doi:10.3390/biomimetics9110650. The literature starting point is `Tspi=0.70`
and six accumulated spikes in six frames. These thresholds still require
hardware calibration and are not a safety certification.

The only transmitted packet is:

- header `90 19 08 40`;
- little-endian `{u32 timestamp_ms, u16 frame_sequence, u8 imminent_threat,
  u8 reserved}`;
- repository CRC32;
- total size 16 bytes.

The recurrent detector state occupies 32,018 bytes in L2. Its 12,800-byte
scratch is allocated in L1 only while the cluster job runs. At boot the app
checks that 32 KiB of contiguous L2 remains after camera-buffer allocation.

## Host tests

```sh
make -C tests clean test
```

## Reproducible GAP8 build

On `a2r-lab`, use the installed GAP SDK and DORY Python environment:

```sh
export GAP_RISCV_GCC_TOOLCHAIN=/home/cchen/gap_riscv_toolchain_ubuntu_18
source /home/cchen/gap_sdk_dory/configs/gap8_v2.sh
export PATH=/home/cchen/miniconda3/envs/dory/bin:$PATH

cd /home/cchen/tinympc-nanocockpit/src/gap/examples/olgmd-obstacle
make "$(pwd)/BUILD/GAP8_V2/GCC_RISCV/olgmd_obstacle"
make image
```

Build outputs are the ELF and `target.board.devices.flash.img` under
`BUILD/GAP8_V2/GCC_RISCV/`. Do not use `make all` for a build-only check: the
GAP SDK's `all` target also attempts to program an attached board.

## Lab flash and staged check

With the AI-deck connected to the supported GAP8 programmer and the same SDK
environment loaded:

```sh
make flash
make run
```

Before installing propellers or commanding flight:

1. Confirm the app boots without L1/L2 allocation errors.
2. Capture UART and verify only header `90 19 08 40` appears at 115200 baud.
3. Verify a stationary scene produces clear packets after detector warm-up.
4. Move a textured target toward the camera and verify repeatable threat
   assertion, timestamp progression, and no CRC failures on the STM32.
5. Measure frame time below the 33.3 ms camera period before guarded flight.

The GAP8 image and STM32 image are a matched pair; do not combine this branch
with the gate/oLGMD STM32 mode.
