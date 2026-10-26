#!/bin/sh
# Writes an ELF into the target through the AK-LINK, verifies it, and resets the target.
# usage: flash.sh <mcu> <firmware.elf>
set -e

here=$(dirname "$0")
mcu=$1
firmware=$2

if [ -z "$mcu" ] || [ -z "$firmware" ]; then
	echo "usage: $0 <mcu> <firmware.elf>" >&2
	exit 2
fi

case "$mcu" in
	stm32f1*) target=target/stm32f1x.cfg ;;
	*) echo "ak-link: no OpenOCD target for $mcu" >&2; exit 1 ;;
esac

# connect_assert_srst holds the core in reset while connecting: a running
# image can be in WFI with its debug domain off, and the halt then times out.
exec openocd -f "$here/ak-link.cfg" -f "$target" \
	-c "reset_config srst_only srst_nogate connect_assert_srst" \
	-c "program {$firmware} verify reset exit"
