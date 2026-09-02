# Paper to Original Code Map

| Paper concept | Section / equation / figure | Original code | Role in system | Confidence |
|---|---|---|---|---|
| NanoCockpit component split across Crazyflie STM32, AI-deck GAP8, AI-deck ESP32, and host | TODO: verify paper section / figure | `README.md` | Defines deployment architecture | High from repository README |
| Human pose estimation / PULP-Frontnet demo | TODO: verify experiment section | `src/gap/examples/pulp-frontnet`, `src/stm32/app` | Demonstration application for onboard inference and closed-loop control | High from README and file names |
| GAP8 camera acquisition and Frontnet inference | TODO: verify paper system section | `src/gap/examples/pulp-frontnet/main.c::camera_callback`, `::inference_task` | Captures frames, runs Frontnet, dequantizes output | High |
| Frontnet output `[x, y, z, phi]` | TODO: verify paper notation | `src/gap/examples/pulp-frontnet/main.c::inference_task`, `src/client/aideck_cpx_streamer/aideck_cpx_streamer/ros_viewer.py::onboard_inference_to_msg` | Pose-like target estimate sent to host and STM32 | High from code |
| GAP8 to STM32 inference feedback | TODO: verify paper control section | `src/gap/lib/uart_protocol.h`, `src/stm32/app/src/frontnet_appchannel.c`, `src/stm32/app/src/frontnet_main.c` | Carries inference to the flight controller application | High |
| CPX streaming over ESP32 NINA Wi-Fi bridge | TODO: verify paper communication section | `src/nina/main/cpx_types.h`, `src/nina/main/cpx_spi.c`, `src/nina/main/cpx_wifi.c`, `src/client/aideck_cpx_streamer` | Host-visible camera, state, ToF, inference, and latency stream | High |
| GVSOC validation for Frontnet | Repository documentation, paper mention TODO: verify | `src/gap/README.md`, `src/gap/examples/pulp-frontnet/config.h` | Hardware-free correctness check with `NETWORK_TEST_INPUT` | Medium; exact paper tie TODO |
| Literature review of nanorobotics | TODO: verify paper section | `docs/literature_review/scopus_query.txt`, `docs/literature_review/scopus_dataset.csv` | Reproduce review resources, not deployment | High from README |
