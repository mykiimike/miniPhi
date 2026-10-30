export const phi = {
	kind: "driver",
	group: ["current", "power"],
	name: "INA219",
	vendor: "Texas Instruments",
	label: "INA219",
	tested: true,
	description: "Current and power monitor.",
	specs: {
		bus: "I2C",
		address: "0x40",
		measure: "current",
	},
	define: "SUPPORT_DRV_INA219",
	sources: ["INA219.c"],
	headers: ["INA219.h"],
	buses: ["i2c"],
	requires: ["DEVICE", "REGMASTER"],
}
