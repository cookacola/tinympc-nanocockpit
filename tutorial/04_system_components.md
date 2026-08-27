# System Components

## STM32 controller

The STM32 application is in `src/stm32/app`. It receives inference messages and uses them in the Frontnet follow-me controller. The tutorial does not reimplement the controller; it preserves the fact that inference must carry the STM32 state timestamp.

Verify:

```bash
cd src/stm32/app
make clean all
make cload CLOAD_ARGS="-w radio://0/100/2M/E7E7E7E7E7"
```

## GAP8 Frontnet application

The GAP8 application is in `src/gap/examples/pulp-frontnet`. It configures the Himax camera, launches asynchronous inference, streams frames and metadata, and sends inference back over UART.

Important original files:

- `main.c`
- `config.h`
- `app/app.mk`
- `app/networks/frontnet-160x32-bgaug/inc/network.h`

Verify with GVSOC after enabling `NETWORK_TEST_INPUT` in `config.h`:

```bash
cd src/gap
./gap8.sh examples/pulp-frontnet clean all run platform=gvsoc
```

Flash:

```bash
cd src/gap
./gap8.sh examples/pulp-frontnet clean all flash
```

## NINA ESP32 CPX bridge

The NINA bridge is in `src/nina`. It forwards CPX between GAP8 SPI and host Wi-Fi. `menuconfig` sets STA/AP mode, SSID, password, and mDNS hostname.

Verify:

```bash
cd src/nina
./esp.sh build
./flash-jtag.sh
```

## Host clients

The host tools are in `src/client` and `tools`.

Verify stream:

```bash
python tools/headless_stream_test.py -host aideck.local -port 5000 -frames 5
plt_viewer -host aideck.local
ros2 launch aideck_cpx_streamer ros_viewer_launch.xml host:=aideck.local
```

## Tutorial implementation

The tutorial models the contracts in Python and does not replace the original deployment code. Use it before touching hardware to confirm that your local source tree still matches the documented Frontnet deployment assumptions.
