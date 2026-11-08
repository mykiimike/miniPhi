export const phi = {
	kind: "module",
	name: "ESP32_RF",
	label: "ESP32 radio",
	description: "Shared host, open, analog I2C and the modem-domain refcount, plus the series calibration. Built only when the series declares an ESP32_RF accelerator. Boot does not open the radio.",
	needsAccelerator: true,
	define: "SUPPORT_ESP32_RF",
	sources: ["host.c", "i2c.c", "rf.c", "../modem/modem.c"],
	tests: ["../modem/test/modem.c"],
}
