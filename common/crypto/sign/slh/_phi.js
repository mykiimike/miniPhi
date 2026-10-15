export const phi = {
	kind: "module",
	name: "SLH",
	label: "SLH-DSA",
	description: "Stateless hash-based post-quantum signatures (FIPS 205), SLH-DSA-SHA2-128s (category 1, n=16). The hash family is SHA-2 and, at n=16, every tweakable hash call is SHA-256 (F, H, T_l, PRF), H_msg is MGF1-SHA-256 and PRF_msg is HMAC-SHA-256, so this requires SHA-256 and HMAC but not SHAKE or SHA-512. The private key is 64 bytes (SK.seed, SK.prf, PK.seed, PK.root), the public key is 32 bytes (PK.seed, PK.root) and the signature is 7856 bytes. The page only verifies a stored signature; signing is slow on this class of CPU. Constant time with respect to the secret: the seed only ever enters a SHA-2 input, never a branch or an index. One operation at a time.",
	define: "SUPPORT_COMMON_CRYPTO_SIGN_SLH",
	sources: ["slh.c"],
	headers: ["slh.h"],
	headerDir: "crypto",
	requires: ["SHA256", "HMAC"],
	settings: [
		/* peak RAM of a verify operation (FORS roots + one XMSS WOTS buffer) */
		{ name: "MP_SLH_RAM", label: "RAM", type: "number", unit: "bytes", default: 2048, min: 2048, max: 2048, ram: true },
	],
}
