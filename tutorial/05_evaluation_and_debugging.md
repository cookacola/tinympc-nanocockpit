# Evaluation and Debugging

## Validate constants first

Run:

```bash
python -m tutorial.tutorial_impl.scripts.run_minimal --config tutorial/tutorial_impl/configs/minimal.json
python -m unittest discover tutorial/tests
```

If this fails, inspect:

- `src/gap/examples/pulp-frontnet/app/app.mk`
- `src/gap/examples/pulp-frontnet/app/networks/frontnet-160x32-bgaug/inc/network.h`
- `src/nina/main/cpx_types.h`

## Test Frontnet without hardware

Enable `NETWORK_TEST_INPUT` in `src/gap/examples/pulp-frontnet/config.h`, then run:

```bash
cd src/gap
./gap8.sh examples/pulp-frontnet clean all run platform=gvsoc
```

Expected behavior: the app runs one inference on a hardcoded input and checks layer outputs against the Python-generated reference data.

## Debug missing Wi-Fi stream

Check the NINA configuration first:

- STA mode must match the target SSID and password.
- AP mode creates the AI-deck network directly.
- The default host is `aideck.local`; AP-mode tools often use `192.168.4.1`.
- Port is normally `5000`.

Then run:

```bash
python tools/headless_stream_test.py -host aideck.local -port 5000 -frames 5
```

## Debug no inference on STM32

Trace the chain:

1. GAP8 camera callback starts inference.
2. `network_dequantize_output()` creates four floats.
3. `uart_protocol_send_inference_async()` sends `inference_stamped_msg_t`.
4. STM32 app channel receives the inference and enqueues it for the controller.

The tutorial test `test_inference_reply_preserves_state_timestamp` captures the critical invariant: inference must stay associated with the STM32 timestamp used for the corresponding state.

## Debug latency

The streamer metadata includes frame timestamps, state timestamps, and reply timestamps. The host client computes state delay and round-trip timing from those fields. If latency looks wrong, verify that GAP8 timestamps are monotonic and that host replies use the metadata from the same frame.
