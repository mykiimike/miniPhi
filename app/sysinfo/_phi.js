export const phi = {
	kind: "app",
	name: "sysinfo",
	label: "System info",
	description: "Prints the miniPhi banner, the version, the build and the CPU on a UART console, then reports uptime, tasks and sensors.",
	sources: ["main.c"],
	requires: ["SERIAL", "SENSOR"],
	devices: [
		{ bus: "uart", name: "console", label: "Console" },
	],
	settings: [
		{ name: "SYSINFO_LINE_DELAY", label: "Delay between boot lines", type: "number", unit: "ms", default: 60, min: 10, max: 1000 },
		{ name: "SYSINFO_PERIOD", label: "Report period", type: "number", unit: "ms", default: 5000, min: 100, max: 60000 },
	],
}
