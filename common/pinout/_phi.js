export const phi = {
	kind: "module",
	name: "PINOUT",
	label: "Pin patterns",
	description: "Timed on/off patterns on a GPIO, such as a blinking status LED.",
	define: "SUPPORT_COMMON_PINOUT",
	sources: ["pinout.c"],
	headers: ["pinout.h"],
	requires: ["MEM"],
}
