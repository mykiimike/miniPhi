export const phi = {
	kind: "module",
	name: "KDF",
	label: "KDF API",
	description: "One API over every key derivation of the build: mp_crypto_kdf(\"HKDF-SHA512\", salt, saltlen, ikm, ikmlen, info, infolen, out, L). The name is matched without case. HKDF- and a hash of the build selects HKDF. Only what the build carries is found. The directory is common/crypto/kdf, three path components, so it is not a class on the Cryptography page.",
	define: "SUPPORT_COMMON_CRYPTO_KDF",
	sources: ["index.c"],
	headers: ["kdf.h"],
	headerDir: "crypto",
}
