# NanoCockpit
Performance-optimized Application Framework for AI-based Autonomous Nanorobotics

## Features
* Camera configuration and acquisition
* State-estimation forwarding to GAP8
* Camera and state-estimation streaming over Wi-Fi

## Structure
This repository is structured with one sub-directory for each component of the system, matched with the hardware components in a Crazyflie drone:
* `stm32`: code for the Crazyflie's main STM32 MCU, further divided in `crazyflie-firmware` (original Crazyflie firmware, with minor additions) and `app` (user application code).
* `nina`: code for the ESP32 NINA module on the AI-deck. Handles Wi-Fi communication, streams data received from GAP8 to the host computer.
* `gap`: code for the GAP8 SoC on the AI-deck. Handles camera configuration and acquisition, receives state estimation from STM32 over UART, streams over SPI to ESP32.
* `client`: Python code for the host computer. Provides ROS and GUI clients for the camera streamer, off-board inference and remote control of the drone.

## Getting started

The following instructions allow you build and deploy a demo application build on top of the NanoCockpit framework to a Crazyflie drone with AI-deck.
The application implements the PULP-Frontnet-based CNN tested in the _Human pose estimation_ experiment, including on-board inference on the GAP8 SoC and closed-loop control on the STM32 MCU.
More detailed information about each component is available in the respective subfolders.

### System requirements

NanoCockpit has been tested with the following system configuration
- Ubuntu 22.04
- Python 3.9

#### Setup virtual environment

```shell
$ python3.9 -m venv venv
$ source venv/bin/activate
$ pip install \
  -r requirements.txt \
  -e src/client/aideck_cpx_streamer \
  -e src/client/crazyflie-clients-python
```
#### Prepare the Crazyflie

First, flash the 2021.06 firmware from cfclient. This ensures that the NRF51 MCU is flashed to a firmware version compatible with the STM32 firmware below. You only need to do this the first time you flash the demo on a Crazyflie.

#### STM32

```shell
$ source venv/bin/activate
$ cd src/stm32/app
$ make all -j8
$ make cload
```

#### GAP8

GAP SDK v3.8.1 is required to build this application. Configure your computer to use the GAP SDK Docker container as described [here](src/docker/gapsdk).
Then activate the GAP SDK container:

```shell
$ source venv/bin/activate
$ cd src/gap
$ gap8 3.8.1
```

NanoCockpit contains a number of GAP application examples. Select the PULP-Frontnet example, then compile it and flash it to the AI-deck:
```
> cd examples/pulp-frontnet
> make clean
> make all flash
```

#### NINA ESP32

ESP-IDF v5.3.1 is required to build this application. Configure your computer to use the ESP-IDF Docker container as described [here](src/docker/espidf).
Then activate the ESP-IDF container:

```shell
$ source venv/bin/activate
$ cd src/nina
$ esp 5.3.1
``` 

```shell
> idf.py clean; idf.py menuconfig; idf.py all
> openocd -f interface/ftdi/olimex-arm-usb-ocd-h.cfg -f board/esp-wroom-32.cfg -c "adapter_khz 20000"  -c 'program_esp build/partition_table/partition-table.bin 0x8000 verify' -c 'program_esp build/bootloader/bootloader.bin 0x1000 verify' -c 'program_esp build/aideck_cpx_streamer.bin 0x10000 verify reset exit'
```

#### Crazyflie Client

```shell
$ source venv/bin/activate
$ cfclient
```

### View the deployed GAP8 perception network

For laptop-only inference, build and flash the dedicated GAP8 streamer. This
target does not link or execute a neural network and does not allocate the
180 kB DORY workspace. It only captures the camera and sends the 160x120 center
crop expected by `shared_dory_frozen_real_v1`.

From `src/gap`, use the repository's GAP SDK wrapper:

```shell
> ./gap8.sh examples/pulp-frontnet clean build STDC_STREAM_ONLY=1
> ./gap8.sh examples/pulp-frontnet all STDC_STREAM_ONLY=1
```

The first command is a build-only check. The second builds the image and
flashes it over JTAG. It is also safe to use just one combined command:

```shell
> ./gap8.sh examples/pulp-frontnet clean all STDC_STREAM_ONLY=1
```

The streaming-only image uses 55,964 bytes of L2 and 28 bytes of L1 in the
linked GAP8 build. The AI-deck must also run this repository's NINA CPX bridge;
the Bitcraze JPEG `wifi-img-streamer` protocol is not compatible with this
client.

With the AI-deck connected in AP mode, the viewer receives the crop and runs
the exact three-component integer ONNX release from
`gap8_stdc_release_shared_real_v1` on the laptop. It renders the ordered
gate-corner and obstacle-danger outputs, including the same validated
three-corner recovery used by the deployed decoder.

```shell
$ source venv/bin/activate
$ python tools/neural_net_viewtester.py \
    --nn-weights gap8_stdc_release_shared_real_v1 \
    --view both
```

Pass `-n <AI-deck-IP>` for a different address. The window closes with `q`.
Use `--save` to save both source frames and
`tools/stream_out/results.csv`. The CSV contains frame timestamps, inference
summaries, and the complete raw-quantized and probability 8x10 danger maps.
Use `--results-out <path.csv>` to record results without saving images, or
`--save --no-display --frames 100` for a bounded headless capture.
The `--send-nn-output` switch is intentionally not used with the STDC viewer:
the legacy streamer reply transports only four floats, whereas this network
produces corner heatmaps and an obstacle map for the on-board controller.

## Literature review
In our paper, we review the body of work on nanorobotics over the last five years and demonstrate both the high research interest in the topic and the Crazyflie's prominent status as de-facto standard robot platform.
The data to reproduce our analysis is available in `docs/literature_review` as a resource to other researchers that approach the nano-drone field.
We search the Elsevier Scopus citation database using the query in [scopus_query.txt](docs/literature_review/scopus_query.txt), resulting in the 554 papers reported in [scopus_dataset.csv](docs/literature_review/scopus_dataset.csv), with our manual annotations with information about the target drone platform.

## Publications
If you use NanoCockpit in an academic context, we kindly ask you to cite the following publication:
* E. Cereda, A. Giusti, and D. Palossi, ‘NanoCockpit: Performance-optimized Application Framework for AI-based Autonomous Nanorobotics’, in IEEE Robotics and Automation Practice, 2026 [IEEE Xplore](https://ieeexplore.ieee.org/document/11494460), [arXiv](https://arxiv.org/abs/2601.07476).
  
```bibtex
@article{cereda2025nanocockpit,
  author={Cereda, Elia and Giusti, Alessandro and Palossi, Daniele},
  title={NanoCockpit: Performance-optimized Application Framework for AI-based Autonomous Nanorobotics},
  journal={IEEE Robotics and Automation Practice},
  year={2026},
  volume={},
  number={},
  pages={1-6},
  doi={10.1109/RAP.2026.3687493}
}
```

## Contributors
Elia Cereda<sup>1</sup>,
Alessandro Giusti<sup>1</sup>,
and Daniele Palossi<sup>1,2</sup>.

<sup>1 </sup>Dalle Molle Institute for Artificial Intelligence (IDSIA), USI and SUPSI, Switzerland.<br>
<sup>2 </sup>Integrated Systems Laboratory (IIS) of ETH Zürich, Switzerland.<br>
