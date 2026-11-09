export const phi = {
	kind: "driver",
	group: ["rtc"],
	name: "PCF8563",
	vendor: "NXP",
	label: "PCF8563 RTC",
	tested: false,
	description: "NXP PCF8563 civil-time RTC on I2C. The chip shares its bus with the OLED: the driver finds the display, borrows its mp_i2c_bus_t, and reads the time registers. The oscillator runs by default. Address 0x51.",
	specs: {
		bus: "I2C",
		address: "0x51",
		backup: "CR1220",
	},
	define: "SUPPORT_DRV_RTC_PCF8563",
	sources: ["pcf8563.c"],
	headers: ["pcf8563.h"],
	requires: ["DEVICE", "I2C"],
	device: { struct: "mp_drv_pcf8563_t", ops: "mp_drv_pcf8563_device" },
	options: [
		{ name: "address", label: "I2C address", type: "enum", values: ["0x51", "0x53"], default: "0x51" },
	],
}
