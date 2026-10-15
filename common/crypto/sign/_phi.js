export const phi = {
	kind: "module",
	name: "SIGN",
	label: "Signature API",
	description: "One API over every signature in the build: mp_sign_keypair(name, seed, seedlen, priv, pub), mp_sign(), mp_sign_verify(). The name is matched without case, as the module name (ED25519) or the label (Ed25519). Only the algorithms the build carries are found; pull them with requires. The caller owns the keys and the signature, so nothing is allocated. RSA sets the buffer sizes: MP_SIGN_PRIV_MAX, MP_SIGN_PUB_MAX and MP_SIGN_SIG_MAX.",
	define: "SUPPORT_COMMON_CRYPTO_SIGN",
	sources: ["index.c"],
	headers: ["sign.h"],
	headerDir: "crypto",
}
