export const phi = {
	kind: "kernel",
	name: "KERNEL",
	label: "Kernel core",
	description: "Boot and panic states, state machine, printk and the options parser. Always compiled.",
	sources: ["kernel.c", "state.c", "printk.c", "utils.c", "mp.c"],
	headers: ["mp.h", "config.h", "list.h", "utils.h", "state.h", "printk.h"],
	headerDir: "",
	requires: ["TASK", "POWER"],
	settings: [
		{ name: "MP_STATE_MAX", label: "Machine states", type: "number", default: 5, min: 3, max: 16 },
		{ name: "_DEBUG", label: "printk output", type: "bool", default: true },
	],
}
