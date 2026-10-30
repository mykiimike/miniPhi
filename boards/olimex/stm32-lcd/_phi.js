export const phi = {
	kind: "board",
	name: "stm32-lcd",
	vendor: "Olimex",
	label: "Olimex STM32-LCD",
	tested: true,
	description: "STM32F103ZE board, revision A, with mini USB, microSD, UEXT and a 20-pin JTAG header. The TFT is shown by the TUI. A 4-wire touch panel, an SMB380, a FLIR Lepton and an MCP73833 sit on the board. The Lepton MOSI stays unconnected. D1 to D4 are supply diodes. Flashed with the ARM-USB-OCD-H.",
	specs: {
		crystal: "8 MHz",
		lse: "32.768 kHz",
		display: "ILI9320 TFT",
		console: "USART1",
	},
	arch: "stm32",
	series: "f1",
	mcu: "stm32f103ze",
	toolchain: "gcc",
	requires: ["arm-usb-ocd-h"],
	app: "sysinfo",
	defines: [],
	modules: ["SERIAL", "DEVICE", "RESISTIVE", "TUI", "KFLASH", "RSA", "ED25519", "ECDSA_P256"],
	drivers: ["ILI932X", "SMB380", "LEPTON", "MCP73833"],
	sources: ["touch.c"],
	settings: {
		/* 2560: the Lepton frame and the device records sit in BSS beside this pool */
		MP_MEM_SIZE: 2560,
		MP_MEM_CHUNK: 64,
		/* one slot for the MCP73833 sample task, taken while its page is open */
		MP_TASK_MAX: 13,
		MP_TUI_TEXT_MAX: 96,
		MP_CLOCK_LSE: true,
		/* one device PHY; the System USB table keeps a row per slot */
		MP_USB_PORT_MAX: 1,
		/* newlib heap. Nothing in this firmware mallocs; 128 frees a task slot. */
		HEAP_SIZE: 128,
	},
	/* touch.c claims PC0 to PC3; the crystal, USB and microSD pins stay reserved too. PC6 and PC7 belong to the charger device. */
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
			options: { gate: "FSMC", cs: "pd7", rs: "pe3", reset: "pe2", backlight: "pd13", backlightLow: true, backlightTimer: "TIM4", orientation: "landscape", model: "ili932x" },
		},
		{
			driver: "SMB380",
			name: "accel",
			options: { gate: "I2C2", sda: "pb11", clk: "pb10", int: "pe4", address: "0x38", range: "2", bandwidth: "25", model: "smb380" },
		},
		{
			driver: "LEPTON",
			name: "lepton",
			options: {
				spiGate: "SPI2", simo: "pb15", somi: "pb14", sck: "pb13", cs: "pb12",
				i2cGate: "I2C1", sda: "pb7", scl: "pb6",
				model: "lepton",
			},
		},
		{
			driver: "MCP73833",
			name: "charger",
			options: { stat1: "pc6", stat2: "pc7", model: "mcp73833", serial: "adafruit" },
		},
	],
}
