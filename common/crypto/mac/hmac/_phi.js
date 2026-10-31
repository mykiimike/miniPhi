export const phi = {
	kind: "module",
	name: "HMAC",
	label: "HMAC",
	group: ["crypto", "mac"],
	pqc: false,
	description: "HMAC (RFC 2104) over any hash of the build, picked by name through HASH: mp_crypto_hmac_init(&hmac, \"SHA256\", key, keylen), mp_crypto_hmac_update(), mp_crypto_hmac_final(). A key longer than the block is hashed first. mp_crypto_hmac_init_fuse() takes an eFuse slot of role MP_CRYPTO_FUSE_HMAC and runs SHA-256 on that block. The context is the caller's. When the series or the MCU declares an HMAC accelerator, the build adds its sources and MP_ARCH_HMAC, and a context runs on the hardware whenever the arch accepts that hash.",
	specs: {
		standard: ["RFC 2104", "ECCG ACM v2"],
		random: "none",
	},
	define: "SUPPORT_COMMON_CRYPTO_MAC_HMAC",
	sources: ["hmac.c"],
	headers: ["hmac.h"],
	headerDir: "crypto",
	requires: ["HASH"],
}
