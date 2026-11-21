export const phi = {
	kind: "module",
	name: "BN",
	label: "Bignum",
	description: "Fixed-width unsigned arithmetic for the signature algorithms. MP_CRYPTO_BN_MAX words of 32 bits: the RSA modulus when RSA is in the build, otherwise 512 bits. The scratch lives in the module. One operation at a time is mp_crypto_lock(); an interrupt must not enter, because the lock spins and raises the task ceiling. Not a crypto class of its own.",
	define: "SUPPORT_COMMON_CRYPTO_BN",
	requires: ["LOCK"],
	sources: ["bn.c"],
	headers: ["bn.h"],
	tests: ["test/bn.c"],
	headerDir: "crypto",
}
