export const phi = {
	kind: "module",
	name: "LMS",
	label: "LMS",
	group: ["crypto", "sign"],
	pqc: true,
	description: "Stateful hash-based signatures (RFC 8554), LMS_SHA256_M32_H5 with LMOTS_SHA256_N32_W8. Post-quantum. The private key is a 16-byte identifier I and a 32-byte seed; the LMOTS secrets x[i] are derived from them by a PRF, so the whole tree is rebuilt on demand. The public key is the Merkle root (56 bytes) and the signature is 1292 bytes. Signing is stateful: the leaf counter q must advance and never repeat, so the page only verifies a stored signature. The hash is SHA-256, which this requires. Constant time with respect to the secret: the secret only ever enters a hash input, never a branch or an index. One operation at a time. mp_crypto_lms_sign_fuse() is the fuse path, role MP_CRYPTO_FUSE_LMS: a series accelerator supplies mp_arch_lms_sign_fuse(), and the wrapped blob is that arch's.",
	specs: {
		standard: ["RFC 8554", "ECCG ACM v2"],
		security: "128 bit",
		lengths: { priv: "52", pub: "56", sig: "1292", seed: "32" },
		random: "none",
	},
	define: "SUPPORT_COMMON_CRYPTO_SIGN_LMS",
	sources: ["lms.c"],
	headers: ["lms.h"],
	headerDir: "crypto",
	requires: ["SHA256"],
	settings: [
		/* peak RAM of a verify operation: the tree is rebuilt on the stack */
		{ name: "MP_CRYPTO_LMS_RAM", label: "RAM", type: "number", unit: "bytes", default: 3072, min: 3072, max: 3072, ram: true },
	],
}
