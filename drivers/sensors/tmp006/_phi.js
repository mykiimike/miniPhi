export const phi = {
	kind: "driver",
	name: "TMP006",
	label: "TMP006",
	description: "TI infrared thermopile temperature sensor on I2C.",
	define: "SUPPORT_DRV_TMP006",
	sources: ["TMP006.c"],
	headers: ["TMP006.h"],
	buses: ["i2c"],
	requires: ["DEVICE", "REGMASTER"],
	options: [
		{ name: "drdy", label: "Data ready", type: "pin", irq: true },
	],
}
