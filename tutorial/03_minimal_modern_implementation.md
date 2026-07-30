# Minimal Modern Implementation

## What is implemented

The minimal implementation is a hardware-free validator and deployment dry-run. It reads the original source tree, checks the constants that must match for Frontnet deployment, constructs a toy streamer metadata object, creates a toy inference reply, and prints the command plan.

## Where it appears in the paper

The implementation corresponds to the NanoCockpit system deployment and Frontnet demo. Exact paper section, equation, and figure references are `TODO: verify`.

## Where it appears in the original code

- `src/gap/examples/pulp-frontnet/app/networks/frontnet-160x32-bgaug/inc/network.h`
- `src/gap/examples/pulp-frontnet/main.c`
- `src/gap/lib/streamer.h`
- `src/gap/lib/uart_protocol.h`
- `src/nina/main/cpx_types.h`
- `src/client/aideck_cpx_streamer/aideck_cpx_streamer/cpx/streamer.py`

## Where it appears in the tutorial

- `tutorial_impl/scripts/run_minimal.py`
- `tutorial_impl/evaluation/source_audit.py`
- `tutorial_impl/sensing_or_observations/contracts.py`
- `tutorial_impl/actions_or_control/reply.py`
- `tutorial_impl/execution/deployment_plan.py`

## How to run

```bash
python -m tutorial.tutorial_impl.scripts.run_minimal --config tutorial/tutorial_impl/configs/minimal.json
```

Use `--include-hardware` only to print hardware-dependent steps as `RUN`; the tutorial still does not execute them.

## How to verify

```bash
python -m unittest discover tutorial/tests
```

The tests intentionally check behavioral contracts: dimensions, CPX IDs, timestamp preservation, empty acknowledgements, and deployment order.
