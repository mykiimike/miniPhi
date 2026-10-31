export const phi = {
	kind: "module",
	name: "SHAKE",
	label: "SHA-3 / SHAKE",
	group: ["crypto", "hash"],
	pqc: false,
	description: "Keccak-f[1600] sponge: SHA3-256, SHA3-512, SHAKE-128 and SHAKE-256. One shot or streamed through a context the caller owns. Used by ML-DSA (FIPS 204); runs in software everywhere. Constant time: the permutation is a fixed function of the full state, so no branch or memory access depends on a secret.",
	specs: {
		standard: ["FIPS 202", "ECCG ACM v2"],
		security: "128 / 256 bit",
		random: "none",
	},
	define: "SUPPORT_COMMON_CRYPTO_HASH_SHAKE",
	sources: ["shake.c"],
	headers: ["shake.h"],
	headerDir: "crypto",
}
