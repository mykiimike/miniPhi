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

/* remap 0 pins, the ones bonded out of reset */
const high = {
	...medium,
	USART3: { kinds: ["uart"], pins: { uart: { txd: "pb10", rxd: "pb11" } } },
	UART4: { kinds: ["uart"], pins: { uart: { txd: "pc10", rxd: "pc11" } } },
	UART5: { kinds: ["uart"], pins: { uart: { txd: "pc12", rxd: "pd2" } } },
	SPI2: { kinds: ["spi"], pins: { spi: { simo: "pb15", somi: "pb14", clk: "pb13" } } },
	SPI3: { kinds: ["spi"], pins: { spi: { simo: "pb5", somi: "pb4", clk: "pb3" } } },
	I2C2: { kinds: ["i2c"], pins: { i2c: { sda: "pb11", clk: "pb10" } } },
	TIM5: { kinds: ["timer"] },
	TIM6: { kinds: ["timer"] },
	TIM7: { kinds: ["timer"] },
	TIM8: { kinds: ["timer"] },
	/* D0 to D15, NOE and NWE; cs and rs are options of the device */
	FSMC: {
		kinds: ["fsmc"],
		claims: ["pd14", "pd15", "pd0", "pd1", "pe7", "pe8", "pe9", "pe10", "pe11", "pe12", "pe13", "pe14", "pe15", "pd8", "pd9", "pd10", "pd4", "pd5"],
	},
}

export const phi = {
	kind: "series",
	arch: "stm32",
	name: "f1",
	label: "STM32F1",
	description: "Cortex-M3 up to 72 MHz. HSE crystal and PLL, a clock per power level, tickless SysTick kernel tick at 1 kHz, Stop woken by the RTC on the LSE crystal, JTAG or SWD debug.",
	boot: ["startup.c", "clock.c"],
	headers: ["include/series.h"],
	headerDir: "arch",
	settings: [
		{ name: "MP_CLOCK_HE_FREQ", label: "HIGH level clock", type: "enum", values: clocks, labels: clockLabels, default: "MHZ72_t" },
		{ name: "MP_CLOCK_ME_FREQ", label: "MEDIUM level clock", type: "enum", values: clocks, labels: clockLabels, default: "MHZ24_t" },
		{ name: "MP_CLOCK_LE_FREQ", label: "LOW level clock (SLEEP runs HSI at 8 MHz)", type: "enum", values: clocks, labels: clockLabels, default: "MHZ8_t" },
		{ name: "MP_CLOCK_HSE_HZ", label: "HSE crystal (0 runs on the internal 8 MHz HSI)", type: "number", unit: "Hz", default: 8000000, min: 0, max: 16000000 },
		{ name: "MP_CLOCK_LSE", label: "32.768 kHz LSE crystal on PC14 / PC15: RTC and Stop at SLEEP", type: "bool", default: false },
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
		{
			name: "stm32f103ze",
			label: "STM32F103ZE",
			cpu: "cortex-m3",
			define: "STM32F10X_HD",
			ld: "ld/stm32f103xe.ld",
			package: "144-pin LQFP",
			flash: 524288,
			ram: 65536,
			pins: ["a", "b", "c", "d", "e", "f", "g"].flatMap((letter) => port(letter, 16)),
			irqLines: true,
			gates: high,
		},
	],
}
