function pads() {
	const pins = []
	let pin

	for (pin = 0; pin <= 21; pin++)
		pins.push("pa" + pin)
	for (pin = 26; pin <= 48; pin++)
		pins.push("pa" + pin)
	return pins
}

const gates = {
	USB: { kinds: ["uart"] },
	UART0: { kinds: ["uart"], pins: { uart: { txd: "pa43", rxd: "pa44" } } },
	SPI2: { kinds: ["spi"], pins: { spi: { simo: "pa9", somi: "pa8", clk: "pa7" } } },
	I2C0: { kinds: ["i2c"], pins: { i2c: { sda: "pa5", clk: "pa6" } } },
}

export const phi = {
	kind: "series",
	arch: "esp32",
	name: "s3",
	vendor: "Espressif",
	label: "ESP32-S3",
	tested: true,
	description: "The PLL stays up so the USB console does not drop. The APP CPU stays in reset.",
	specs: {
		core: "Xtensa LX7",
		clock: "240 MHz",
		pll: "480 MHz",
		source: "40 MHz crystal",
		memory: "512 KB SRAM",
		tick: "1 kHz, 16 MHz SYSTIMER",
	},
	boot: ["startup.c", "clock.c", "machine.c", "gate.c", "gpio.c", "interrupt.c", "uart.c", "adc.c"],
	headers: ["include/series.h", "include/adc.h"],
	headerDir: "arch",
	mcus: [
		{
			name: "esp32s3",
			label: "ESP32-S3",
			cpu: "lx7",
			define: "ESP32S3",
			ld: "ld/esp32s3.ld",
			package: "QFN56",
			flash: 16777216,
			ram: 397312,
			pins: pads(),
			irqLines: true,
			gates,
			reserved: [
				{ pin: "pa19", who: "USB D-" },
				{ pin: "pa20", who: "USB D+" },
				{ pin: "pa26", who: "flash" },
				{ pin: "pa27", who: "flash" },
				{ pin: "pa28", who: "flash" },
				{ pin: "pa29", who: "flash" },
				{ pin: "pa30", who: "flash" },
				{ pin: "pa31", who: "flash" },
				{ pin: "pa32", who: "flash" },
				{ pin: "pa33", who: "PSRAM" },
				{ pin: "pa34", who: "PSRAM" },
				{ pin: "pa35", who: "PSRAM" },
				{ pin: "pa36", who: "PSRAM" },
				{ pin: "pa37", who: "PSRAM" },
				{ pin: "pa45", who: "VDD_SPI strap" },
				{ pin: "pa46", who: "ROM log strap" },
			],
		},
	],
}
