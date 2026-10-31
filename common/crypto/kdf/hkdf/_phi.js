export const phi = {
	kind: "module",
	name: "HKDF",
	label: "HKDF",
	group: ["crypto", "kdf"],
	pqc: false,
	description: "HKDF (RFC 5869) over any hash of the build, picked by name: mp_crypto_hkdf(\"SHA512\", salt, saltlen, ikm, ikmlen, info, infolen, out, L). An empty salt is HashLen zero bytes. Extract and expand are also separate calls, which is what DHKEM uses. The output length is at most 255 digests. Needs HASH and HMAC. The caller owns every buffer.",
	specs: {
		standard: ["RFC 5869", "ECCG ACM v2"],
		random: "none",
	},
	define: "SUPPORT_COMMON_CRYPTO_KDF_HKDF",
	requires: ["HASH", "HMAC"],
	sources: ["hkdf.c"],
	headers: ["hkdf.h"],
	headerDir: "crypto",
}
