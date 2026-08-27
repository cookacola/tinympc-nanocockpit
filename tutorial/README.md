# NanoCockpit Frontnet Deployment Tutorial

This tutorial teaches how NanoCockpit deploys the PULP-Frontnet demo across a Crazyflie 2.x with AI-deck:

- STM32 runs the follow-me controller and receives inference results.
- GAP8 captures Himax camera frames, runs Frontnet on board, and streams frame metadata.
- ESP32 NINA bridges CPX packets between GAP8 and the Wi-Fi host.
- The host verifies streams with the Python client or ROS2 viewer.

It does not retrain Frontnet, regenerate DORY/PULP-NN code, replace the safety workflow for real flight, or reproduce the paper's full benchmark results.

## Minimal Runnable Path

From `tinympc-nanocockpit`:

```bash
python -m tutorial.tutorial_impl.scripts.run_minimal --config tutorial/tutorial_impl/configs/minimal.json
```

This command does not flash hardware. It validates the source constants that the deployment depends on and prints the ordered build, GVSOC, flash, and stream-check commands.

Run the tests:

```bash
python -m unittest discover tutorial/tests
```

## Hardware Deployment Overview

After the dry-run validates, deploy in this order:

1. Prepare the Python client environment.
2. Build and flash the STM32 app from `src/stm32/app`.
3. Build and test GAP8 Frontnet from `src/gap/examples/pulp-frontnet`; use GVSOC with `NETWORK_TEST_INPUT` before flashing when possible.
4. Build and flash the NINA CPX bridge from `src/nina`.
5. Verify Wi-Fi streaming with `tools/headless_stream_test.py`, `plt_viewer`, or the ROS2 viewer.
6. Use `cfclient` for radio setup and controlled operation.

## Maps

- [00_big_picture.md](00_big_picture.md)
- [01_paper_to_original_code_map.md](01_paper_to_original_code_map.md)
- [02_original_to_modern_stack_map.md](02_original_to_modern_stack_map.md)
- [03_minimal_modern_implementation.md](03_minimal_modern_implementation.md)
- [04_system_components.md](04_system_components.md)
- [05_evaluation_and_debugging.md](05_evaluation_and_debugging.md)
- [06_reproducing_vs_understanding.md](06_reproducing_vs_understanding.md)
- [modernization_notes.md](modernization_notes.md)
- [deviations_from_original.md](deviations_from_original.md)
- [glossary.md](glossary.md)
