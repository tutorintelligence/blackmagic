#!/usr/bin/env bash
# Flash the tutorduino BMP firmware to an STM32F103C8T6 via an ST-Link v2.
#
# The STM32F103C8T6 (and many GD32F103 clones used on these boards) reports
# only 64 KiB of flash in its device ID register, but the chip physically
# has 128 KiB.  OpenOCD will refuse to touch anything above 0x08010000
# unless FLASH_SIZE is set explicitly -- hence the override below.
#
# Usage:
#   scripts/flash-tutorduino.sh [BUILD_DIR]
#
#   BUILD_DIR  Path to a configured tutorduino build directory.
#              Defaults to "build-tutorduino" (relative to the repo root).
#
# Requirements:
#   openocd, arm-none-eabi-gcc toolchain, meson, ninja

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${1:-"$REPO_ROOT/build-tutorduino"}"

BOOT="$BUILD/blackmagic_tutorduino_bootloader.bin"
FW="$BUILD/blackmagic_tutorduino_firmware.bin"

# ── build if binaries are missing ────────────────────────────────────────────
if [[ ! -f "$BOOT" || ! -f "$FW" ]]; then
    echo "Binaries not found in $BUILD — building now..."
    meson setup --cross-file "$REPO_ROOT/cross-file/tutorduino.ini" "$BUILD"
    meson compile -C "$BUILD"
    meson compile -C "$BUILD" boot-bin
fi

echo "Bootloader : $BOOT  ($(wc -c < "$BOOT") bytes)"
echo "Firmware   : $FW  ($(wc -c < "$FW") bytes)"
echo

# ── flash both images in one OpenOCD session ─────────────────────────────────
# The STM32F103C8 reports 64 KiB in its device ID but has 128 KiB of usable
# flash.  Pass FLASH_SIZE before sourcing stm32f1x.cfg so OpenOCD uses the
# real size instead of the auto-detected one.
openocd \
    -f interface/stlink.cfg \
    -c "set FLASH_SIZE 0x20000" \
    -f target/stm32f1x.cfg \
    -c "program $BOOT 0x08000000 verify" \
    -c "program $FW   0x08002000 verify reset exit"

echo
echo "Done. The tutorduino should now enumerate as a Black Magic Debug Probe."
echo "GDB port  : /dev/ttyACM0  (or the first ACM device)"
echo "UART port : /dev/ttyACM1"
