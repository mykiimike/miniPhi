export const phi = {
	kind: "module",
	name: "CHACHA20_POLY1305",
	label: "ChaCha20-Poly1305",
	description: "ChaCha20-Poly1305 AEAD (RFC 8439), one-shot: 256-bit key, 96-bit nonce, 16-byte tag. The one-time Poly1305 key is the first 32 bytes of the ChaCha20 block 0 keystream, the message is encrypted from block 1, and the tag is Poly1305 over aad || pad || ct || pad || le64(aad) || le64(ct). The tag is verified before any plaintext is released. Constant time: the tag compare reads every byte. The caller owns the buffers, so nothing is allocated.",
	define: "SUPPORT_COMMON_CRYPTO_MODE_CHACHA20_POLY1305",
	sources: ["chacha20_poly1305.c"],
	headers: ["chacha20_poly1305.h"],
	headerDir: "crypto",
	requires: ["CHACHA20", "POLY1305", "MAC"],
	settings: [
		{
			name: "MP_CHACHA20_POLY1305_RAM",
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
