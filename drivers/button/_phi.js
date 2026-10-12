export const phi = {
	kind: "driver",
	name: "BUTTON",
	label: "Button",
	description: "Debounced push button on an interrupt pin, with press-pattern events.",
	define: "SUPPORT_DRV_BUTTON",
	sources: ["button.c"],
	headers: ["button.h"],
	options: [
		{ name: "port", label: "Pin", type: "pin", irq: true, required: true },
	],
}
