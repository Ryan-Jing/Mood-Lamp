#!/usr/bin/env bash
# Local QEMU boot smoke test — mirrors the CI qemu-smoke job.
#   1. builds the firmware on the host (fast; uses your existing PlatformIO)
#   2. builds the QEMU Docker image (first run only)
#   3. merges + boots the image in Espressif QEMU inside the container
#
# Usage:  ./Utils/qemu/run.sh
# Needs:  Docker Desktop running, and `pio` on PATH.
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel)"
BUILD="$ROOT/Firmware/.pio/build/seeed_xiao_esp32c3"

echo "==> building firmware"
( cd "$ROOT/Firmware" && pio run -e seeed_xiao_esp32c3 )

echo "==> building QEMU image (cached after first run)"
docker build --platform linux/amd64 -t moodlamp-qemu "$ROOT/Utils/qemu"

echo "==> booting in QEMU"
docker run --rm --platform linux/amd64 \
    -v "$BUILD:/img" \
    -v "$HOME/.platformio:/pio:ro" \
    moodlamp-qemu
