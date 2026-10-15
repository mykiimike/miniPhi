export const phi = {
	kind: "module",
	name: "CCM",
	label: "AES-CCM",
	description: "AES-CCM AEAD (RFC 3610 / NIST SP 800-38C), one-shot: CBC-MAC for the tag, CTR for the ciphertext, the tag XORed with AES(CTR_0). Only the AES forward cipher is used, so it reuses the AES context the caller already started. Nonce 7..13 bytes, tag 4,6,8,10,12,14 or 16 bytes. The tag is verified before any plaintext is released. Constant time: the tag compare reads every byte. The caller owns the buffers, so nothing is allocated.",
	define: "SUPPORT_COMMON_CRYPTO_MODE_CCM",
	sources: ["ccm.c"],
	headers: ["ccm.h"],
	headerDir: "crypto",
	requires: ["CIPHER", "AES", "MAC"],
	settings: [
		{
			name: "MP_CCM_RAM",
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
