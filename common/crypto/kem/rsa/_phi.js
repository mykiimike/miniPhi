export const phi = {
	kind: "module",
	name: "RSAKEM",
	label: "RSA-KEM",
	group: ["crypto", "kem"],
	pqc: false,
	description: "RSA-KEM (RFC 5990) on the RSA key of this build. The public key and the private key are the RSA layouts. Encapsulation coins are one modulus of bytes, an integer z in [1, n). The ciphertext is z^e mod n. The shared secret is 32 bytes of SHAKE-256 over the big-endian z. The private operation is the CRT, and its time follows the length of the exponents, not their bits. Key generation is the RSA one and is slow on this class of CPU. Needs RSA and SHAKE. One operation at a time. mp_crypto_rsakem_decaps_fuse() is the fuse path, role MP_CRYPTO_FUSE_RSAKEM. Nothing here burns a fuse. This module is not linked beside LMS, SLH-DSA or ML-DSA, because those already exclude RSA.",
	specs: {
		standard: ["RFC 5990"],
		security: "128 bit",
		lengths: { ss: "32", seed: "32" },
		random: "coins",
	},
	define: "SUPPORT_COMMON_CRYPTO_KEM_RSAKEM",
	requires: ["RSA", "SHAKE"],
	sources: ["rsa.c"],
	headers: ["rsakem.h"],
	headerDir: "crypto",
}
