const gate = (kind) => ({ name: "gate", label: "USCI", type: "gate", gateKind: kind, required: true })

export const phi = {
	kind: "arch",
	name: "msp430",
	label: "TI MSP430",
	description: "16-bit ultra-low-power microcontrollers.",
	define: "__MSP430__",
	headers: [
		"include/adc.h",
		"include/clock.h",
		"include/flash.h",
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
				gate("uart"),
				{ name: "txd", label: "TXD", type: "pin" },
				{ name: "rxd", label: "RXD", type: "pin" },
				{ name: "rts", label: "RTS (flow control)", type: "pin" },
				{ name: "cts", label: "CTS (flow control)", type: "pin", irq: true },
				{ name: "baudRate", label: "Baud rate", type: "enum", values: ["1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200"], default: "9600" },
			],
		},
		i2c: {
			label: "I2C",
			options: [
				gate("i2c"),
				{ name: "sda", label: "SDA", type: "pin", required: true },
				{ name: "clk", label: "SCL", type: "pin", required: true },
			],
		},
		spi: {
			label: "SPI",
			options: [
				gate("spi"),
				{ name: "simo", label: "SIMO", type: "pin", required: true },
				{ name: "somi", label: "SOMI", type: "pin", required: true },
				{ name: "clk", label: "CLK", type: "pin", required: true },
			],
		},
	},
	toolchains: [
		{
			name: "ti",
			label: "TI cl430",
			description: "The compiler the project was built with in Code Composer Studio 5.5. The flags below come from that project.",
			vars: [
				{ name: "CG_TOOL_ROOT", label: "Compiler root", default: "/opt/ti/ccsv5/tools/compiler/msp430_4.4.6", hint: "Contains bin/cl430" },
				{ name: "CCS_BASE_ROOT", label: "ccs_base", default: "/opt/ti/ccsv5/ccs_base", hint: "Contains msp430/include/msp430.h" },
			],
			settings: [
				{ name: "STACK_SIZE", label: "Stack", type: "number", unit: "bytes", default: 1024, min: 160, max: 8192, ram: true, emit: "make" },
				{ name: "HEAP_SIZE", label: "C heap", type: "number", unit: "bytes", default: 0, min: 0, max: 8192, ram: true, emit: "make" },
			],
			cc: "$(CG_TOOL_ROOT)/bin/cl430",
			object: ".obj",
			output: "miniphi.out",
			linkerScript: true,
			defineFlag: "--define={}",
			includeFlag: "--include_path={}",
			cflags: [
				"-vmspx",
				"--code_model=large",
				"--data_model=small",
				"--near_data=globals",
				"--use_hw_mpy=F5",
				"--abi=eabi",
				"-O2",
				"-g",
				"--printf_support=full",
				"--silicon_errata=CPU15",
				"--silicon_errata=CPU18",
				"--silicon_errata=CPU21",
				"--silicon_errata=CPU22",
				"--silicon_errata=CPU23",
				"--silicon_errata=CPU40",
				"--diag_warning=225",
				"--display_error_number",
				"--diag_wrap=off",
				"--include_path=\"$(CCS_BASE_ROOT)/msp430/include\"",
				"--include_path=\"$(CG_TOOL_ROOT)/include\"",
			],
			ldflags: [
				"-m miniphi.map",
				"--heap_size=$(HEAP_SIZE)",
				"--stack_size=$(STACK_SIZE)",
				"--cinit_hold_wdt=on",
				"--unused_section_elimination=on",
				"--rom_model",
				"--reread_libs",
				"-i\"$(CCS_BASE_ROOT)/msp430/include\"",
				"-i\"$(CCS_BASE_ROOT)/msp430/lib/5xx_6xx_FRxx\"",
				"-i\"$(CG_TOOL_ROOT)/lib\"",
				"-i\"$(CG_TOOL_ROOT)/include\"",
			],
			libs: ["-llibc.a", "-llibmath.a"],
			compile: "$(CC) $(CFLAGS) --compile_only --obj_directory=$(@D) $<",
			link: "$(CC) $(CFLAGS) -z $(LDFLAGS) -o $@ $(OBJS) $(LDSCRIPT) $(LIBS)",
		},
		{
			name: "gcc",
			label: "msp430-elf-gcc",
			description: "TI's open-source GCC. clock.c still uses #pragma DATA_SECTION and _system_pre_init, so this path is not proven.",
			experimental: true,
			vars: [
				{ name: "MSP430_GCC_ROOT", label: "GCC root", default: "/opt/ti/msp430-gcc", hint: "Contains bin/msp430-elf-gcc and include/<mcu>.ld" },
			],
			cc: "$(MSP430_GCC_ROOT)/bin/msp430-elf-gcc",
			object: ".o",
			output: "miniphi.elf",
			linkerScript: false,
			defineFlag: "-D{}",
			includeFlag: "-I{}",
			cflags: [
				"-mmcu=$(MCU)",
				"-mlarge",
				"-O2",
				"-g",
				"-ffunction-sections",
				"-fdata-sections",
				"-I$(MSP430_GCC_ROOT)/include",
			],
			ldflags: [
				"-mmcu=$(MCU)",
				"-mlarge",
				"-L$(MSP430_GCC_ROOT)/include",
				"-Wl,--gc-sections",
				"-Wl,-Map=miniphi.map",
			],
			libs: [],
			compile: "$(CC) $(CFLAGS) -c $< -o $@",
			link: "$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)",
		},
	],
}

export function prepare(ctx) {
	ctx.requireTool("make", "GNU make runs build/Makefile.")

	if (ctx.toolchain.name === "ti") {
		ctx.requireFile("$(CG_TOOL_ROOT)/bin/cl430", "Point CG_TOOL_ROOT at the MSP430 code generation tools.")
		ctx.requireFile("$(CCS_BASE_ROOT)/msp430/include/msp430.h", "Point CCS_BASE_ROOT at the ccs_base directory of Code Composer Studio.")
		ctx.requireFile("$(CCS_BASE_ROOT)/msp430/include/" + ctx.mcu.name + ".cmd", "The device memory map pulled in by the linker script.")
	}

	if (ctx.toolchain.name === "gcc") {
		ctx.requireFile("$(MSP430_GCC_ROOT)/bin/msp430-elf-gcc", "Install MSP430-GCC from https://www.ti.com/tool/MSP430-GCC-OPENSOURCE.")
		ctx.requireFile("$(MSP430_GCC_ROOT)/include/" + ctx.mcu.name + ".ld", "Install the MSP430 GCC support files into the same root.")
		ctx.warn("clock.c uses #pragma DATA_SECTION and _system_pre_init, which only the TI compiler understands.")
	}
}
