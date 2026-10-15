export const phi = {
	kind: "module",
	name: "CTR",
	label: "CTR",
	description: "Counter mode, a stream over a block cipher. The IV is the initial 128-bit counter, incremented big-endian. Encryption and decryption are the same operation. Wraps a cipher the caller started with mp_cipher_init(). Constant time with respect to the key: the cipher does that, the mode only XORs, copies and increments.",
	define: "SUPPORT_COMMON_CRYPTO_MODE_CTR",
	requires: ["MODE"],
	sources: ["ctr.c"],
	headerDir: "crypto",
	settings: [
		{ name: "MP_CTR_RAM", label: "RAM", type: "number", unit: "bytes", default: 64, min: 32, max: 512, ram: true },
	],
}
