const clocks = ["MHZ8_t", "MHZ16_t", "MHZ24_t", "MHZ32_t", "MHZ40_t", "MHZ48_t", "MHZ56_t", "MHZ64_t", "MHZ72_t"]
const clockLabels = ["8 MHz", "16 MHz", "24 MHz", "32 MHz", "40 MHz", "48 MHz", "56 MHz", "64 MHz", "72 MHz"]

function port(letter, count) {
	return Array.from({ length: count }, (_, bit) => "p" + letter + bit)
}

const medium = {
	USART1: { kinds: ["uart"], pins: { uart: { txd: "pa9", rxd: "pa10" } } },
	USART2: { kinds: ["uart"], pins: { uart: { txd: "pa2", rxd: "pa3" } } },
	SPI1: { kinds: ["spi"], pins: { spi: { simo: "pa7", somi: "pa6", clk: "pa5" } } },
	I2C1: { kinds: ["i2c"], pins: { i2c: { sda: "pb7", clk: "pb6" } } },
	TIM1: { kinds: ["timer"] },
	TIM2: { kinds: ["timer"] },
	TIM3: { kinds: ["timer"] },
	TIM4: { kinds: ["timer"] },
}

export const phi = {
	kind: "series",
	arch: "stm32",
	name: "f1",
	label: "STM32F1",
	description: "Cortex-M3 up to 72 MHz. HSE crystal and PLL, SysTick kernel tick at 1 kHz, JTAG or SWD debug.",
	boot: ["startup.c", "clock.c"],
	headers: ["include/series.h"],
	headerDir: "arch",
	settings: [
		{ name: "MP_CLOCK_HE_FREQ", label: "System clock", type: "enum", values: clocks, labels: clockLabels, default: "MHZ72_t" },
		{ name: "MP_CLOCK_HSE_HZ", label: "HSE crystal (0 runs on the internal 8 MHz HSI)", type: "number", unit: "Hz", default: 8000000, min: 0, max: 16000000 },
		{ name: "_SWD_ONLY", label: "SWD only, frees PA15, PB3 and PB4 from JTAG", type: "bool", default: false },
	],
	reserved: [
		{ pin: "pa13", who: "JTAG TMS / SWDIO" },
		{ pin: "pa14", who: "JTAG TCK / SWCLK" },
		{ pin: "pa15", who: "JTAG TDI", when: "!_SWD_ONLY" },
		{ pin: "pb3", who: "JTAG TDO", when: "!_SWD_ONLY" },
		{ pin: "pb4", who: "JTAG nTRST", when: "!_SWD_ONLY" },
	],
	mcus: [
		{
			name: "stm32f103t8",
			label: "STM32F103T8",
			cpu: "cortex-m3",
			define: "STM32F10X_MD",
			ld: "ld/stm32f103x8.ld",
			package: "36-pin VFQFPN",
			flash: 65536,
			ram: 20480,
			pins: [...port("a", 16), ...port("b", 8)],
			irqLines: true,
			gates: medium,
		},
	],
}
