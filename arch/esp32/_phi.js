const gate = (kind, label) => ({ name: "gate", label, type: "gate", gateKind: kind, required: true })

export const phi = {
	kind: "arch",
	name: "esp32",
	vendor: "Espressif",
	label: "Espressif ESP32",
	description: "Espressif ESP32 microcontrollers. The C6 series is the HP RV32IMAC core: GPIO, a 1 kHz SYSTIMER tick, and the USB Serial/JTAG console the ROM already enumerated. Wi-Fi, BLE, 802.15.4 and the low-power core are not in this port.",
	define: "__ESP32__",
	headers: [
		"include/clock.h",
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
		"esp32.c",
		"gate.c",
		"gpio.c",
		"i2c.c",
		"interrupt.c",
		"spi.c",
		"syscalls.c",
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
				gate("uart", "UART"),
				{ name: "txd", label: "TX", type: "pin" },
				{ name: "rxd", label: "RX", type: "pin" },
				{ name: "baudRate", label: "Baud rate", type: "enum", values: ["9600", "19200", "38400", "57600", "115200", "230400", "460800"], default: "115200" },
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
			label: "riscv32-esp-elf-gcc",
			description: "Espressif GNU toolchain for the HP core, rv32imac, ILP32, newlib. RISCV_GCC_ROOT is the directory that contains bin/riscv32-esp-elf-gcc.",
			vars: [
				{ name: "RISCV_GCC_ROOT", label: "GCC root", default: "/home/mykii/.local/riscv32-esp-elf", hint: "Contains bin/riscv32-esp-elf-gcc" },
			],
			settings: [
				{ name: "STACK_SIZE", label: "Stack", type: "number", unit: "bytes", default: 8192, min: 2048, max: 65536, ram: true, emit: "make" },
				{ name: "HEAP_SIZE", label: "C heap", type: "number", unit: "bytes", default: 8192, min: 0, max: 65536, ram: true, emit: "make" },
			],
			cc: "$(RISCV_GCC_ROOT)/bin/riscv32-esp-elf-gcc",
			object: ".o",
			output: "miniphi.elf",
			linkerScript: true,
			defineFlag: "-D{}",
			includeFlag: "-I{}",
			cflags: [
				"-march=rv32imac_zicsr_zifencei",
				"-mabi=ilp32",
				"-mcmodel=medany",
				"-mno-relax",
				"-O2",
				"-g",
				"-std=gnu99",
				"-ffunction-sections",
				"-fdata-sections",
				"-fno-common",
				"-fno-pie",
				"-fno-asynchronous-unwind-tables",
				"-MMD",
				"-MP",
			],
			ldflags: [
				"-march=rv32imac_zicsr_zifencei",
				"-mabi=ilp32",
				"-mcmodel=medany",
				"-mno-relax",
				"-nostartfiles",
				"-no-pie",
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
			link: "$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LIBS) && $(RISCV_GCC_ROOT)/bin/riscv32-esp-elf-size $@",
		},
	],
}

export function prepare(ctx) {
	ctx.requireTool("make", "GNU make runs the generated Makefile.")
	ctx.requireFile("$(RISCV_GCC_ROOT)/bin/riscv32-esp-elf-gcc", "Install the Espressif riscv32-esp-elf toolchain, or point RISCV_GCC_ROOT at it.")
	ctx.requireFile("$(RISCV_GCC_ROOT)/bin/riscv32-esp-elf-size", "riscv32-esp-elf-size prints the flash and RAM use after the link.")
}
