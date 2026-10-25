const clocks = ["MHZ12_t", "MHZ24_t", "MHZ48_t", "MHZ84_t", "MHZ96_t", "MHZ120_t", "MHZ144_t", "MHZ168_t"]
const clockLabels = ["12 MHz", "24 MHz", "48 MHz", "84 MHz", "96 MHz", "120 MHz", "144 MHz", "168 MHz"]

function port(letter, count) {
	return Array.from({ length: count }, (_, bit) => "p" + letter + bit)
}

/* one pin set per gate: the alternate wired at reset, same rule as the F1 remap 0 */
const gates = {
	USART1: { kinds: ["uart"], pins: { uart: { txd: "pa9", rxd: "pa10" } } },
	USART2: { kinds: ["uart"], pins: { uart: { txd: "pa2", rxd: "pa3" } } },
	USART3: { kinds: ["uart"], pins: { uart: { txd: "pb10", rxd: "pb11" } } },
	UART4: { kinds: ["uart"], pins: { uart: { txd: "pc10", rxd: "pc11" } } },
	UART5: { kinds: ["uart"], pins: { uart: { txd: "pc12", rxd: "pd2" } } },
	USART6: { kinds: ["uart"], pins: { uart: { txd: "pc6", rxd: "pc7" } } },
	SPI1: { kinds: ["spi"], pins: { spi: { simo: "pa7", somi: "pa6", clk: "pa5" } } },
	SPI2: { kinds: ["spi"], pins: { spi: { simo: "pb15", somi: "pb14", clk: "pb13" } } },
	SPI3: { kinds: ["spi"], pins: { spi: { simo: "pb5", somi: "pb4", clk: "pb3" } } },
	I2C1: { kinds: ["i2c"], pins: { i2c: { sda: "pb7", clk: "pb6" } } },
	I2C2: { kinds: ["i2c"], pins: { i2c: { sda: "pb11", clk: "pb10" } } },
	TIM1: { kinds: ["timer"] },
	TIM2: { kinds: ["timer"] },
	TIM3: { kinds: ["timer"] },
	TIM4: { kinds: ["timer"] },
	TIM5: { kinds: ["timer"] },
	TIM6: { kinds: ["timer"] },
	TIM7: { kinds: ["timer"] },
	TIM8: { kinds: ["timer"] },
}

export const phi = {
	kind: "series",
	arch: "stm32",
	name: "f4",
	vendor: "STMicroelectronics",
	label: "STM32F4",
	description: "Cortex-M4F up to 168 MHz, HSI at 16 MHz. GPIO, EXTI, USART, SPI, I2C, the basic timers, ADC and the PLL clock. One pin set per gate, the reset alternate.",
	boot: ["startup.c", "clock.c"],
	headers: ["include/series.h"],
	headerDir: "arch",
	settings: [
		{ name: "MP_CLOCK_HE_FREQ", label: "HIGH level clock", type: "enum", values: clocks, labels: clockLabels, default: "MHZ168_t" },
		{ name: "MP_CLOCK_ME_FREQ", label: "MEDIUM level clock", type: "enum", values: clocks, labels: clockLabels, default: "MHZ84_t" },
		{ name: "MP_CLOCK_LE_FREQ", label: "LOW level clock (SLEEP runs the 16 MHz HSI)", type: "enum", values: clocks, labels: clockLabels, default: "MHZ24_t" },
		{ name: "MP_CLOCK_HSE_HZ", label: "HSE crystal (0 runs on the internal 16 MHz HSI)", type: "number", unit: "Hz", default: 8000000, min: 0, max: 26000000 },
		{ name: "MP_CLOCK_LSE", label: "32.768 kHz LSE crystal on PC14 / PC15", type: "bool", default: false },
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
			name: "stm32f407zg",
			label: "STM32F407ZG",
			cpu: "cortex-m4",
			define: "STM32F407xx",
			ld: "ld/stm32f407xg.ld",
			package: "144-pin LQFP",
			flash: 1048576,
			/* 128 KB system SRAM at 0x20000000. The 64 KB CCM at 0x10000000 is not in this budget. */
			ram: 131072,
			pins: ["a", "b", "c", "d", "e", "f", "g"].flatMap((letter) => port(letter, 16)).concat(["ph0", "ph1"]),
			irqLines: true,
			gates,
		},
	],
}
