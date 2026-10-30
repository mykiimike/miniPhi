export const phi = {
	kind: "driver",
	group: ["temperature"],
	name: "TMP006",
	vendor: "Texas Instruments",
	label: "TMP006",
	tested: true,
	description: "Infrared thermopile.",
	specs: {
		bus: "I2C",
		measure: "temperature",
	},
	define: "SUPPORT_DRV_TMP006",
	sources: ["TMP006.c"],
	headers: ["TMP006.h"],
	buses: ["i2c"],
	requires: ["DEVICE", "REGMASTER"],
	options: [
		{ name: "drdy", label: "Data ready", type: "pin", irq: true },
	],
}
