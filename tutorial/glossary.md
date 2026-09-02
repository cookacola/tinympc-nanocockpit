# Glossary

AI-deck: Bitcraze expansion deck with GAP8, ESP32 NINA, and Himax camera.

CPX: Crazyflie Packet eXchange protocol used to route packets among STM32, ESP32, GAP8, and the host.

Frontnet: Vision network used by the NanoCockpit demo to produce a four-value target estimate `[x, y, z, phi]`.

GAP8: GreenWaves/PULP system-on-chip on the AI-deck. NanoCockpit uses it for camera acquisition and onboard neural-network inference.

GVSOC: GAP SDK simulator for GAP chips. In this project it can test Frontnet with `NETWORK_TEST_INPUT`.

Himax: Camera sensor on the AI-deck.

NINA: ESP32 module on the AI-deck. NanoCockpit uses it as the CPX Wi-Fi bridge.

PULP-NN / DORY: Tooling and kernels used for optimized quantized neural-network deployment on GAP8.

STM32: Main Crazyflie microcontroller that runs the flight-side application and receives Frontnet inference.

Streamer: NanoCockpit's frame and metadata transport path from GAP8 through NINA to the host.
