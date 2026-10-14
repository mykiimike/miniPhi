export const phi = {
	kind: "module",
	name: "BLAKE2S",
	label: "BLAKE2s",
	description: "BLAKE2s-256 digest (RFC 7693), one shot or streamed through a context the caller owns. The 32-bit variant, suited to a Cortex-M. An optional key of up to 32 bytes turns it into a MAC. Runs in software everywhere. When the series or the MCU declares a BLAKE2S accelerator, the build adds its sources and MP_ARCH_BLAKE2S, and an unkeyed context runs on the hardware whenever the arch accepts it. A keyed digest stays in software.",
	define: "SUPPORT_COMMON_CRYPTO_HASH_BLAKE2S",
	sources: ["blake2s.c"],
	headers: ["blake2s.h"],
	headerDir: "crypto",
}
