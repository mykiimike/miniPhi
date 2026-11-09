export const phi = {
	kind: "module",
	name: "SERIAL",
	label: "Serial",
	description: "Buffered serial line over a UART. Printk can be wired to it. A terminal takes the line and then printk cannot, until the line is given back. Each byte received holds the LOW power level for a while, so the line stays out of deep sleep while someone types, and wakes the reader task.",
	define: "SUPPORT_COMMON_SERIAL",
	sources: ["serial.c"],
	headers: ["serial.h"],
	requires: ["CIRCULAR"],
	tests: ["test/printk.c"],
	settings: [
		{ name: "MP_SERIAL_AWAKE", label: "Awake after a received byte", type: "number", unit: "ms", default: 30000, min: 0, max: 600000 },
		{ name: "MP_SERIAL_PRINTK_MAX", label: "Printk line", type: "number", unit: "bytes", default: 128, min: 32, max: 256 },
	],
}
