export const phi = {
	kind: "hat",
	name: "xiao-powerbread",
	vendor: "Seeed Studio",
	label: "Seeed Studio XIAO PowerBread",
	tested: false,
	description: "Breadboard power supply and meter that seats one XIAO module. The module's pads D0..D10 route to the elements below, so the hat is module-agnostic: a XIAO ESP32-C6 maps D4 to pa22 and D5 to pa23, D8/D10 to SPI2 and D2 to pa2. The on-glass ST7735S TFT is on SPI (SCK D8, MOSI D10, DC D9, RST D3); chip select is tied and there is no backlight pin, so the panel lights as soon as it is powered. The TI INA3221 on I2C 0x40 measures both rails through 50 mOhm shunts (channel 1 is the 3V3 rail, channel 2 the 5V rail; channel 3 is unused). A resistive dial ladder on D2 reads as a single ADC sample, not a quadrature encoder: the firmware thresholds the count into up, down, press and rest. The output switch is the MT9700 power switch, mechanical (UP off, DOWN on), so the LCD and sensor stay on while the rails are cut. USB-C through the XIAO is the only input; the board bucks to 3V3 on a TPS54202 and passes 5V straight through, each up to 1.5 A with a 2.0 A overcurrent limit. The hat carries no GPIO of its own; the module owns every pin.",
	specs: {
		footprint: "XIAO module socket, breadboard pin rows",
		power: "USB-C through the XIAO, TPS54202 buck 3V3/1.5 A, 5 V pass-through/1.5 A, MT9700 switch, 2.0 A overcurrent",
		sensing: "TI INA3221, 2 channels, 50 mOhm shunts, 0.8 mA resolution, 3.6 A max",
	},
	/* XIAO silk is the hat's own reference. The module _phi.js maps silk to its GPIO. */
	silk: ["D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9", "D10"],
	elements: [
		{
			kind: "display", name: "tft", driver: "ST7735",
			bus: "SPI2", size: "160x80", depth: "16 bit RGB565",
			/* CS is tied to ground on the module, so the hat does not name it. */
			pins: { sck: "D8", mosi: "D10", dc: "D9", reset: "D3" },
		},
		{
			kind: "sensor", name: "meter", driver: "INA3221",
			bus: "I2C0", address: "0x40",
			/* channel 1 is the 3V3 rail, channel 2 the 5V rail, channel 3 unused */
			channels: ["3V3", "5V"],
			shunt: "50 mOhm",
			pins: { sda: "D4", clk: "D5" },
		},
		{
			kind: "dial", name: "dial", pin: "D2", drive: "resistive ladder, single ADC sample",
		},
	],
	connectors: [
		{ name: "USB-C", kind: "power input", via: "XIAO module" },
		{ name: "output", kind: "slide switch", signal: "MT9700", position: "UP off, DOWN on" },
		{ name: "3V3", kind: "rail", limit: "1.5 A", source: "TPS54202 buck" },
		{ name: "5V", kind: "rail", limit: "1.5 A", source: "USB-C pass-through" },
	],
}
