export const phi = {
	kind: "module",
	name: "FUSE",
	label: "FUSE",
	description: "Named eFuse key slots. The module is built only when the series or the MCU declares a FUSE accelerator. A slot is a number and a role, MP_CRYPTO_FUSE_HMAC, MP_CRYPTO_FUSE_DS or MP_CRYPTO_FUSE_XTS. mp_crypto_fuse_check() accepts it when the burned purpose matches that role. The registry does not burn a slot and does not hand one out.",
	needsAccelerator: true,
	define: "SUPPORT_COMMON_CRYPTO_FUSE",
	sources: ["fuse.c"],
	headers: ["fuse.h"],
	headerDir: "crypto",
}
