export const phi = {
	kind: "app",
	name: "sysinfo",
	label: "System info",
	description: "Prints the miniPhi banner, the version, the build and the CPU on a UART console, then reports uptime, tasks and sensors. Blinks SOS on the first LED and HELP on the second when the board has them. On an ILI932x screen the TUI home lists Accelerometer, then System. System is a scrollable page with about, version, copyright, license, the flash (size, image, blank pages and the controller), uptime and the MCU sensors, and opens a Task manager that lists the kernel tasks. Accelerometer shows the 3-axis sensor that is present, with a bubble level.",
	sources: ["main.c", "screen.c", "morse.c"],
	requires: ["SERIAL", "SENSOR"],
	devices: [
		{ bus: "uart", name: "console", label: "Console" },
	],
	settings: [
		{ name: "SYSINFO_LINE_DELAY", label: "Delay between boot lines", type: "number", unit: "ms", default: 60, min: 10, max: 1000 },
		{ name: "SYSINFO_PERIOD", label: "Report period", type: "number", unit: "ms", default: 5000, min: 100, max: 60000 },
		{ name: "SYSINFO_MORSE_UNIT", label: "Morse unit on the LEDs", type: "number", unit: "ms", default: 200, min: 50, max: 2000 },
	],
}
