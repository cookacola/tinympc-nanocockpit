#!/usr/bin/env bash
#
# Flash the AI-deck ESP32 (NINA) over JTAG, running OpenOCD inside the
# ESP-IDF v5.3.3 docker image.
#
# Adapter note:
#   The NanoCockpit README documents the Olimex ARM-USB-OCD-H. The adapter
#   actually connected to this machine is an Olimex ARM-USB-TINY-H
#   (lsusb 15ba:002a), so this script uses olimex-arm-usb-tiny-h.cfg. If you
#   swap to an OCD-H, change the interface cfg below to olimex-arm-usb-ocd-h.cfg.
#
# Why JTAG and not `idf.py flash`?
#   The AI-deck has no UART auto-reset path to the ESP32, so esptool's serial
#   download mode times out. OpenOCD drives the FT2232-based Olimex over libusb.
#
# Requirements:
#   - Olimex ARM-USB-TINY-H connected to the deck's ESP debug port (powered).
#   - ./esp.sh build  already run so build/ contains the binaries.
#
# Usage:
#   ./flash-jtag.sh            # flash partition table + bootloader + app, then reset
#
set -euo pipefail

IMAGE="registry.gitlab.com/eliacereda/espidf:22.04-5.3.3"
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

for f in build/partition_table/partition-table.bin \
         build/bootloader/bootloader.bin \
         build/aideck_cpx_streamer.bin; do
  if [ ! -f "$PROJECT_DIR/$f" ]; then
    echo "Missing $f -- run ./esp.sh build first." >&2
    exit 1
  fi
done

# OpenOCD reaches the FT2232 (JTAG) over libusb, so pass the USB bus through
# read-write (--privileged + /dev/bus/usb). The README's `esp` alias mounts
# /dev/bus read-only, which is fine for build but not for OpenOCD writes.
exec docker run --rm -it \
  --platform linux/arm64 \
  --privileged -v /dev/bus/usb:/dev/bus/usb \
  -v "$PROJECT_DIR":/module/data -w /module/data \
  "$IMAGE" \
  bash -c 'source /esp/esp-idf/export.sh >/dev/null 2>&1 && exec \
    openocd -f interface/ftdi/olimex-arm-usb-tiny-h.cfg -f board/esp-wroom-32.cfg \
      -c "adapter_khz 20000" \
      -c "program_esp build/partition_table/partition-table.bin 0x8000 verify" \
      -c "program_esp build/bootloader/bootloader.bin 0x1000 verify" \
      -c "program_esp build/aideck_cpx_streamer.bin 0x10000 verify reset exit"'
