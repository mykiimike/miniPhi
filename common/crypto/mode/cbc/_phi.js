export const phi = {
	kind: "module",
	name: "CBC",
	label: "CBC",
	description: "Cipher block chaining mode, raw (no padding): the caller feeds block-aligned data and handles padding on top. Wraps a cipher the caller started with mp_cipher_init(); the direction (encrypt or decrypt) is set at mp_cipher_mode_init(). Constant time with respect to the key: the cipher does that, the mode only XORs and copies.",
	define: "SUPPORT_COMMON_CRYPTO_MODE_CBC",
	requires: ["MODE"],
	sources: ["cbc.c"],
	headerDir: "crypto",
	settings: [
		{ name: "MP_CBC_RAM", label: "RAM", type: "number", unit: "bytes", default: 64, min: 32, max: 512, ram: true },
	],
}
