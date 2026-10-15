export const phi = {
	kind: "module",
	name: "BN",
	label: "Bignum",
	description: "Fixed-width unsigned arithmetic for the signature algorithms. MP_BN_MAX words of 32 bits: the RSA modulus when RSA is in the build, otherwise 512 bits. The scratch lives in the module, so one operation runs at a time: the kernel does not preempt, and an interrupt must not enter it. Not a crypto class of its own.",
	define: "SUPPORT_COMMON_CRYPTO_BN",
	sources: ["bn.c"],
	headers: ["bn.h"],
	headerDir: "crypto",
}
