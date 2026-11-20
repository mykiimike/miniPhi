const gate = (kind, label) => ({ name: "gate", label, type: "gate", gateKind: kind, required: true })

export const phi = {
	kind: "arch",
	name: "rp2",
	vendor: "Raspberry Pi",
	label: "Raspberry Pi RP2",
	description: "Raspberry Pi RP2 family (RP2350, and later RP2040). Cortex-M33 port; Hazard3 is not started. The peripheral code follows the RP2350 address map (SIO, IO_BANK0, PL011 UART, DW SPI and I2C). clk_usb is PLL_USB at 48 MHz. clk_sys is the crystal at 12 MHz, PLL_USB at 48 MHz, or PLL_SYS at 150 MHz; the governor picks, and an enumerated USB console stays at or above 48 MHz. usb.c enumerates that controller as a CDC console. PWM slices are clocked by clk_sys. A pin selects its slice, so a backlight duty does not name a timer. TIMER0 remains the microsecond counter.",
	define: "__RP2__",
	/* PendSV sits under SysTick, so two tasks of one priority can share a tick. */
	taskShare: true,
	headers: [
		"include/adc.h",
		"include/clock.h",
		"include/dma.h",
		"include/flash.h",
		"include/gate.h",
		"include/gpio.h",
		"include/i2c.h",
		"include/internal.h",
		"include/interrupt.h",
		"include/pio.h",
		"include/rp2350.h",
		"include/spi.h",
		"include/timer.h",
		"include/uart.h",
		"include/usb.h",
	],
	headerDir: "arch",
	sources: [
		"adc.c",
		"cpu.c",
		"dma.c",
		"flash.c",
		"gate.c",
		"gpio.c",
		"i2c.c",
		"interrupt.c",
		"rp2.c",
		"share.c",
		"spi.c",
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
				gate("uart", "UART"),
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
	},
	toolchains: [
		{
			name: "gcc",
			label: "arm-none-eabi-gcc",
			description: "GNU Arm Embedded with newlib-nano. Debian and Ubuntu ship it as gcc-arm-none-eabi. The FPU is the single-precision FPv5 of the Cortex-M33, hard-float.",
			vars: [
				{ name: "ARM_GCC_ROOT", label: "GCC root", default: "/usr", hint: "Contains bin/arm-none-eabi-gcc" },
			],
			settings: [
				{ name: "STACK_SIZE", label: "Stack", type: "number", unit: "bytes", default: 8192, min: 512, max: 65536, ram: true, emit: "make" },
				{ name: "HEAP_SIZE", label: "C heap", type: "number", unit: "bytes", default: 8192, min: 0, max: 65536, ram: true, emit: "make" },
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
				"-mfpu=fpv5-sp-d16",
				"-mfloat-abi=hard",
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
				"-mfpu=fpv5-sp-d16",
				"-mfloat-abi=hard",
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
