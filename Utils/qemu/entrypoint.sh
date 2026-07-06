#!/usr/bin/env bash
# Runs inside the container. Expects:
#   /img  -> the PlatformIO build dir (bootloader.bin, partitions.bin, firmware.bin)
#   /pio  -> the host ~/.platformio (read-only, for boot_app0.bin)
# Merges a flash image, boots it in QEMU for 30s, and checks for the boot banner.
set -euo pipefail

cd /img
BOOT_APP0=$(find /pio -name boot_app0.bin | head -n1)
echo "==> merging flash image (boot_app0: $BOOT_APP0)"
esptool.py --chip esp32c3 merge_bin --fill-flash-size 4MB -o flash_image.bin \
    0x0     bootloader.bin \
    0x8000  partitions.bin \
    0xe000  "$BOOT_APP0" \
    0x10000 firmware.bin

echo "==> booting in QEMU (30s cap)"
timeout 30 qemu-system-riscv32 -nographic -no-reboot -machine esp32c3 \
    -drive file=flash_image.bin,if=mtd,format=raw -serial mon:stdio \
    2>&1 | tee qemu.log || true

echo "----- serial log tail -----"; tail -n 20 qemu.log
echo "----- boot marker check -----"
if grep -q "\[boot\] mood-lamp firmware up" qemu.log; then
    echo "BOOT OK"
else
    echo "BOOT MARKER NOT FOUND"
    exit 1
fi
