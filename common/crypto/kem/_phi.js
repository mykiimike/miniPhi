export const phi = {
	kind: "module",
	name: "KEM",
	label: "KEM API",
	description: "One API over every key-encapsulation mechanism in the build: mp_crypto_kem_keypair(name, seed, seedlen, priv, pub), mp_crypto_kem_encaps(), mp_crypto_kem_decaps(). mp_crypto_kem_decaps_fuse() calls that algorithm's fuse path. The name is matched without case, as the module name (MLKEM) or the label (ML-KEM). Only the algorithms the build carries are found. The caller owns every buffer, seeds and coins included, so a self-test can replay a known answer. Encapsulation returns the ciphertext length, or 0. Decapsulation returns YES and always writes the shared secret when the ciphertext has the algorithm's length, including a ciphertext that fails the check: that secret is then the rejection value, so the two cases take the same time. The directory is common/crypto/kem, three path components, so it is not a class on the Cryptography page.",
	define: "SUPPORT_COMMON_CRYPTO_KEM",
	sources: ["index.c"],
	headers: ["kem.h"],
	headerDir: "crypto",
}
