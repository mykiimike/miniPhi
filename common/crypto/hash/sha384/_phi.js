export const phi = {
	kind: "module",
	name: "SHA384",
	label: "SHA-384",
	description: "SHA-384 digest (FIPS 180-4). It is SHA-512 with a different initial value and a 48-byte result, so it requires SHA512 and does not carry its own compression. Runs in software everywhere. When the series or the MCU declares a SHA384 accelerator, the build adds its sources and MP_ARCH_SHA384, and a context runs on the hardware whenever the arch accepts it.",
	define: "SUPPORT_COMMON_CRYPTO_HASH_SHA384",
	requires: ["SHA512"],
	sources: ["sha384.c"],
	headers: ["sha384.h"],
	headerDir: "crypto",
}
