export const arch = {
	name: "msp430",
	toolchain: "msp430-elf-",
	define: "__MSP430__",
	include: "include",
	sources: [
		"adc.c",
		"flash.c",
		"gate.c",
		"gpio.c",
		"i2c.c",
		"interrupt.c",
		"msp430.c",
		"rtc.c",
		"spi.c",
		"timer.c",
		"uart.c",
	],
	series: [
		{
			name: "f5xx_f6xx",
			boot: ["f5xx_f6xx/clock.c"],
			mcus: [
				{
					name: "msp430f5438",
					define: "__MSP430F5438__",
					ld: "ld/msp430f5438.cmd",
				},
				{
					name: "msp430f5438a",
					define: "__MSP430F5438A__",
					ld: "ld/msp430f5438a.cmd",
				},
			],
		},
	],
}

export function prepare(ctx) {
	ctx.requireTool(arch.toolchain + "gcc")
	ctx.define(arch.define)
	ctx.include("arch/msp430/" + arch.include)

	for (const src of arch.sources)
		ctx.source("arch/msp430/" + src)

	for (const series of arch.series) {
		for (const boot of series.boot)
			ctx.boot("arch/msp430/" + boot)
		for (const mcu of series.mcus)
			ctx.linker("arch/msp430/" + mcu.ld)
	}
}
