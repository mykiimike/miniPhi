export const phi = {
	kind: "module",
	name: "HSM",
	label: "HSM",
	description: "Hardware entropy for a DRBG. The module is built only when the series or the MCU declares an HSM accelerator. mp_hsm_entropy() fills a buffer from that block. Drawing application random bytes goes through mp_rng_bytes().",
	needsAccelerator: true,
	define: "SUPPORT_COMMON_CRYPTO_RNG_HSM",
	sources: ["hsm.c"],
	headers: ["hsm.h"],
	headerDir: "crypto",
}
