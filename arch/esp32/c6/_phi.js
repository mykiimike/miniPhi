function pads() {
	return Array.from({ length: 31 }, (_, pin) => "pa" + pin)
}

const gates = {
	USB: { kinds: ["uart"] },
	UART0: { kinds: ["uart"], pins: { uart: { txd: "pa16", rxd: "pa17" } } },
	SPI2: { kinds: ["spi"], pins: { spi: { simo: "pa18", somi: "pa20", clk: "pa19" } } },
	I2C0: { kinds: ["i2c"], pins: { i2c: { sda: "pa22", clk: "pa23" } } },
}

export const phi = {
	kind: "series",
	arch: "esp32",
	name: "c6",
	vendor: "Espressif",
	label: "ESP32-C6",
	tested: true,
	description: "The PLL stays up so the USB console does not drop. SHA-1, SHA-256, AES-128 and AES-256 run on the accelerators. The HSM reads the hardware RNG; HMAC-DRBG conditions it.",
	accelerators: {
		SHA1: { sources: ["../crypto/hash/sha.c"], headers: ["../crypto/hash/sha1.h"] },
		SHA256: { sources: ["../crypto/hash/sha.c"], headers: ["../crypto/hash/sha256.h"] },
		AES: { sources: ["../crypto/cipher/aes.c"], headers: ["../crypto/cipher/aes.h"] },
		HSM: { sources: ["../crypto/rng/hsm.c"], headers: [] },
	},
	specs: {
		core: "HP RV32IMAC",
		clock: "160 MHz",
		pll: "480 MHz",
		source: "40 MHz crystal",
		memory: "512 KB HP SRAM",
		tick: "1 kHz, 16 MHz SYSTIMER",
	},
	boot: ["startup.c", "clock.c", "esp32.c", "gate.c", "gpio.c", "interrupt.c", "uart.c"],
	headers: ["include/series.h"],
	headerDir: "arch",
	mcus: [
		{
			name: "esp32c6",
			label: "ESP32-C6",
			cpu: "rv32imac",
			define: "ESP32C6",
			ld: "ld/esp32c6.ld",
			package: "QFN40",
			flash: 4194304,
			ram: 446464,
			pins: pads(),
			irqLines: true,
			gates,
			reserved: [
				{ pin: "pa12", who: "USB D-" },
				{ pin: "pa13", who: "USB D+" },
				{ pin: "pa24", who: "flash CS" },
				{ pin: "pa25", who: "flash Q" },
				{ pin: "pa26", who: "flash WP" },
				{ pin: "pa27", who: "flash VDD" },
				{ pin: "pa28", who: "flash HD" },
				{ pin: "pa29", who: "flash CLK" },
				{ pin: "pa30", who: "flash D" },
			],
		},
	],
}
