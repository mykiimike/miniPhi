const clocks = ["MHZ1_t", "MHZ4_t", "MHZ8_t", "MHZ12_t", "MHZ16_t", "MHZ20_t", "MHZ25_t"]
const clockLabels = ["1 MHz", "4 MHz", "8 MHz", "12 MHz", "16 MHz", "20 MHz", "25 MHz"]

function ports(count, last) {
	const pins = []
	for (let port = 1; port <= count; port++)
		for (let bit = 0; bit < 8; bit++)
			pins.push("p" + port + "." + bit)
	for (let bit = 0; bit < last; bit++)
		pins.push("p" + (count + 1) + "." + bit)
	return pins
}

const usci = {
	USCI_A0: { kinds: ["uart", "spi"], pins: { uart: { txd: "p3.4", rxd: "p3.5" }, spi: { simo: "p3.4", somi: "p3.5", clk: "p3.0" } } },
	USCI_B0: { kinds: ["spi", "i2c"], pins: { spi: { simo: "p3.1", somi: "p3.2", clk: "p3.3" }, i2c: { sda: "p3.1", clk: "p3.2" } } },
	USCI_A1: { kinds: ["uart", "spi"], pins: { uart: { txd: "p5.6", rxd: "p5.7" }, spi: { simo: "p5.6", somi: "p5.7", clk: "p3.6" } } },
	USCI_B1: { kinds: ["spi", "i2c"], pins: { spi: { simo: "p3.7", somi: "p5.4", clk: "p5.5" }, i2c: { sda: "p3.7", clk: "p5.4" } } },
	USCI_A2: { kinds: ["uart", "spi"], pins: { uart: { txd: "p9.4", rxd: "p9.5" }, spi: { simo: "p9.4", somi: "p9.5", clk: "p9.0" } } },
	USCI_B2: { kinds: ["spi", "i2c"], pins: { spi: { simo: "p9.1", somi: "p9.2", clk: "p9.3" }, i2c: { sda: "p9.1", clk: "p9.2" } } },
	USCI_A3: { kinds: ["uart", "spi"], pins: { uart: { txd: "p10.4", rxd: "p10.5" }, spi: { simo: "p10.4", somi: "p10.5", clk: "p10.0" } } },
	USCI_B3: { kinds: ["spi", "i2c"], pins: { spi: { simo: "p10.1", somi: "p10.2", clk: "p10.3" }, i2c: { sda: "p10.1", clk: "p10.2" } } },
	TIMER_A0: { kinds: ["timer"] },
	TIMER_A1: { kinds: ["timer"] },
	TIMER_B0: { kinds: ["timer"] },
}

const f543x = {
	package: "100-pin LQFP",
	flash: 262016,
	ram: 16384,
	pins: ports(10, 3),
	irqPorts: [1, 2],
	gates: usci,
}

export const phi = {
	kind: "series",
	arch: "msp430",
	name: "f5xx_f6xx",
	label: "MSP430F5xx / F6xx",
	description: "UCS clock system, PMM core voltage, USCI serial blocks.",
	boot: ["clock.c"],
	settings: [
		{ name: "MP_CLOCK_LE_FREQ", label: "Low-energy clock", type: "enum", values: clocks, labels: clockLabels, default: "MHZ1_t" },
		{ name: "MP_CLOCK_HE_FREQ", label: "High-energy clock", type: "enum", values: clocks, labels: clockLabels, default: "MHZ25_t" },
		{ name: "_LED_DEBUG", label: "Scheduler debug LED on P4.2", type: "bool", default: false },
	],
	reserved: [
		{ pin: "p7.0", who: "XT1 crystal (XIN)" },
		{ pin: "p7.1", who: "XT1 crystal (XOUT)" },
		{ gate: "TIMER_A1", who: "Kernel tick" },
		{ pin: "p4.2", who: "Scheduler debug LED", when: "_LED_DEBUG" },
	],
	mcus: [
		{ name: "msp430f5438", label: "MSP430F5438", define: "__MSP430F5438__", ld: "../ld/msp430f5438.cmd", ...f543x },
		{ name: "msp430f5438a", label: "MSP430F5438A", define: "__MSP430F5438A__", ld: "../ld/msp430f5438a.cmd", ...f543x },
	],
}
