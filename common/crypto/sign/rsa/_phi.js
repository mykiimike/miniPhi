export const phi = {
	kind: "module",
	name: "RSA",
	label: "RSA",
	description: "RSA signatures, PKCS#1 v1.5 with SHA-256. MP_RSA_BITS is the modulus, at least 3072 and a multiple of 64; the two primes are half of that and the exponent is 65537. The public key is the modulus and the exponent, the private key is the CRT form and the signature is the modulus size. Key generation expands a 32-byte seed with HMAC-DRBG and is slow on this class of CPU; signing uses the CRT. One operation at a time.",
	define: "SUPPORT_COMMON_CRYPTO_SIGN_RSA",
	sources: ["rsa.c"],
	headers: ["rsa.h"],
	headerDir: "crypto",
	requires: ["SHA256", "HMAC_DRBG", "BN"],
	settings: [
		{ name: "MP_RSA_BITS", label: "Modulus", type: "number", unit: "bits", default: 3072, min: 3072, max: 4096 },
	],
}
