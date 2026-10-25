export const phi = {
	kind: "driver",
	name: "INA219",
	label: "INA219",
	description: "TI current and power monitor on I2C, address 0x40.",
	define: "SUPPORT_DRV_INA219",
	sources: ["INA219.c"],
	headers: ["INA219.h"],
	buses: ["i2c"],
	requires: ["DEVICE", "REGMASTER"],
}
