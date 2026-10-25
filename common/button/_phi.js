export const phi = {
	kind: "module",
	name: "BUTTON",
	label: "GPIO button",
	description: "A push button on a GPIO pin, with no controller chip. The board names each one: the pin, the active level, the model and the serial. This module arms the interrupt and registers an MP_DEVICE_BUTTON. The first subscriber starts it.",
	define: "SUPPORT_COMMON_BUTTON",
	sources: ["button.c"],
	headers: ["button.h"],
	requires: ["DEVICE"],
}
