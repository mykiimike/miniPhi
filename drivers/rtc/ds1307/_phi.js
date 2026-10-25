export const phi = {
	kind: "driver",
	name: "DS1307",
	label: "DS1307",
	description: "Maxim DS1307 real-time clock. SDA and SCL are bit-banged, at most 100 kHz, on the pins the board names. The slave address is 0x68. VCC must be 5 V; the backup battery keeps time and does not answer on the bus. One clock device publishes civil time while a subscriber holds it.",
	define: "SUPPORT_DRV_DS1307",
	sources: ["ds1307.c"],
	headers: ["ds1307.h"],
	requires: ["DEVICE"],
	device: { struct: "mp_drv_ds1307_t", ops: "mp_drv_ds1307_device" },
	options: [
		{ name: "model", label: "Model", type: "text", required: true },
		{ name: "serial", label: "Serial", type: "text", required: true },
		{ name: "sda", label: "SDA", type: "pin", required: true },
		{ name: "clk", label: "SCL", type: "pin", required: true },
		{ name: "address", label: "I2C address", type: "enum", values: ["0x68"], default: "0x68" },
	],
}
