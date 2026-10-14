export const phi = {
	kind: "module",
	name: "MAC",
	label: "MAC API",
	description: "One API over every MAC in the build: mp_mac_init(&mac, \"HMAC-SHA256\", key, keylen), mp_mac_update(), mp_mac_final(), mp_mac_free(), and mp_mac_equal() to check a tag in constant time. HMAC-<hash> takes any hash of the build through HMAC; BLAKE2S is the keyed BLAKE2s. The name is matched without case. Only what the build carries is found; pull it with requires. The context is the caller's and holds the largest of them, so nothing is allocated.",
	define: "SUPPORT_COMMON_CRYPTO_MAC",
	sources: ["index.c"],
	headers: ["mac.h"],
	headerDir: "crypto",
}
