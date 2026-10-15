export const phi = {
	kind: "app",
	name: "sysinfo",
	label: "System info",
	description: "Prints the miniPhi banner, the version, the build and the CPU on a UART console, then reports uptime, tasks and sensors. Blinks SOS on the first LED and HELP on the second when the board has them. It names no driver: LEDs, display, touch panels and accelerometers are the board devices found through DEVICE. On the first display the TUI home lists Accelerometer, Cryptography, Power, then System. Cryptography lists the classes in this firmware (hash, mac, cipher, kdf, rng, sign). A class lists its algorithms, and opening one says whether the chip accelerates it; HMAC and HMAC-DRBG also list the hashes they run over. A signature shows its key and signature sizes and checks itself on \"miniPhi\": Ed25519 and ECDSA P-256 sign and verify from a fixed seed, RSA only verifies a stored signature of a 3072-bit key, because generating a key is too slow for the screen. The modulus is MP_RSA_BITS, at least 3072. RNG names the generator in use, shows a fresh random number every 500 ms, then lists HMAC-DRBG; a task reseeds it from the ADC. System is a scrollable page with about, version, copyright, license, the flash (size, image, blank pages and the controller), uptime, RAM and the MCU sensors, and opens a Task manager that lists the kernel tasks and a Devices page that lists the board devices by class, name and state. Accelerometer shows the 3-axis sensor that is present, with a bubble level, and a switch that starts or stops the accelerometers; with a screen they only run while that page is on and lit. With KFLASH the switch is the key sysinfo.accelerometers and survives a reset. Power starts with a Governor switch (off pins the level at High), then the level and its clock, a table of the load, the wake ups, the level changes, the deep sleeps and the supply sensors, the holds, and a table of the time at each level. The Task manager also counts the wake ups of each task per second, and the console report has a power line.",
	sources: ["main.c", "screen.c", "morse.c", "pages/index.c", "pages/sensor.c", "pages/accelerometer/index.c", "pages/cryptography/index.c", "pages/cryptography/vectors.c", "pages/power/index.c", "pages/system/index.c", "pages/system/tasks.c", "pages/system/devices.c"],
	requires: ["SERIAL", "SENSOR", "DEVICE", "BLAKE2S", "SHA1", "SHA256", "SHA384", "SHA512", "HMAC", "HMAC_DRBG", "RNG", "ED25519", "ECDSA_P256", "SIGN", "CIPHER", "AES", "MODE", "CBC", "CTR", "GCM", "CHACHA20", "MAC", "POLY1305", "CHACHA20_POLY1305", "CCM"],
	devices: [
		{ bus: "uart", name: "console", label: "Console" },
	],
	settings: [
		{ name: "SYSINFO_LINE_DELAY", label: "Delay between boot lines", type: "number", unit: "ms", default: 60, min: 10, max: 1000 },
		{ name: "SYSINFO_PERIOD", label: "Report period", type: "number", unit: "ms", default: 5000, min: 100, max: 60000 },
		{ name: "SYSINFO_MORSE_UNIT", label: "Morse unit on the LEDs", type: "number", unit: "ms", default: 200, min: 50, max: 2000 },
	],
}
