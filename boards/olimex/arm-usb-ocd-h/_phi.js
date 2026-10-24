import fs from "fs"

const product = "Olimex OpenOCD JTAG ARM-USB-OCD-H"

export const phi = {
	kind: "probe",
	name: "arm-usb-ocd-h",
	vendor: "Olimex",
	label: "Olimex ARM-USB-OCD-H",
	description: "FT2232H JTAG adapter driven by OpenOCD. Buffered, with nTRST and nSRST. flash.sh uses OpenOCD's interface/ftdi/olimex-arm-usb-ocd-h.cfg.",
	targets: [
		{ name: "flash", label: "Flash", script: "flash.sh", args: "$(MCU) $(TARGET)" },
	],
}

function plugged() {
	const dir = "/sys/bus/usb/devices"
	if (!fs.existsSync(dir))
		return null
	return fs.readdirSync(dir).some((entry) => {
		try {
			return fs.readFileSync(dir + "/" + entry + "/product", "utf8").trim() === product
		}
		catch {
			return false
		}
	})
}

export function prepare(ctx) {
	ctx.requireTool("openocd", "Install OpenOCD 0.12 or later (apt install openocd). Its udev rules give the user access to the FT2232H (15ba:002b).")

	if (!/^stm32f[14]/.test(ctx.mcu.name))
		ctx.error(ctx.mcu.label + " has no OpenOCD target in flash.sh.", "flash.sh knows the STM32F1 and STM32F4 parts.")

	const found = plugged()
	if (found === false)
		ctx.warn("No " + product + " on USB.", "make flash needs the probe plugged in and the board powered.")
	else if (found)
		ctx.info(product + " is plugged in.")
}
