export const phi = {
	kind: "hat",
	name: "xiao-expansion",
	vendor: "Seeed Studio",
	label: "Seeed Studio XIAO Expansion Board v1.2",
	tested: false,
	description: "Breakout base that seats one XIAO module. The module's pads D0..D10 route to the elements below, so the hat is module-agnostic: a XIAO ESP32-C6 maps D4 to pa22 and D5 to pa23, a XIAO ESP32-S3 maps them differently. The on-glass SSD1306 and the PCF8563 RTC share the I2C bus on D4/D5 at 0x3C and 0x51. The microSD is on SPI (D8/D9/D10, CS on D2). The user button on D1 is active low, the passive buzzer on D3 needs a square wave. Two Grove I2C sockets parallel the OLED bus, one Grove UART sits on D6/D7, one Grove A/D on D0. A JST 2.0 LiPo charger (~460 mA) and a 5 V servo header share D0. SYS_3V3 follows the module 3V3 rail; USB 5 V is boosted to 5 V and bucked to 3V3 on board. The hat carries no GPIO of its own; the module owns every pin.",
	specs: {
		footprint: "XIAO module socket",
		power: "SYS_3V3 from module 3V3, USB 5 V boost ETA1038 5 V/700 mA, buck ETA3410 3V3/1.2 A",
		rtc: "PCF8563 + CR1220",
		charger: "ETA6003 LiPo 4.2 V/500 mA, STAT LED",
	},
	/* XIAO silk is the hat's own reference. The module _phi.js maps silk to its GPIO. */
	silk: ["D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9", "D10"],
	elements: [
		{
			kind: "display", name: "oled", driver: "SSD1306",
			bus: "I2C0", address: "0x3C", size: "128x64", depth: "1 bit",
			pins: { sda: "D4", clk: "D5" },
		},
		{
			kind: "clock", name: "rtc", driver: "PCF8563",
			bus: "I2C0", address: "0x51", backup: "CR1220",
			pins: { sda: "D4", clk: "D5" },
		},
		{
			kind: "storage", name: "sd",
			bus: "SPI2",
			pins: { sck: "D8", miso: "D9", mosi: "D10", cs: "D2" },
		},
		{
			kind: "button", name: "user", pin: "D1", active: "low",
		},
		{
			kind: "buzzer", name: "buzzer", pin: "D3", drive: "square wave",
		},
		{
			kind: "grove", name: "i2c-a", bus: "I2C0",
			pins: { sda: "D4", clk: "D5" },
		},
		{
			kind: "grove", name: "i2c-b", bus: "I2C0",
			pins: { sda: "D4", clk: "D5" },
		},
		{
			kind: "grove", name: "uart", bus: "UART0",
			pins: { tx: "D6", rx: "D7" },
		},
		{
			kind: "grove", name: "ad", pin: "D0",
		},
	],
	connectors: [
		{ name: "RESET", kind: "push button" },
		{ name: "SWD", pins: ["swclk", "swdio"] },
		{ name: "servo", signal: "D0", power: "5 V" },
		{ name: "battery", kind: "JST 2.0", cell: "LiPo 3.7 V", charge: "460 mA" },
	],
}
