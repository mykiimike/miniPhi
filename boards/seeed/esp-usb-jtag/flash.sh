#!/bin/sh
# Write an ESP32-C6 image through the USB Serial/JTAG port.
# The ROM loads the image at flash offset 0 into HP SRAM.
# If connect fails, hold BOOT, tap RESET, release BOOT, and run again.
set -eu

mcu=$1
elf=$2
port=${ESP_PORT:-/dev/ttyACM0}
bin=${elf%.elf}.bin

if command -v esptool >/dev/null 2>&1; then
	tool="esptool"
elif command -v esptool.py >/dev/null 2>&1; then
	tool="esptool.py"
elif python3 -m esptool version >/dev/null 2>&1; then
	tool="python3 -m esptool"
else
	echo "esptool is not installed" >&2
	exit 1
fi

# shellcheck disable=SC2086
$tool --chip "$mcu" elf2image --flash-mode dio --flash-freq 80m --flash-size 4MB -o "$bin" "$elf"
# shellcheck disable=SC2086
$tool --chip "$mcu" -p "$port" -b 460800 --before default-reset --after hard-reset write-flash 0x0 "$bin"
