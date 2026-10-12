export const phi = {
	kind: "driver",
	name: "NRF8001",
	label: "nRF8001",
	description: "Nordic Bluetooth Low Energy controller on SPI.",
	define: "SUPPORT_DRV_NRF8001",
	sources: ["nRF8001.c", "cmds.c", "evts.c"],
	headers: ["nRF8001.h", "aci.h", "aci_cmds.h", "aci_evts.h", "lib_aci.h"],
	buses: ["spi"],
	options: [
		{ name: "reqn", label: "REQN", type: "pin", required: true },
		{ name: "rdyn", label: "RDYN", type: "pin", irq: true, required: true },
		{ name: "reset", label: "Reset", type: "pin", required: true },
	],
}

export function prepare(ctx) {
	ctx.info("The nRF8001 GATT setup comes from nRFgo Studio (services.h). Include it from your application.")
}
