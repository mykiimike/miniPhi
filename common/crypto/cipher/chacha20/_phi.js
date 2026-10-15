export const phi = {
	kind: "module",
	name: "CHACHA20",
	label: "ChaCha20",
	description: "ChaCha20 stream cipher (RFC 8439), 256-bit key, 96-bit nonce, 20 rounds ARX. No table and no S-box: every step is adds, rotates and xors on 32-bit words, so it is constant time by construction, including the key schedule (there is none). The keystream is buffered so any length can be XORed through mp_chacha20_update_block(). The caller owns the context, so nothing is allocated.",
	define: "SUPPORT_COMMON_CRYPTO_CIPHER_CHACHA20",
	sources: ["chacha20.c"],
	headers: ["chacha20.h"],
	headerDir: "crypto",
	settings: [
		{
			name: "MP_CHACHA20_RAM",
			label: "RAM",
			type: "number",
			unit: "bytes",
			default: 256,
			min: 128,
			max: 4096,
			ram: true,
		},
	],
}
