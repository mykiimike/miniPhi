export const phi = {
	kind: "module",
	name: "SHAKE",
	label: "SHA-3 / SHAKE",
	description: "Keccak-f[1600] sponge: SHA3-256, SHA3-512, SHAKE-128 and SHAKE-256. One shot or streamed through a context the caller owns. Used by ML-DSA (FIPS 204); runs in software everywhere. Constant time: the permutation is a fixed function of the full state, so no branch or memory access depends on a secret.",
	define: "SUPPORT_COMMON_CRYPTO_HASH_SHAKE",
	sources: ["shake.c"],
	headers: ["shake.h"],
	headerDir: "crypto",
}
