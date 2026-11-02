function pads() {
	return Array.from({ length: 22 }, (_, pin) => "pa" + pin)
}

const gates = {
	USB: { kinds: ["uart"] },
	UART0: { kinds: ["uart"], pins: { uart: { txd: "pa21", rxd: "pa20" } } },
	SPI2: { kinds: ["spi"], pins: { spi: { simo: "pa7", somi: "pa2", clk: "pa6" } } },
	I2C0: { kinds: ["i2c"], pins: { i2c: { sda: "pa5", clk: "pa4" } } },
}

export const phi = {
	kind: "series",
	arch: "esp32",
	name: "c3",
	vendor: "Espressif",
	label: "ESP32-C3",
	tested: true,
	description: "RV32IMC, one core. The PLL stays selected so the USB console does not drop, and the lowest CPU level is 80 MHz because the 480 MHz PLL has no 40 MHz tap. GPIO, a 1 kHz SYSTIMER tick and the USB Serial/JTAG console come up. CHIP_RST bit 2 keeps a host open from resetting the chip. Wi-Fi and BLE are not in this port.",
	specs: {
		core: "RV32IMC",
		clock: "160 MHz",
		pll: "480 MHz",
		source: "40 MHz crystal",
		memory: "400 KB SRAM",
		tick: "1 kHz, 16 MHz SYSTIMER",
	},
	boot: ["startup.c", "clock.c", "esp32.c", "gate.c", "gpio.c", "pixel.c", "interrupt.c", "uart.c"],
	headers: ["include/series.h"],
	headerDir: "arch",
	mcus: [
		{
			name: "esp32c3",
			label: "ESP32-C3",
			cpu: "rv32imc",
			define: "ESP32C3",
			ld: "ld/esp32c3.ld",
			package: "QFN32",
			flash: 4194304,
			ram: 131072,
			pins: pads(),
			irqLines: true,
			gates,
			reserved: [
				{ pin: "pa11", who: "flash VDD" },
				{ pin: "pa12", who: "flash HD" },
				{ pin: "pa13", who: "flash WP" },
				{ pin: "pa14", who: "flash CS" },
				{ pin: "pa15", who: "flash CLK" },
				{ pin: "pa16", who: "flash D" },
				{ pin: "pa17", who: "flash Q" },
				{ pin: "pa18", who: "USB D-" },
				{ pin: "pa19", who: "USB D+" },
			],
		},
	],
}
