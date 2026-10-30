export const phi = {
	kind: "driver",
	group: ["adc"],
	name: "ADS1115",
	vendor: "Texas Instruments",
	label: "ADS1115",
	tested: true,
	description: "The four inputs share one converter.",
	specs: {
		bus: "I2C",
		depth: "16 bit",
		channels: "4",
	},
	define: "SUPPORT_DRV_ADS1115",
	sources: ["ADS1115.c"],
	headers: ["ADS1115.h"],
	buses: ["i2c"],
	requires: ["DEVICE", "REGMASTER"],
	options: [
		{ name: "drdy", label: "Data ready", type: "pin", irq: true },
	],
}
