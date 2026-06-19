#!/usr/bin/env bash
#
# Wrapper to run ESP-IDF v5.3.3 (the version this NINA firmware is pinned to)
# inside the maintainer's espidf docker image.
#
# This host is aarch64 and the image is multi-arch, so it runs NATIVELY under
# --platform linux/arm64 (no qemu emulation, unlike the older 4.3 setup in
# ../../../aideck-esp-firmware).
#
# Usage:
#   ./esp.sh build                 # build the firmware (aideck_cpx_streamer.bin)
#   ./esp.sh all                   # same as build
#   ./esp.sh fullclean             # wipe the build/ dir
#   ./esp.sh menuconfig            # interactive config (WiFi STA/AP, mDNS, ...)
#
# Any arguments are passed straight through to idf.py.
set -euo pipefail

IMAGE="registry.gitlab.com/eliacereda/espidf:22.04-5.3.3"
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Only allocate a tty when we actually have one. `-it` breaks non-interactive
# runs (e.g. `./esp.sh build` from a script/CI). menuconfig still gets a tty.
TTY_ARGS=()
[ -t 0 ] && TTY_ARGS+=(-it)

exec docker run --rm \
  "${TTY_ARGS[@]}" \
  --platform linux/arm64 \
  -v "$PROJECT_DIR":/module/data \
  -w /module/data \
  "$IMAGE" \
  bash -c 'source /esp/esp-idf/export.sh >/dev/null 2>&1 && exec idf.py "$@"' bash "$@"
