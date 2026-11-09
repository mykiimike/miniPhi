const here = "common/i2c/"

export const phi = {
	kind: "module",
	name: "I2C",
	label: "Shared I2C bus",
	description: "A synchronous 100 kHz I2C master on two open-drain GPIO pins. One bus owns SDA and SCL and serves every device on the same pair of pins; the kernel is cooperative, so a flush and a read never overlap mid-byte. A driver that needs the bus opens it once and shares the handle.",
	define: "SUPPORT_COMMON_I2C",
	requires: [],
	sources: ["i2c.c"],
	headers: ["i2c.h"],
}
