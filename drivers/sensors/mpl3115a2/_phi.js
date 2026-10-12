export const phi = {
	kind: "driver",
	name: "MPL3115A2",
	label: "MPL3115A2",
	description: "Freescale barometric pressure and altitude sensor on I2C.",
	define: "SUPPORT_DRV_MPL3115A2",
	sources: ["MPL3115A2.c"],
	headers: ["MPL3115A2.h"],
	buses: ["i2c"],
	requires: ["SENSOR", "REGMASTER"],
	options: [
		{ name: "drdy", label: "Data ready", type: "pin", irq: true },
	],
}
