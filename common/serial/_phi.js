export const phi = {
	kind: "module",
	name: "SERIAL",
	label: "Serial",
	description: "Buffered serial line over a UART. The usual printk sink.",
	define: "SUPPORT_COMMON_SERIAL",
	sources: ["serial.c"],
	headers: ["serial.h"],
	requires: ["CIRCULAR"],
}
