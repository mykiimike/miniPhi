/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2026  Michael VERGOZ                                      *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify    *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation; either version 3 of the License, or       *
 * (at your option) any later version.                                     *
 *                                                                         *
 * This program is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with this program; if not, write to the Free Software Foundation, *
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA       *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

import fs from "fs"

/* RP2350 ROM in BOOTSEL mode: Raspberry Pi VID, PICOBOOT PID. */
const vendorId = "2e8a"
const productId = "000f"
const device = "RP2350 PICOBOOT (2e8a:000f)"

export const phi = {
	kind: "probe",
	name: "picotool",
	vendor: "Raspberry Pi",
	label: "picotool (USB BOOTSEL)",
	description: "The RP2350 is flashed over USB. Hold BOOTSEL and picotool loads the ELF (or a UF2) with the Arm Secure family id 0xe48bff59, then reboots the chip to run it.",
	targets: [
		{ name: "flash", label: "Flash", script: "flash.sh", args: "$(TARGET)" },
	],
	files: ["flash.sh"],
}

function plugged() {
	try {
		const dir = "/sys/bus/usb/devices"
		if (!fs.existsSync(dir))
			return null
		return fs.readdirSync(dir).some((entry) => {
			try {
				const vendor = fs.readFileSync(dir + "/" + entry + "/idVendor", "utf8").trim().toLowerCase()
				const product = fs.readFileSync(dir + "/" + entry + "/idProduct", "utf8").trim().toLowerCase()
				return vendor === vendorId && product === productId
			}
			catch {
				return false
			}
		})
	}
	catch {
		return null
	}
}

export function prepare(ctx) {
	ctx.requireTool("picotool", "Install picotool (build from raspberrypi/picotool, or distro package). The RP2350 must be in BOOTSEL mode: hold BOOT while plugging USB.")

	const found = plugged()
	if (found === false)
		ctx.warn("No " + device + " on USB.", "make flash needs the RP2350 in BOOTSEL mode: hold BOOT while plugging USB.")
	else if (found)
		ctx.info(device + " is plugged in.")
}
