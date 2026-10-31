export const phi = {
	kind: "module",
	name: "SHA512",
	label: "SHA-512",
	group: ["crypto", "hash"],
	pqc: false,
	description: "SHA-512 digest (FIPS 180-4), one shot or streamed through a context the caller owns. SHA-384 uses this compression. Runs in software everywhere. When the series or the MCU declares a SHA512 accelerator, the build adds its sources and MP_ARCH_SHA512, and a context runs on the hardware whenever the arch accepts it.",
	specs: {
		standard: ["FIPS 180-4", "ECCG ACM v2"],
		security: "256 bit",
		lengths: { digest: "64", block: "128" },
		random: "none",
	},
	define: "SUPPORT_COMMON_CRYPTO_HASH_SHA512",
	sources: ["sha512.c"],
	headers: ["sha512.h"],
	headerDir: "crypto",
}
