export const phi = {
	kind: "module",
	name: "GCM",
	label: "GCM",
	description: "Galois/Counter Mode, an AEAD over a 128-bit block cipher. The IV is 12 bytes (J0 = IV || 0x00000001). The ciphertext is CTR (inc32) and the tag is GHASH over the AAD and ciphertext, encrypted with J0. Wraps a cipher the caller started with mp_cipher_init(); the direction is set at mp_cipher_mode_init(). Constant time with respect to the key: H is the only secret and it only enters the GF(2^128) multiply as the shifted operand; bit tests and the reduction use masks, not branches.",
	define: "SUPPORT_COMMON_CRYPTO_MODE_GCM",
	requires: ["MODE"],
	sources: ["gcm.c"],
	headerDir: "crypto",
	settings: [
		{ name: "MP_GCM_RAM", label: "RAM", type: "number", unit: "bytes", default: 256, min: 128, max: 2048, ram: true },
	],
}
