export const phi = {
	kind: "module",
	name: "SHA1",
	label: "SHA-1",
	description: "SHA-1 digest (FIPS 180-4), one shot or streamed through a context the caller owns. Broken for signatures and certificates; keep it for protocols that still name it. Runs in software everywhere. When the series or the MCU declares a SHA1 accelerator, the build adds its sources and MP_ARCH_SHA1, and a context runs on the hardware whenever the arch accepts it.",
	define: "SUPPORT_COMMON_CRYPTO_HASH_SHA1",
	sources: ["sha1.c"],
	headers: ["sha1.h"],
	headerDir: "crypto",
}
