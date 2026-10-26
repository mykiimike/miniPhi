import fs from "fs"

const product = "MSP430 JTAGTiny ver.2"

export const phi = {
	kind: "probe",
	name: "msp430-jtag-tiny-v2",
	vendor: "Olimex",
	label: "Olimex MSP430-JTAG-TINY-V2",
	description: "USB CDC probe (15ba:0031) driven by mspdebug's olimex driver. flash.sh programs an ELF over the 14-pin JTAG; closing the session lets the core run. 60-msp430-jtag-tiny-v2.rules gives plugdev write access so libusb can detach cdc-acm.",
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
	ctx.requireTool("mspdebug", "Install mspdebug (apt install mspdebug). The Tiny V2 is USB 15ba:0031, and 60-msp430-jtag-tiny-v2.rules must let plugdev write it.")

	if (!/^msp430/.test(ctx.mcu.name))
		ctx.error(ctx.mcu.label + " has no mspdebug target in flash.sh.", "flash.sh only knows the MSP430 parts.")

	const found = plugged()
	if (found === false)
		ctx.warn("No " + product + " on USB.", "make flash needs the probe plugged in and the board powered.")
	else if (found)
		ctx.info(product + " is plugged in.")
}
