function pads() {
	const pins = []
	let pin

	for (pin = 0; pin <= 19; pin++)
		pins.push("pa" + pin)
	pins.push("pa21", "pa22", "pa23")
	for (pin = 25; pin <= 27; pin++)
		pins.push("pa" + pin)
	for (pin = 32; pin <= 39; pin++)
		pins.push("pa" + pin)
	return pins
}

const gates = {
	UART0: { kinds: ["uart"], pins: { uart: { txd: "pa1", rxd: "pa3" } } },
	UART1: { kinds: ["uart"], pins: { uart: { txd: "pa10", rxd: "pa9" } } },
	UART2: { kinds: ["uart"], pins: { uart: { txd: "pa17", rxd: "pa16" } } },
	SPI2: { kinds: ["spi"], pins: { spi: { simo: "pa13", somi: "pa12", clk: "pa14" } } },
	SPI3: { kinds: ["spi"], pins: { spi: { simo: "pa23", somi: "pa19", clk: "pa18" } } },
	I2C0: { kinds: ["i2c"], pins: { i2c: { sda: "pa21", clk: "pa22" } } },
	I2C1: { kinds: ["i2c"], pins: { i2c: { sda: "pa25", clk: "pa26" } } },
}

export const phi = {
	kind: "series",
	arch: "esp32",
	name: "classic",
	vendor: "Espressif",
	label: "ESP32",
	tested: true,
	description: "Original ESP32, Xtensa LX6, two cores. HIGH runs the CPUs at 240 MHz from the 480 MHz PLL with APB at 80 MHz; the other levels run on the 40 MHz crystal with APB at 40 MHz. The PLL stays up once started. UARTs up to 250 kbaud are clocked from the 1 MHz REF_TICK, so the level can change under them; a faster UART stays on APB and pins the level. There is no SYSTIMER: TIMG0 timer 0 runs at 1 MHz and alarms at 1 kHz on a level-2 line of the PRO CPU, which keeps the tick count. Tasks are preemptive on one stack: FROM_CPU_0 uses the other level-2 line, so it cuts a background task and the tick runs when that handler returns. The scheduler releases the APP CPU onto its own stack. TIMG1 wakes it every tick and FROM_CPU_1 is its scheduler line. Wi-Fi and BLE are not in this port. I2C has no direct IO_MUX function; the gate pins are the conventional matrix pins.",
	cpus: 2,
	schedulers: ["0", "1", "2"],
	scheduler: "2",
	specs: {
		core: "Xtensa LX6",
		clock: "240 MHz PLL, 40 MHz crystal at low load",
		source: "40 MHz crystal",
		memory: "192 KB SRAM2",
		tick: "1 kHz, TIMG0 on core 0 and TIMG1 on core 1",
	},
	boot: ["startup.c", "clock.c", "machine.c", "gate.c", "gpio.c", "interrupt.c", "cpu.c", "uart.c", "spi.c"],
	headers: ["include/series.h"],
	headerDir: "arch",
	mcus: [
		{
			name: "esp32",
			label: "ESP32",
			cpu: "lx6",
			define: "ESP32CLASSIC",
			ld: "ld/esp32.ld",
			package: "QFN48",
			flash: 4194304,
			ram: 196608,
			pins: pads(),
			irqLines: true,
			gates,
			reserved: [
				{ pin: "pa6", who: "flash" },
				{ pin: "pa7", who: "flash" },
				{ pin: "pa8", who: "flash" },
				{ pin: "pa9", who: "flash" },
				{ pin: "pa10", who: "flash" },
				{ pin: "pa11", who: "flash" },
			],
		},
	],
}
