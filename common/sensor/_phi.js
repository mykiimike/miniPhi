export const phi = {
	kind: "module",
	name: "SENSOR",
	label: "Sensors",
	description: "Sensor registry. The kernel also registers the MCU temperature here.",
	define: "SUPPORT_COMMON_SENSOR",
	sources: ["sensor.c"],
	headers: ["sensor.h"],
	requires: ["MEM"],
}
