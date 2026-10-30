import fs from "fs"

export const phi = {
	kind: "probe",
	name: "esp-usb-jtag",
	vendor: "Espressif",
	label: "USB Serial/JTAG",
	description: "The USB-C port enumerates as 303a:1001. flash.sh writes the image at flash offset 0. Hold BOOT if the running image has cleared the USB reset strap.",
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
			const vendor = fs.readFileSync(dir + "/" + entry + "/idVendor", "utf8").trim()
			const product = fs.readFileSync(dir + "/" + entry + "/idProduct", "utf8").trim()
			return vendor === "303a" && product === "1001"
		}
		catch {
			return false
		}
	})
}

export function prepare(ctx) {
	if (ctx.mcu.name !== "esp32c6")
		ctx.error(ctx.mcu.label + " is not an ESP32-C6.", "flash.sh passes --chip esp32c6 to esptool.")

	const home = process.env.HOME || ""
	const esptool = ["esptool", "esptool.py"].some((name) =>
		["/usr/bin/", "/usr/local/bin/", home + "/.local/bin/"].some((dir) => fs.existsSync(dir + name)))
	if (!esptool)
		ctx.warn("esptool was not found.", "pip install esptool. flash.sh also runs python3 -m esptool when the command is not on PATH.")

	const found = plugged()
	if (found === false)
		ctx.warn("No Espressif USB Serial/JTAG (303a:1001) on USB.", "make flash needs the XIAO USB-C port plugged in. ESP_PORT selects the tty, /dev/ttyACM0 when unset.")
	else if (found)
		ctx.info("Espressif USB Serial/JTAG (303a:1001) is plugged in.")
}
