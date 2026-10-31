export const phi = {
	kind: "module",
	name: "X25519",
	label: "X25519",
	description: "DHKEM(X25519, HKDF-SHA256) from RFC 9180, with the Montgomery ladder of RFC 7748. A seed is the 32-byte private key. Encapsulation coins are the 32-byte ephemeral secret. The ciphertext and the shared secret are 32 bytes each. The ladder always runs 255 steps and swaps with a mask, so the scalar never becomes a branch or a table index. Needs SHA-256 and HMAC. One operation at a time. mp_crypto_x25519_decaps_fuse() is the fuse path, role MP_CRYPTO_FUSE_X25519. Nothing here burns a fuse.",
	define: "SUPPORT_COMMON_CRYPTO_KEM_X25519",
	requires: ["SHA256", "HMAC"],
	sources: ["x25519.c"],
	headers: ["x25519.h"],
	headerDir: "crypto",
}
