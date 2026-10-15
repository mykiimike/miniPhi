export const phi = {
	kind: "module",
	name: "CIPHER",
	label: "Cipher API",
	description: "One API over every cipher in the build: mp_cipher_init(&cipher, \"AES\", key, keylen), mp_cipher_update_block(), mp_cipher_free(). The name is matched without case, as the module name (AES) or the label (AES). Only the algorithms the build carries are found; pull them with requires. The caller owns the context, so nothing is allocated. AES sets the buffer sizes: MP_CIPHER_BLOCK, MP_CIPHER_KEY and MP_CIPHER_KEY_MAX.",
	define: "SUPPORT_COMMON_CRYPTO_CIPHER",
	sources: ["index.c"],
	headers: ["cipher.h"],
	headerDir: "crypto",
}
