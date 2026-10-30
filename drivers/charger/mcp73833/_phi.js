export const phi = {
	kind: "driver",
	group: ["charger"],
	name: "MCP73833",
	vendor: "Microchip",
	label: "MCP73833",
	tested: true,
	description: "STAT1 and STAT2 are active-low open-drain inputs on the pins the board names. STAT1 low is charging, STAT2 low is charge complete, both released is waiting. Both low leaves the device on its last state. The board pulls the lines up to the MCU rail, because the charger's LEDs pull toward its 5 V input. One charger device publishes that state while a subscriber holds it.",
	specs: {
		bus: "GPIO",
		measure: "Li-Ion charge",
	},
	define: "SUPPORT_DRV_MCP73833",
	sources: ["mcp73833.c"],
	headers: ["mcp73833.h"],
	requires: ["DEVICE"],
	device: { struct: "mp_drv_mcp73833_t", ops: "mp_drv_mcp73833_device" },
	options: [
		{ name: "model", label: "Model", type: "text", required: true },
		{ name: "serial", label: "Serial", type: "text", required: true },
		{ name: "stat1", label: "STAT1", type: "pin", required: true },
		{ name: "stat2", label: "STAT2", type: "pin", required: true },
	],
}
