const gate = (kind, label) => ({ name: "gate", label, type: "gate", gateKind: kind, required: true })

export const phi = {
	kind: "arch",
	name: "stm32",
	vendor: "STMicroelectronics",
	label: "ST STM32",
	description: "32-bit Cortex-M microcontrollers. The peripheral code follows the F1 register layout (GPIO CRL/CRH, I2C v1, ADC with calibration).",
	define: "__STM32__",
	headers: [
		"include/adc.h",
		"include/clock.h",
		"include/dma.h",
		"include/flash.h",
		"include/fsmc.h",
		"include/gate.h",
		"include/gpio.h",
		"include/i2c.h",
		"include/internal.h",
		"include/interrupt.h",
		"include/spi.h",
		"include/timer.h",
		"include/uart.h",
	],
	headerDir: "arch",
	sources: [
		"adc.c",
		"dma.c",
		"flash.c",
		"fsmc.c",
		"gate.c",
		"gpio.c",
		"i2c.c",
		"interrupt.c",
		"spi.c",
		"stm32.c",
		"syscalls.c",
		"timer.c",
		"uart.c",
		"usb.c",
	],
	buses: {
		gpio: {
			label: "GPIO",
			options: [
				{ name: "port", label: "Pin", type: "pin", required: true },
			],
		},
		uart: {
			label: "UART",
			options: [
				gate("uart", "USART"),
				{ name: "txd", label: "TX", type: "pin" },
				{ name: "rxd", label: "RX", type: "pin" },
				{ name: "rts", label: "RTS (flow control, any pin)", type: "pin" },
				{ name: "cts", label: "CTS (flow control)", type: "pin", irq: true },
				{ name: "baudRate", label: "Baud rate", type: "enum", values: ["1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200", "230400", "460800"], default: "115200" },
			],
		},
		i2c: {
			label: "I2C",
			options: [
				gate("i2c", "I2C"),
				{ name: "sda", label: "SDA", type: "pin", required: true },
				{ name: "clk", label: "SCL", type: "pin", required: true },
			],
		},
		spi: {
			label: "SPI",
			options: [
				gate("spi", "SPI"),
				{ name: "simo", label: "MOSI", type: "pin", required: true },
				{ name: "somi", label: "MISO", type: "pin", required: true },
				{ name: "clk", label: "SCK", type: "pin", required: true },
			],
		},
		fsmc: {
			label: "Parallel LCD (FSMC)",
			options: [
				gate("fsmc", "FSMC"),
				{ name: "cs", label: "Chip select (NE1 PD7, NE2 PG9, NE3 PG10, NE4 PG12)", type: "pin", required: true },
				{ name: "rs", label: "RS (an address line: A0 PF0, A16 PD11, A19 PE3...)", type: "pin", required: true },
				{ name: "writeCycle", label: "Write cycle", type: "number", unit: "ns", default: 100, min: 30, max: 3000 },
				{ name: "readCycle", label: "Read cycle", type: "number", unit: "ns", default: 500, min: 30, max: 3000 },
			],
		},
	},
	toolchains: [
		{
			name: "gcc",
			label: "arm-none-eabi-gcc",
			description: "GNU Arm Embedded with newlib-nano. Debian and Ubuntu ship it as gcc-arm-none-eabi.",
			vars: [
				{ name: "ARM_GCC_ROOT", label: "GCC root", default: "/usr", hint: "Contains bin/arm-none-eabi-gcc" },
			],
			settings: [
				{ name: "STACK_SIZE", label: "Stack", type: "number", unit: "bytes", default: 2048, min: 512, max: 16384, ram: true, emit: "make" },
				{ name: "HEAP_SIZE", label: "C heap", type: "number", unit: "bytes", default: 1024, min: 0, max: 16384, ram: true, emit: "make" },
			],
			cc: "$(ARM_GCC_ROOT)/bin/arm-none-eabi-gcc",
			object: ".o",
			output: "miniphi.elf",
			linkerScript: true,
			defineFlag: "-D{}",
			includeFlag: "-I{}",
			cflags: [
				"-mcpu=$(CPU)",
				"-mthumb",
				"-O2",
				"-g",
				"-std=gnu99",
				"-ffunction-sections",
				"-fdata-sections",
				"-fno-common",
				"-MMD",
				"-MP",
			],
			ldflags: [
				"-mcpu=$(CPU)",
				"-mthumb",
				"-nostartfiles",
				"--specs=nano.specs",
				"-u _printf_float",
				"-T$(LDSCRIPT)",
				"-Wl,--defsym=__stack_size=$(STACK_SIZE)",
				"-Wl,--defsym=__heap_size=$(HEAP_SIZE)",
				"-Wl,--gc-sections",
				"-Wl,-Map=miniphi.map",
			],
			libs: ["-lm"],
			compile: "$(CC) $(CFLAGS) -c $< -o $@",
			link: "$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS) && $(ARM_GCC_ROOT)/bin/arm-none-eabi-size $@",
		},
	],
}

export function prepare(ctx) {
	ctx.requireTool("make", "GNU make runs build/Makefile.")
	ctx.requireFile("$(ARM_GCC_ROOT)/bin/arm-none-eabi-gcc", "Install gcc-arm-none-eabi, or point ARM_GCC_ROOT at a GNU Arm Embedded toolchain.")
	ctx.requireFile("$(ARM_GCC_ROOT)/bin/arm-none-eabi-size", "arm-none-eabi-size prints the flash and RAM use after the link.")
}
