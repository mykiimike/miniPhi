export const phi = {
	kind: "series",
	arch: "rp2",
	name: "rp2350",
	vendor: "Raspberry Pi",
	label: "RP2350",
	tested: false,
	description: "Cortex-M33 only, at 150 MHz from the 12 MHz crystal through PLL_SYS. PLL_USB supplies the 48 MHz USB clock, and the USB gate is a CDC console on the device controller. This is the RP2350A (QFN-60, GPIO0-29). QSPI flash is a dedicated boot pin group, not GPIO0-5. GPIO0-29 are pa0-pa29. Boot is the QSPI chip-select strap, not a GPIO, and reset is the RUN pin.",
	schedulers: ["0", "1", "2"],
	scheduler: "2",
	/* The second Cortex-M33 shares the one task list. */
	cpus: 2,
	specs: {
		core: "Cortex-M33",
		clock: "150 MHz",
		source: "12 MHz XOSC",
		memory: "520 KB SRAM",
		tick: "1 kHz, SysTick",
	},
	boot: ["startup.c", "clock.c", "pixel.c"],
	headers: ["include/series.h"],
	headerDir: "arch",
	mcus: [
		{
			name: "rp2350",
			label: "RP2350",
			cpu: "cortex-m33",
			define: "RP2350",
			ld: "ld/rp2350.ld",
			package: "QFN60",
			/* W25Q128 on the RP2350-LCD-1.47-A, 16 MB, mapped at the XIP window */
			flash: 16777216,
			/* SRAM0-SRAM9, 512 KB striped plus two 4 KB banks, one contiguous region */
			ram: 532480,
			pins: Array.from({ length: 30 }, (_, i) => "pa" + i),
			irqLines: true,
			/*
			 * Defaults are real FUNCSEL pins on the RP2350A, kept off the LCD
			 * (GPIO16-21). USB is the dedicated PHY, not a GPIO pair.
			 * UART0 TX/RX GPIO12/13 is function 2. UART1 TX/RX GPIO8/9 is function 2.
			 * SPI0 SCK/MOSI/MISO GPIO2/3/4 is function 1. SPI1 is GPIO26/27/24
			 * (GPIO18/19/20 are SPI0, and they are the LCD on this board).
			 * I2C0 SDA/SCL GPIO0/1 is function 3. GPIO6/7 is I2C1, not I2C0.
			 * I2C1 SDA/SCL GPIO14/15 is function 3.
			 */
			gates: {
				USB: { kinds: ["uart"] },
				UART0: { kinds: ["uart"], pins: { uart: { txd: "pa12", rxd: "pa13" } } },
				UART1: { kinds: ["uart"], pins: { uart: { txd: "pa8", rxd: "pa9" } } },
				SPI0: { kinds: ["spi"], pins: { spi: { simo: "pa3", somi: "pa4", clk: "pa2" } } },
				SPI1: { kinds: ["spi"], pins: { spi: { simo: "pa27", somi: "pa24", clk: "pa26" } } },
				I2C0: { kinds: ["i2c"], pins: { i2c: { sda: "pa0", clk: "pa1" } } },
				I2C1: { kinds: ["i2c"], pins: { i2c: { sda: "pa14", clk: "pa15" } } },
			},
		},
	],
}
