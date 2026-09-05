# oLGMD obstacle-stop GAP8 application

This is the hardware application for the obstacle-only stopping experiment. It
captures the AI-deck Himax camera at 30 Hz, downsamples each 160x160 grayscale
frame to the oLGMD1 80x80 input internally, runs the fixed-point two-polarity
collision detector on the GAP8 cluster, and transmits a 16-byte threat packet followed by a 20-byte diagnostic packet
to the STM32 for every frame.

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

The existing control/threat packet is unchanged:

- header `90 19 08 40`;
- little-endian `{u32 timestamp_ms, u16 frame_sequence, u8 imminent_threat,
  u8 reserved}`;
- repository CRC32;
- total size 16 bytes.

Each threat transmission completes before the diagnostic packet is sent:

- header `90 19 08 42`;
- little-endian `{u32 timestamp_ms, u16 frame_sequence, u16 membrane_q15,
  u8 spike_count, u8 imminent_threat, u8 valid, u8 reserved}`;
- repository CRC32 over the header and payload (16 bytes), stored little-endian;
- total size 20 bytes; reserved is zero.

`membrane_q15 / 32768.0` is the post-adaptation membrane activity used for
spike generation. `spike_count` is the current frame's count, not the accumulated
collision-window count. `imminent_threat` matches the binary control packet.
The first/priming frame has `valid=0`; subsequent successfully processed frames
have `valid=1`. Both packets share the timestamp and sequence for that frame.
Transmission does not depend on arming, so matching STM32 diagnostic logging
can display these values while disarmed. The diagnostic transfer completes
before the frame is released or its packet buffer reused.

At 30 Hz these packets use 1,080 bytes/s, about 9.4% of a 115200-baud 8N1
link. Diagnostic transmission adds approximately 1.74 ms of serial time per
frame; measure total frame time on hardware.

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
2. Capture UART and verify alternating `90 19 08 40` threat and `90 19 08 42` diagnostic
   packets at 115200 baud.
3. Verify a stationary scene produces clear packets after detector warm-up.
4. Move a textured target toward the camera and verify repeatable threat
   assertion, timestamp progression, and no CRC failures on the STM32.
5. Measure frame time below the 33.3 ms camera period before guarded flight.

The GAP8 image and STM32 image are a matched pair; do not combine this branch
with the gate/oLGMD STM32 mode.
