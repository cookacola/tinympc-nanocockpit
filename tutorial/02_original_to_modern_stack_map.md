# Original to Modern Stack Map

| Original component | Original stack/API | Modern tutorial equivalent | Preserved semantics | Changed because | Verification |
|---|---|---|---|---|---|
| GAP8 Frontnet app | GAP SDK v3.8.1, PMSIS, PULP-NN/DORY C code | `tutorial_impl/evaluation/source_audit.py`, `tutorial_impl/sensing_or_observations/contracts.py` | Input count, output count, L2 memory requirement, command path | Firmware build is hardware/toolchain-heavy | `python -m unittest discover tutorial/tests` |
| CPX route constants | C enum in `src/nina/main/cpx_types.h`, Python ctypes in client | `load_cpx_constants()` | CPX version, target IDs, STREAMER function | Only constants are needed for minimal dry-run | `test_cpx_constants_match_streamer_route` |
| Streamer metadata | Packed C structs in `src/gap/lib/streamer.h` and ctypes in client | `StreamerMetadata` dataclass | Frame ID, GAP timestamps, STM32 timestamp, metadata version | Python dataclass is inspectable and CPU-runnable | `test_inference_reply_preserves_state_timestamp` |
| Inference reply | CPX streamer buffer and UART inference message | `make_inference_reply()` | Four outputs and STM32 timestamp association | No packet packing or socket send in tutorial | `test_empty_reply_has_zero_timestamp_for_onboard_inference_ack` |
| Multi-step deployment | README commands, Docker wrappers, Makefiles, OpenOCD | `build_frontnet_deployment_plan()` | STM32 -> GAP8 -> NINA -> host order | Commands become testable data | `test_deployment_plan_keeps_component_order` |
| Real stream verification | AI-deck Wi-Fi, `plt_viewer`, ROS2 viewer, headless tool | Dry-run lists `tools/headless_stream_test.py` | Host, port, stream-check intent | No network dependency in minimal path | Manual hardware verification |
