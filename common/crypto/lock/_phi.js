export const phi = {
	kind: "module",
	name: "LOCK",
	label: "Crypto lock",
	description: "One asymmetric operation at a time. The bignum scratch and the signature and KEM scratch stay in the modules; this lock is what admits a single caller. It is recursive, so a public operation may call another and RSA key generation may call the prime test. With more than one core it is a cpu mutex held at the kernel ceiling. Otherwise it is the task ceiling, which compiles to nothing under the cooperative and priority schedulers. An interrupt must not take it: the lock spins and it raises the task ceiling. Not a crypto class of its own.",
	define: "SUPPORT_COMMON_CRYPTO_LOCK",
	sources: ["lock.c"],
	headers: ["lock.h"],
	tests: ["test/lock.c", "test/smp.c"],
	headerDir: "crypto",
}
