export const phi = {
	kind: "board",
	name: "stm32-lcd",
	vendor: "Olimex",
	label: "Olimex STM32-LCD",
	description: "STM32F103ZE board, revision A: 8 MHz and 32.768 kHz crystals, mini USB, microSD, UEXT, 20-pin JTAG. ILI9320 TFT on the FSMC (NE1, RS on A19), shown by the TUI. The 4-wire touch panel is on PC0 to PC3 (YD, YU, XL, XR). A Bosch SMB380 accelerometer is on I2C2 (PB10 SCL, PB11 SDA), its interrupt on PE4. A FLIR Lepton breakout is on SPI2 (PB13 SCK, PB14 MISO, PB15 MOSI, chip select PB12) and I2C1 (PB6 SCL, PB7 SDA); MOSI stays unconnected at the module. No user LED: D1 to D4 are supply diodes. Flashed with the ARM-USB-OCD-H.",
	arch: "stm32",
	series: "f1",
	mcu: "stm32f103ze",
	toolchain: "gcc",
	requires: ["arm-usb-ocd-h"],
	app: "sysinfo",
	defines: [],
	modules: ["SERIAL", "DEVICE", "RESISTIVE", "TUI", "KFLASH", "RSA"],
	drivers: ["ILI932X", "SMB380", "LEPTON"],
	sources: ["touch.c"],
	settings: {
		/* 2560: the Lepton frame and the device records sit in BSS beside this pool */
		MP_MEM_SIZE: 2560,
		MP_MEM_CHUNK: 64,
		/* screen, USB, ADC, touch, accelerometer and the Lepton each take tasks */
		MP_TASK_MAX: 12,
		MP_TUI_TEXT_MAX: 96,
		MP_CLOCK_LSE: true,
		/* one device PHY; the System USB table keeps a row per slot */
		MP_USB_PORT_MAX: 1,
		/* newlib heap. The kernel pool is MP_MEM_SIZE; nothing in this firmware mallocs. */
		HEAP_SIZE: 256,
	},
	/* touch.c claims PC0 to PC3; the crystal, USB and microSD pins stay reserved too */
	reserved: [
		{ pin: "pc0", who: "4-wire touch YD" },
		{ pin: "pc1", who: "4-wire touch YU" },
		{ pin: "pc2", who: "4-wire touch XL" },
		{ pin: "pc3", who: "4-wire touch XR" },
		{ pin: "pc14", who: "32.768 kHz crystal, RTC" },
		{ pin: "pc15", who: "32.768 kHz crystal, RTC" },
		{ pin: "pa11", who: "USB D-" },
		{ pin: "pa12", who: "USB D+" },
		{ pin: "pd3", who: "USB pull-up" },
		{ pin: "pc8", who: "microSD DAT0" },
		{ pin: "pc9", who: "microSD DAT1" },
		{ pin: "pc10", who: "microSD DAT2" },
		{ pin: "pc11", who: "microSD DAT3" },
		{ pin: "pc12", who: "microSD CLK" },
		{ pin: "pd2", who: "microSD CMD" },
	],
	devices: [
		{ bus: "uart", name: "console", options: { gate: "USART1", txd: "pa9", rxd: "pa10", baudRate: "115200" } },
		{
			driver: "ILI932X",
			name: "lcd",
			options: { gate: "FSMC", cs: "pd7", rs: "pe3", reset: "pe2", backlight: "pd13", backlightLow: true, backlightTimer: "TIM4", orientation: "landscape" },
		},
		{
			driver: "SMB380",
			name: "accel",
			options: { gate: "I2C2", sda: "pb11", clk: "pb10", int: "pe4", address: "0x38", range: "2", bandwidth: "25" },
		},
		{
			driver: "LEPTON",
			name: "lepton",
			options: {
				spiGate: "SPI2", simo: "pb15", somi: "pb14", sck: "pb13", cs: "pb12",
				i2cGate: "I2C1", sda: "pb7", scl: "pb6",
			},
		},
	],
}
