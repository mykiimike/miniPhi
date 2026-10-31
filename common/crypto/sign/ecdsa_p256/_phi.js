export const phi = {
	kind: "module",
	name: "ECDSA_P256",
	label: "ECDSA P-256",
	group: ["crypto", "sign"],
	pqc: false,
	description: "ECDSA on NIST P-256 with SHA-256. A private key is 32 bytes, the public key is x and y (64 bytes, no 0x04 prefix) and the signature is r and s, 32 bytes each. The nonce comes from RFC 6979, so signing does not draw from the RNG. One operation at a time. mp_crypto_ecdsa_p256_sign_fuse() is the fuse path, role MP_CRYPTO_FUSE_ECDSA_P256: a series accelerator supplies mp_arch_ecdsa_p256_sign_fuse(), and the wrapped blob is that arch's.",
	specs: {
		standard: ["FIPS 186-5", "RFC 6979", "ECCG ACM v2"],
		security: "128 bit",
		lengths: { priv: "32", pub: "64", sig: "64", seed: "32" },
		random: "none",
	},
	define: "SUPPORT_COMMON_CRYPTO_SIGN_ECDSA_P256",
	sources: ["ecdsa_p256.c"],
	headers: ["ecdsa_p256.h"],
	headerDir: "crypto",
	requires: ["SHA256", "HMAC", "BN"],
}
