const here = "common/spi/"

export const phi = {
	kind: "module",
	name: "SPI",
	label: "Shared SPI bus",
	description: "A synchronous SPI master on GPIO pins, mode 0, MSB first, about 100 kHz. One bus owns SCLK and MOSI. MISO is claimed only when a device names it. Chip select stays with the device. A driver opens the bus once and shares the handle.",
	define: "SUPPORT_COMMON_SPI",
	requires: ["DEVICE"],
	sources: ["spi.c"],
	headers: ["spi.h"],
	tests: ["test/bang.c"],
}
