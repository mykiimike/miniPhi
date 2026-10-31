export const phi = {
	kind: "module",
	name: "ED25519",
	label: "Ed25519",
	group: ["crypto", "sign"],
	pqc: false,
	description: "Ed25519 signatures (RFC 8032). A private key is 32 random bytes, the public key and the signature are 32 and 64. Signing is deterministic: SHA-512, which this requires, makes the nonce, so a signature does not draw from the RNG. The arithmetic scratch is shared, one operation at a time. mp_crypto_ed25519_sign_fuse() is the fuse path, role MP_CRYPTO_FUSE_ED25519: a series accelerator supplies mp_arch_ed25519_sign_fuse(), and the wrapped blob is that arch's.",
	specs: {
		standard: ["RFC 8032"],
		security: "128 bit",
		lengths: { priv: "32", pub: "32", sig: "64", seed: "32" },
		random: "none",
	},
	define: "SUPPORT_COMMON_CRYPTO_SIGN_ED25519",
	sources: ["ed25519.c"],
	headers: ["ed25519.h"],
	headerDir: "crypto",
	requires: ["SHA512", "BN"],
}
