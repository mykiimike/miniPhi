export const phi = {
	kind: "board",
	name: "ak-stm32-lkit",
	vendor: "Artekit",
	label: "Artekit AK-STM32-LKIT",
	description: "STM32F103T8U6 in a 36-pin package, with no 32 kHz crystal. The console goes through the ST3232. The USB B connector is fitted and the port is not driven. D1 and D2 are the red LEDs and sink. The reset key is NRST. Flashed with the AK-LINK.",
	specs: {
		crystal: "8 MHz",
		console: "USART1",
	},
	arch: "stm32",
	series: "f1",
	mcu: "stm32f103t8",
	toolchain: "gcc",
	requires: ["ak-link"],
	app: "sysinfo",
	defines: [],
	modules: ["SERIAL", "DEVICE"],
	drivers: ["LED"],
	settings: {
		/* 20 KB RAM: the pool, a small C heap and the 2 KB stack have to share it */
		MP_MEM_SIZE: 4096,
		MP_MEM_CHUNK: 64,
		MP_CLOCK_HSE_HZ: 8000000,
		HEAP_SIZE: 128,
	},
	/* the crystal is OSC_IN / OSC_OUT, not a GPIO; JTAG pins are reserved by the series */
	reserved: [
		{ pin: "pa11", who: "USB D-" },
		{ pin: "pa12", who: "USB D+" },
	],
	devices: [
		{ driver: "LED", name: "d1", options: { port: "pa0", reverse: true } },
		{ driver: "LED", name: "d2", options: { port: "pa1", reverse: true } },
		{ bus: "uart", name: "console", options: { gate: "USART1", txd: "pa9", rxd: "pa10", baudRate: "115200" } },
	],
}
