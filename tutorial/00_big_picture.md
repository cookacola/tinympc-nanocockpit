# Big Picture

## Problem

NanoCockpit is an application framework for AI-based autonomous nanorobotics on a Crazyflie with AI-deck. The Frontnet demo deploys a quantized vision network to estimate a target pose from the AI-deck camera and uses that inference in a low-level STM32 controller.

## Inputs and outputs

Input:

- Himax grayscale camera frame on GAP8.
- Crazyflie state and time-of-flight data forwarded from STM32 to GAP8.
- Optional offboard inference reply from the host.

Output:

- Frontnet inference vector `[x, y, z, phi]`.
- UART inference message from GAP8 to STM32.
- CPX streamer packets carrying frame, state, ToF, inference, and timing metadata to the host.

## Core idea

The deployment is a three-firmware system. STM32 controls the Crazyflie, GAP8 handles camera and neural-network work, and NINA ESP32 bridges the CPX stream over Wi-Fi. Frontnet can be used on board on GAP8, while the host client can still acknowledge frames and support offboard experiments.

## System components

| Component | Hardware | Original location | Role |
|---|---|---|---|
| STM32 app | Crazyflie STM32 | `src/stm32/app` | Controller, state forwarding, inference receiver |
| GAP8 app | AI-deck GAP8 | `src/gap/examples/pulp-frontnet` | Camera acquisition, Frontnet inference, streamer source |
| NINA bridge | AI-deck ESP32 | `src/nina` | CPX SPI to Wi-Fi bridge |
| Host clients | Laptop | `src/client`, `tools` | Stream verification, ROS2 bridge, dataset saving |

## What this tutorial preserves

- Frontnet input count `15360`, output count `4`, and L2 buffer size `352000`.
- CPX version `0`, STREAMER function `0x06`, GAP target `0x04`, and host target `0x03`.
- The inference vector semantics `[x, y, z, phi]`.
- The STM32 timestamp association used when sending inference results to the controller.
- The deployment order across STM32, GAP8, NINA, and host verification.

## What this tutorial simplifies

- It does not run the neural network kernels.
- It does not flash hardware by default.
- It models CPX and streamer data as Python dataclasses instead of packed C structs.
- It validates source constants and command order instead of rebuilding every firmware image.

## First command to run

```bash
python -m tutorial.tutorial_impl.scripts.run_minimal --config tutorial/tutorial_impl/configs/minimal.json
```
