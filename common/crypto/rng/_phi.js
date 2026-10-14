export const phi = {
	kind: "module",
	name: "RNG",
	label: "RNG",
	description: "The running generator. mp_rng_start() is called from the kernel once tasks exist, and a task reseeds HMAC-DRBG after MP_RNG_RESEED draws, or on the first draw once the seed is MP_RNG_PERIOD ms old; with nobody drawing it sleeps. The noise is the low byte of the die temperature and the voltage reference when the arch has them; until a sample arrives, mp_rng_bytes() returns FALSE. Draw with mp_rng_bytes().",
	define: "SUPPORT_COMMON_CRYPTO_RNG",
	sources: ["index.c"],
	headers: ["rng.h"],
	headerDir: "crypto",
	requires: ["HMAC_DRBG"],
	settings: [
		{ name: "MP_RNG_PERIOD", label: "Oldest seed drawn from", type: "number", unit: "ms", default: 60000, min: 100, max: 3600000 },
		{ name: "MP_RNG_RESEED", label: "Draws between reseeds", type: "number", default: 64, min: 1, max: 100000 },
	],
}
