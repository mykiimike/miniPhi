export const phi = {
	kind: "driver",
	name: "ADS124X",
	vendor: "Texas Instruments",
	label: "ADS124x",
	description: "TI 24-bit delta-sigma ADC family on SPI.",
	define: "SUPPORT_DRV_ADS124X",
	sources: ["ADS124x.c"],
	headers: ["ADS124x.h"],
	buses: ["spi"],
	requires: ["DEVICE", "REGMASTER"],
	options: [
		{ name: "version", label: "Part", type: "enum", values: ["ADS1246", "ADS1247", "ADS1248"], required: true },
		{ name: "drdy", label: "Data ready", type: "pin", irq: true, required: true },
		{ name: "cs", label: "Chip select", type: "pin", required: true },
		{ name: "reset", label: "Reset", type: "pin", required: true },
		{ name: "start", label: "Start", type: "pin", required: true },
	],
}
