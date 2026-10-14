export const phi = {
	kind: "module",
	name: "RNG",
	label: "RNG",
	description: "The running generator. mp_rng_start() is called from the kernel once tasks exist, and a task reseeds HMAC-DRBG every MP_RNG_PERIOD ms. The noise is the low byte of the die temperature and the voltage reference when the arch has them; until a sample arrives, mp_rng_bytes() returns FALSE. Draw with mp_rng_bytes().",
	define: "SUPPORT_COMMON_CRYPTO_RNG",
	sources: ["index.c"],
	headers: ["rng.h"],
	headerDir: "crypto",
	requires: ["HMAC_DRBG"],
	settings: [
		{ name: "MP_RNG_PERIOD", label: "Reseed period", type: "number", unit: "ms", default: 1000, min: 100, max: 60000 },
	],
}
