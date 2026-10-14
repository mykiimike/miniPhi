export const phi = {
	kind: "module",
	name: "HMAC",
	label: "HMAC",
	description: "HMAC (RFC 2104) over any hash of the build, picked by name through HASH: mp_hmac_init(&hmac, \"SHA256\", key, keylen), mp_hmac_update(), mp_hmac_final(). A key longer than the block is hashed first. The context is the caller's. When the series or the MCU declares an HMAC accelerator, the build adds its sources and MP_ARCH_HMAC, and a context runs on the hardware whenever the arch accepts that hash.",
	define: "SUPPORT_COMMON_CRYPTO_MAC_HMAC",
	sources: ["hmac.c"],
	headers: ["hmac.h"],
	headerDir: "crypto",
	requires: ["HASH"],
}
