export const phi = {
	kind: "module",
	name: "MODE",
	label: "Cipher mode API",
	description: "One API over every cipher mode in the build: mp_cipher_mode_init(&mode, \"CBC\", &cipher, iv, ivlen), mp_cipher_mode_update(), mp_cipher_mode_aad(), mp_cipher_mode_final(), mp_cipher_mode_free(). The mode wraps a cipher the caller already started with mp_cipher_init(), so the cipher and the mode are two orthogonal families: pick a cipher (AES), pick a mode (CBC, CTR, GCM). CBC carries PKCS#7 padding; CTR is a stream; GCM is AEAD with a 16-byte tag. Only the modes the build carries are found; pull them with requires.",
	define: "SUPPORT_COMMON_CRYPTO_MODE",
	requires: ["CIPHER"],
	sources: ["index.c"],
	headers: ["mode.h"],
	headerDir: "crypto",
}
