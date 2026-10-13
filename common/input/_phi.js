export const phi = {
	kind: "module",
	name: "INPUT",
	label: "Input",
	description: "Input devices listed on the kernel: touch panels, mice with a wheel, keyboards and terminals. Feeding only queues; reading turns it into down, click, drag and drop, wheel and key events. The TUI reads every input on each tick.",
	define: "SUPPORT_COMMON_INPUT",
	sources: ["input.c"],
	headers: ["input.h"],
	settings: [
		{ name: "MP_INPUT_QUEUE", label: "Queued transitions per input", type: "number", default: 16, min: 4, max: 64 },
		{ name: "MP_INPUT_DRAG_THRESHOLD", label: "Move before a press becomes a drag", type: "number", unit: "px", default: 6, min: 1, max: 64 },
	],
}
