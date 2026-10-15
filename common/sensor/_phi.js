export const phi = {
	kind: "module",
	name: "SENSOR",
	label: "Sensors",
	description: "Sensor registry: drivers publish typed measures (C, Pa, m, g, dps, gauss, V, A) from tasks or interrupts, readers get a coherent copy with its age. The kernel also registers the MCU temperature here.",
	define: "SUPPORT_COMMON_SENSOR",
	sources: ["sensor.c"],
	headers: ["sensor.h"],
	requires: ["MEM"],
}
