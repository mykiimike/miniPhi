export const phi = {
	kind: "driver",
	group: ["pressure", "altitude"],
	name: "MPL3115A2",
	vendor: "Freescale",
	label: "MPL3115A2",
	tested: true,
	description: "Barometric pressure and altitude.",
	specs: {
		bus: "I2C",
		measure: "pressure",
	},
	define: "SUPPORT_DRV_MPL3115A2",
	sources: ["MPL3115A2.c"],
	headers: ["MPL3115A2.h"],
	buses: ["i2c"],
	requires: ["DEVICE", "REGMASTER"],
	options: [
		{ name: "drdy", label: "Data ready", type: "pin", irq: true },
	],
}
