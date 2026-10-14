export const phi = {
	kind: "module",
	name: "HASH",
	label: "Hash API",
	description: "One API over every hash in the build: mp_hash_init(&hash, \"SHA256\"), mp_hash_update(), mp_hash_digest(), mp_hash_free(). The name is matched without case, as the module name (SHA256) or the label (SHA-256). Only the algorithms the build carries are found; pull them with requires. The context is the caller's and holds the largest of them, so nothing is allocated.",
	define: "SUPPORT_COMMON_CRYPTO_HASH",
	sources: ["index.c"],
	headers: ["hash.h"],
	headerDir: "crypto",
}
