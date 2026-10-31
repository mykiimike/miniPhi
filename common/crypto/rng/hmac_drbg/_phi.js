export const phi = {
	kind: "module",
	name: "HMAC_DRBG",
	label: "HMAC-DRBG",
	description: "HMAC_DRBG (NIST SP 800-90A) over any hash of the build, through HMAC: mp_hmac_drbg_init(&drbg, \"SHA256\", entropy, entropylen, nonce, noncelen, personal, personlen), mp_hmac_drbg_generate(), mp_hmac_drbg_reseed(), mp_hmac_drbg_free(). A NULL entropy pointer seeds from the HSM when the series declares one. Passing entropy keeps that seed, which is how a deterministic draw is requested. NIST approves SHA-1, SHA-256, SHA-384 and SHA-512 for it; any other hash of the build runs the same construction. The context is the caller's.",
	define: "SUPPORT_COMMON_CRYPTO_RNG_HMAC_DRBG",
	sources: ["hmac_drbg.c"],
	headers: ["hmac_drbg.h"],
	headerDir: "crypto",
	requires: ["HMAC"],
}
