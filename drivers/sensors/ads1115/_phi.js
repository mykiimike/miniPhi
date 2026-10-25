export const phi = {
	kind: "driver",
	name: "ADS1115",
	vendor: "Texas Instruments",
	label: "ADS1115",
	description: "TI 16-bit 4-channel ADC on I2C.",
	define: "SUPPORT_DRV_ADS1115",
	sources: ["ADS1115.c"],
	headers: ["ADS1115.h"],
	buses: ["i2c"],
	requires: ["DEVICE", "REGMASTER"],
	options: [
		{ name: "drdy", label: "Data ready", type: "pin", irq: true },
	],
}
