export const phi = {
	kind: "module",
	name: "SIGN",
	label: "Signature API",
	description: "One API over every signature in the build: mp_crypto_sign_keypair(name, seed, seedlen, priv, pub), mp_crypto_sign(), mp_crypto_sign_verify(). mp_crypto_sign_fuse(name, fuse, wrapped, wrappedlen, msg, len, sig) calls that algorithm's mp_<algo>_sign_fuse(). RSA signs with the DS slot. Ed25519, ECDSA P-256, LMS, SLH-DSA and ML-DSA call the arch when the series declares that accelerator, and print once otherwise. The name is matched without case, as the module name (ED25519) or the label (Ed25519). Only the algorithms the build carries are found; pull them with requires. The caller owns the keys and the signature, so nothing is allocated. RSA sets the buffer sizes: MP_CRYPTO_SIGN_PRIV_MAX, MP_CRYPTO_SIGN_PUB_MAX and MP_CRYPTO_SIGN_SIG_MAX.",
	define: "SUPPORT_COMMON_CRYPTO_SIGN",
	sources: ["index.c"],
	headers: ["sign.h"],
	headerDir: "crypto",
}
