export const phi = {
	kind: "module",
	name: "SHA512",
	label: "SHA-512",
	description: "SHA-512 digest (FIPS 180-4), one shot or streamed through a context the caller owns. SHA-384 uses this compression. Runs in software everywhere. When the series or the MCU declares a SHA512 accelerator, the build adds its sources and MP_ARCH_SHA512, and a context runs on the hardware whenever the arch accepts it.",
	define: "SUPPORT_COMMON_CRYPTO_HASH_SHA512",
	sources: ["sha512.c"],
	headers: ["sha512.h"],
	headerDir: "crypto",
}
