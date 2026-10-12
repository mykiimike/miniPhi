export const board = {
	name: "olimex",
	arch: "msp430",
	series: "f5xx_f6xx",
	mcu: "msp430f5438",
	defines: ["__OLIMEX_PROTOBOARD__"],
	features: ["MEM", "SERIAL", "PINOUT", "SENSOR", "CIRCULAR"],
	drivers: ["LED", "BUTTON", "INA219"],
	pins: {
		power: "p1.7",
	},
	uarts: [
		{ name: "dst", gate: "USCI_A3", txd: "p10.4", rxd: "p10.5", baudRate: "9600" },
		{ name: "src", gate: "USCI_A0", txd: "p3.4", rxd: "p3.5", baudRate: "9600" },
	],
	pinout: {
		LED: {
			red: { port: "p10.6" },
			green: { port: "p10.7" },
			systemGreen: { port: "p2.5" },
		},
		BUTTON: {
			left: { port: "p1.6" },
			right: { port: "p1.5" },
			center: { port: "p1.4" },
			up: { port: "p1.3" },
			down: { port: "p1.2" },
		},
		INA219: {
			ina219: { gate: "USCI_B3", sda: "p10.1", clk: "p10.2" },
		},
	},
}

export function prepare(ctx) {
	ctx.useArch(board.arch)
	ctx.useSeries(board.series)
	ctx.useMcu(board.mcu)

	for (const define of board.defines)
		ctx.define(define)
	for (const feature of board.features)
		ctx.feature(feature)
	for (const driver of board.drivers)
		ctx.driver(driver)

	ctx.pins(board.pinout)
}
