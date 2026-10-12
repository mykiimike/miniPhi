export const phi = {
	kind: "driver",
	name: "LCD_NOKIA3310",
	label: "Nokia 3310 LCD",
	description: "84x48 PCD8544 display on SPI.",
	define: "SUPPORT_DRV_LCD_NOKIA3310",
	sources: ["nokia3310.c"],
	headers: ["nokia3310.h"],
	buses: ["spi"],
	options: [
		{ name: "dc", label: "Data/command", type: "pin", required: true },
		{ name: "contrast", label: "Reset (contrast)", type: "pin", required: true },
	],
}
