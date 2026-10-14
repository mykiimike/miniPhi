export const phi = {
	kind: "module",
	name: "SERIAL",
	label: "Serial",
	description: "Buffered serial line over a UART. The usual printk sink. Each byte received holds the LOW power level for a while, so the line stays out of deep sleep while someone types, and wakes the reader task.",
	define: "SUPPORT_COMMON_SERIAL",
	sources: ["serial.c"],
	headers: ["serial.h"],
	requires: ["CIRCULAR"],
	settings: [
		{ name: "MP_SERIAL_AWAKE", label: "Awake after a received byte", type: "number", unit: "ms", default: 30000, min: 0, max: 600000 },
	],
}
