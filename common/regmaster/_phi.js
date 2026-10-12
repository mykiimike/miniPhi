export const phi = {
	kind: "module",
	name: "REGMASTER",
	label: "Register master",
	description: "Queued register reads and writes over I2C or SPI. Used by the sensor drivers.",
	define: "SUPPORT_COMMON_REGMASTER",
	sources: ["regMaster.c"],
	headers: ["regMaster.h"],
	requires: ["MEM"],
}
