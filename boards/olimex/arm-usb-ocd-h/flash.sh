#!/bin/sh
# Writes an ELF into the target through the Olimex ARM-USB-OCD-H, verifies it, and resets the target.
# usage: flash.sh <mcu> <firmware.elf>
set -e

mcu=$1
firmware=$2

if [ -z "$mcu" ] || [ -z "$firmware" ]; then
	echo "usage: $0 <mcu> <firmware.elf>" >&2
	exit 2
fi

case "$mcu" in
	stm32f1*) target=target/stm32f1x.cfg; reset="reset_config srst_only srst_nogate connect_assert_srst" ;;
	stm32f4*) target=target/stm32f4x.cfg; reset="reset_config srst_only srst_nogate connect_assert_srst" ;;
	*) echo "arm-usb-ocd-h: no OpenOCD target for $mcu" >&2; exit 1 ;;
esac

# srst_only: this adapter also drives nTRST, and pulsing it leaves the
# core running, so the halt at the start of program times out.
# connect_assert_srst holds the core in reset while connecting: a
# running image can be in WFI with its debug domain off.
exec openocd \
	-f interface/ftdi/olimex-arm-usb-ocd-h.cfg \
	-c "transport select jtag" \
	-f "$target" \
	-c "$reset" \
	-c "program {$firmware} verify reset exit"
