#!/usr/bin/env bash
#
# Wrapper to run GAP SDK v3.8.1 make targets for the NanoCockpit GAP8 examples
# inside the maintainer's gapsdk docker image, with the ai_deck config sourced.
#
# This host is aarch64 and the image is multi-arch, so it runs NATIVELY under
# --platform linux/arm64 (no qemu emulation).
#
# The whole src/gap tree is mounted so each example's cross-references to
# ../../lib resolve. Pick the example with the first argument; the rest are
# passed to `make`.
#
# Usage:
#   ./gap8.sh <example-subdir> [make args...]
#
# Examples:
#   ./gap8.sh examples/pulp-frontnet clean build       # build only; never touches JTAG
#   ./gap8.sh examples/pulp-frontnet clean all         # build + image + readfs flash over JTAG
#   ./gap8.sh examples/pulp-frontnet build run         # build + run once over JTAG (semihosted stdout)
#   ./gap8.sh examples/cpx build run platform=gvsoc    # build + run in the GVSOC simulator (no hardware)
#   ./gap8.sh examples/streamer clean                  # clean
#
# JTAG adapter: defaults to the Olimex ARM-USB-TINY-H (the adapter connected to
# this machine, lsusb 15ba:002a). Override with GAPY_OPENOCD_CABLE if needed.
set -euo pipefail

IMAGE="registry.gitlab.com/eliacereda/gapsdk:22.04-3.8.1"
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GAP_CONFIG="${GAP_CONFIG:-ai_deck}"
CABLE="${GAPY_OPENOCD_CABLE:-interface/ftdi/olimex-arm-usb-tiny-h.cfg}"
REPO_DIR="$(cd "$PROJECT_DIR/../.." && pwd)"

if [ $# -lt 1 ]; then
  echo "Usage: ./gap8.sh <example-subdir> [make args...]" >&2
  echo "  e.g. ./gap8.sh examples/pulp-frontnet clean build" >&2
  echo "       ./gap8.sh examples/pulp-frontnet clean all  # includes JTAG flash" >&2
  echo "       ./gap8.sh examples/cpx build run platform=gvsoc" >&2
  exit 1
fi
SUBDIR="$1"; shift

if [ ! -d "$PROJECT_DIR/$SUBDIR" ]; then
  echo "No such example dir: $SUBDIR (under $PROJECT_DIR)" >&2
  exit 1
fi

# OpenOCD reaches the FT2232 (JTAG) over libusb, so pass the USB bus through
# read-write (--privileged + /dev/bus/usb). The README's `gap8` alias mounts
# /dev/bus read-only, which is fine for GVSOC/build but not for JTAG flashing.
TTY_ARGS=()
[ -t 0 ] && TTY_ARGS+=(-it)

exec docker run --rm \
  "${TTY_ARGS[@]}" \
  --platform linux/arm64 \
  --privileged -v /dev/bus/usb:/dev/bus/usb \
  -e "GAPY_OPENOCD_CABLE=$CABLE" \
  -v "$PROJECT_DIR":/module/data \
  -w "/module/data/$SUBDIR" \
  "$IMAGE" \
  bash -c "source /gap_sdk/configs/${GAP_CONFIG}.sh >/dev/null 2>&1 && exec make \"\$@\"" bash "$@"
