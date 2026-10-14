export const phi = {
	kind: "module",
	name: "SHA256",
	label: "SHA-256",
	description: "SHA-256 digest (FIPS 180-4), one shot or streamed through a context the caller owns. Runs in software everywhere. When the series or the MCU declares a SHA256 accelerator, the build adds its sources and MP_ARCH_SHA256, and a context runs on the hardware whenever the arch accepts it.",
	define: "SUPPORT_COMMON_CRYPTO_HASH_SHA256",
	sources: ["sha256.c"],
	headers: ["sha256.h"],
	headerDir: "crypto",
}
