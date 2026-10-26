#!/bin/sh
# Writes an ELF into the target through the Olimex MSP430-JTAG-TINY-V2.
# usage: flash.sh <mcu> <firmware.elf>
set -e

mcu=$1
firmware=$2

if [ -z "$mcu" ] || [ -z "$firmware" ]; then
	echo "usage: $0 <mcu> <firmware.elf>" >&2
	exit 2
fi

case "$mcu" in
	msp430*) ;;
	*) echo "msp430-jtag-tiny-v2: mspdebug has no target for $mcu" >&2; exit 1 ;;
esac

# -j selects the 14-pin JTAG. Without it mspdebug tries Spy-Bi-Wire and
# stops if that fails. Closing the session resets the core and lets it run.
exec mspdebug -n -j olimex "prog $firmware"
