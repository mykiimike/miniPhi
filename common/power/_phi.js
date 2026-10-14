export const phi = {
	kind: "module",
	name: "POWER",
	label: "Power",
	description: "Four levels, HIGH, MEDIUM, LOW and SLEEP, picked by a governor from the load of the kernel loop: the busier, the higher. Drivers and the TUI hold a floor while they need it. SLEEP lets the arch sleep deep between deadlines.",
	required: true,
	sources: ["power.c"],
	headers: ["power.h"],
	settings: [
		{ name: "MP_POWER_PINNED", label: "Start pinned at HIGH, governor off", type: "bool", default: false },
		{ name: "MP_POWER_WINDOW", label: "Load window", type: "number", unit: "ms", default: 100, min: 10, max: 2000 },
		{ name: "MP_POWER_UP", label: "Load that steps up", type: "number", unit: "%", default: 70, min: 30, max: 95 },
		{ name: "MP_POWER_DOWN", label: "Load that steps down", type: "number", unit: "%", default: 20, min: 1, max: 60 },
		{ name: "MP_POWER_DOWN_DELAY", label: "Calm before stepping down", type: "number", unit: "ms", default: 500, min: 0, max: 60000 },
		{ name: "MP_POWER_STOP_MIN", label: "Shortest deep sleep", type: "number", unit: "ms", default: 30, min: 2, max: 10000 },
	],
}
