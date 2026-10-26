export const phi = {
	kind: "module",
	name: "POWER",
	label: "Power",
	description: "Four levels, HIGH, MEDIUM, LOW and SLEEP. The governor follows the load of the kernel loop: up at once, down after the calm delay. Holds are frequency floors or vetoes of deep sleep. SLEEP lets the arch sleep deep between deadlines when nothing vetoes it.",
	required: true,
	sources: ["power.c"],
	headers: ["power.h"],
	settings: [
		{ name: "MP_POWER_PINNED", label: "Start pinned at HIGH, governor off", type: "bool", default: false },
		{ name: "MP_POWER_WINDOW", label: "Load window", type: "number", unit: "ms", default: 100, min: 10, max: 2000 },
		{ name: "MP_POWER_TARGET", label: "Load the running clock should sit at", type: "number", unit: "%", default: 60, min: 1, max: 100 },
		{ name: "MP_POWER_DOWN_DELAY", label: "Calm before stepping down", type: "number", unit: "ms", default: 500, min: 0, max: 60000 },
		{ name: "MP_POWER_STOP_MIN", label: "Shortest deep sleep", type: "number", unit: "ms", default: 30, min: 2, max: 10000 },
	],
}
