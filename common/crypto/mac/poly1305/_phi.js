export const phi = {
	kind: "module",
	name: "POLY1305",
	label: "Poly1305",
	description: "Poly1305 one-time MAC (RFC 7539), 130-bit prime field, 16-byte tag. The 32-byte key (r || s) is one-time: a fresh key per message, never reused. No table and no S-box: five 26-bit limbs and 64-bit products, so it is constant time by construction. Used by the ChaCha20-Poly1305 AEAD which derives a fresh key per message. The caller owns the context, so nothing is allocated.",
	define: "SUPPORT_COMMON_CRYPTO_MAC_POLY1305",
	sources: ["poly1305.c"],
	headers: ["poly1305.h"],
	headerDir: "crypto",
	settings: [
		{
			name: "MP_POLY1305_RAM",
			label: "RAM",
			type: "number",
			unit: "bytes",
			default: 128,
			min: 64,
			max: 4096,
			ram: true,
		},
	],
}
