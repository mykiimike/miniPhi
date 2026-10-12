import fs from "fs"

const product = "AK-LINK JTAG Adapter"

export const phi = {
	kind: "probe",
	name: "ak-link",
	vendor: "Artekit",
	label: "Artekit AK-LINK",
	description: "FT2232D JTAG adapter driven by OpenOCD. JTAG only, no SWD.",
	files: ["ak-link.cfg"],
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
	ctx.requireTool("openocd", "Install OpenOCD 0.12 or later (apt install openocd). Its udev rules give the user access to the FT2232.")

	if (!/^stm32f1/.test(ctx.mcu.name))
		ctx.error(ctx.mcu.label + " has no OpenOCD target in flash.sh.", "flash.sh only knows the STM32F1 parts.")

	const found = plugged()
	if (found === false)
		ctx.warn("No " + product + " on USB.", "make flash needs the probe plugged in and the board powered.")
	else if (found)
		ctx.info(product + " is plugged in.")
}
