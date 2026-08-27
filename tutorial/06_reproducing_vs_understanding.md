# Reproducing vs Understanding

## Understanding path

Use the tutorial implementation when you want to understand the deployment contracts:

```bash
python -m tutorial.tutorial_impl.scripts.run_minimal --config tutorial/tutorial_impl/configs/minimal.json
python -m unittest discover tutorial/tests
```

This path teaches which constants and message semantics matter before you build firmware.

## Reproduction path

Use the original firmware paths when you want to reproduce the deployed demo:

1. Set up Python 3.9 environment and client packages.
2. Flash stock Crazyflie 2021.06 NRF51 firmware once with `cfclient`.
3. Build and flash `src/stm32/app`.
4. Build and GVSOC-test `src/gap/examples/pulp-frontnet`.
5. Flash GAP8 with `src/gap/gap8.sh`.
6. Configure, build, and flash NINA with `src/nina/esp.sh` and `src/nina/flash-jtag.sh`.
7. Verify stream and operate through `cfclient`.

## What must match for reproduction

- Hardware: Crazyflie with AI-deck and compatible JTAG/debug adapters.
- Toolchains: GAP SDK v3.8.1 and ESP-IDF v5.3.x as used by the wrappers.
- Firmware: Crazyflie STM32 app based on the repository's customized 2021.06 firmware; NRF51 stock 2021.06.
- Network: NINA Wi-Fi configuration reachable from the host.
- Frontnet artifact: `frontnet-160x32-bgaug` selected in `app/app.mk`.

## What can differ for understanding

- You can run only the dry-run command.
- You can inspect CPX constants without connecting to Wi-Fi.
- You can model inference outputs with toy floats.
- You can avoid ROS2 and use the headless stream tester when hardware is available.

## Remaining TODOs

- TODO: verify exact paper section, figure, and experiment names from the publisher PDF or arXiv source.
- TODO: add an optional parser for `src/gap/lib/streamer.h` packed struct sizes if the stream wire format changes.
- TODO: add a hardware smoke-test fixture that runs only when an AI-deck is reachable.
