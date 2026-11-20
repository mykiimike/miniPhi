#!/usr/bin/env bash
# The RP2350 must be in BOOTSEL mode: hold BOOT, then plug USB.
# picotool writes the ELF with the Arm Secure family id, then reboots into it.
# -t selects the file type (elf, uf2, bin). The family id is --family, not -t.
set -euo pipefail

elf=${1:-}

if ! command -v picotool >/dev/null 2>&1; then
	echo "picotool is missing. Install picotool (build from raspberrypi/picotool, or the distro package)." >&2
	exit 1
fi

if [ -z "$elf" ] || [ ! -f "$elf" ]; then
	echo "picotool: firmware ELF not found${elf:+ ($elf)}. Build first; the image is miniphi.elf." >&2
	exit 1
fi

# RP2350 PICOBOOT is USB 2e8a:000f while BOOTSEL is held. No sysfs means
# this is not Linux; picotool itself reports whether a device is present.
if [ -d /sys/bus/usb/devices ]; then
	found=0
	for entry in /sys/bus/usb/devices/*; do
		vendor=""
		product=""
		if [ -r "$entry/idVendor" ]; then
			IFS= read -r vendor < "$entry/idVendor" || vendor=""
		fi
		if [ -r "$entry/idProduct" ]; then
			IFS= read -r product < "$entry/idProduct" || product=""
		fi
		if [ "$vendor" = "2e8a" ] && [ "$product" = "000f" ]; then
			found=1
			break
		fi
	done
	if [ "$found" -eq 0 ]; then
		echo "No RP2350 in BOOTSEL mode was found (USB 2e8a:000f). Hold BOOT and plug USB, then run make flash again." >&2
		exit 1
	fi
elif ! picotool info >/dev/null 2>&1; then
	echo "No RP2350 in BOOTSEL mode was found. Hold BOOT and plug USB, then run make flash again." >&2
	exit 1
fi

picotool load "$elf" -t elf --family 0xe48bff59
# Stay on the Cortex-M33. Bit 0x20 of a reboot flag selects RISC-V, and a
# later reboot without -c arm would keep that core.
picotool reboot -a -c arm
