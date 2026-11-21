/* Shared by l512.c, l768.c and l1024.c.
 * A ciphertext of the right length always decapsulates. A modified one
 * yields the implicit-rejection secret, which the malleable vectors name. */

#if MP_CRYPTO_MLKEM_LEVEL == 512
	#include "mlkem512.h"
	#define KEM_COUNT MLKEM512_COUNT
	#define kem_vec mlkem512_vec
	#define kem_at mlkem512_at
#elif MP_CRYPTO_MLKEM_LEVEL == 768
	#include "mlkem768.h"
	#define KEM_COUNT MLKEM768_COUNT
	#define kem_vec mlkem768_vec
	#define kem_at mlkem768_at
#elif MP_CRYPTO_MLKEM_LEVEL == 1024
	#include "mlkem1024.h"
	#define KEM_COUNT MLKEM1024_COUNT
	#define kem_vec mlkem1024_vec
	#define kem_at mlkem1024_at
#else
	#error MP_CRYPTO_MLKEM_LEVEL
#endif

int main(void) {
	unsigned int i;
	uint8_t priv[MP_CRYPTO_MLKEM_PRIV];
	uint8_t pub[MP_CRYPTO_MLKEM_PUB];
	uint8_t ss[32];
	uint8_t ss2[32];
	uint8_t ct[MP_CRYPTO_MLKEM_CT];
	int flipped = 0;

	for(i = 0; i < KEM_COUNT; i++) {
		int accepted = 0;
		int have = 0;

		if(kem_vec[i].n[0] != MP_CRYPTO_MLKEM_SEED) {
			if(mp_crypto_mlkem_keypair(kem_vec[i].n[0] ? kem_at(i, 0) : NULL, kem_vec[i].n[0], priv, pub) == TRUE)
				return(host_fail("mlkem seed"));
		}
		else if(mp_crypto_mlkem_keypair(kem_at(i, 0), MP_CRYPTO_MLKEM_SEED, priv, pub) != TRUE)
			return(host_fail("mlkem keypair"));
		else {
			have = 1;
			if(kem_vec[i].n[1] == MP_CRYPTO_MLKEM_PUB && memcmp(pub, kem_at(i, 1), MP_CRYPTO_MLKEM_PUB) != 0)
				return(host_fail("mlkem ek"));
		}
		if(have && kem_vec[i].n[2] == MP_CRYPTO_MLKEM_CT && kem_vec[i].n[3] == 32) {
			if(mp_crypto_mlkem_decaps(priv, kem_at(i, 2), MP_CRYPTO_MLKEM_CT, ss) == YES && memcmp(ss, kem_at(i, 3), 32) == 0)
				accepted = 1;
		}
		else if(have) {
			memset(ss, 0xa5, sizeof(ss));
			if(mp_crypto_mlkem_decaps(priv, kem_vec[i].n[2] ? kem_at(i, 2) : ct, kem_vec[i].n[2], ss) == YES)
				accepted = 1;
			else if(ss[0] != 0xa5)
				return(host_fail("mlkem short ct"));
		}
		if(kem_vec[i].result == 1 && kem_vec[i].n[0] != MP_CRYPTO_MLKEM_SEED)
			return(host_fail("mlkem valid seed"));
		if(host_expect("mlkem", kem_vec[i].result, accepted)) {
			fprintf(stderr, "id %u\n", kem_vec[i].id);
			return(1);
		}
		if(accepted && !flipped) {
			memcpy(ct, kem_at(i, 2), MP_CRYPTO_MLKEM_CT);
			ct[0] ^= 1;
			if(mp_crypto_mlkem_decaps(priv, ct, MP_CRYPTO_MLKEM_CT, ss2) != YES || memcmp(ss2, ss, 32) == 0)
				return(host_fail("mlkem flip"));
			if(mp_crypto_mlkem_decaps(priv, ct, MP_CRYPTO_MLKEM_CT, ss) != YES || memcmp(ss, ss2, 32) != 0)
				return(host_fail("mlkem flip again"));
			flipped = 1;
		}
	}
	if(!flipped)
		return(host_fail("mlkem no vector"));
	host_report("mlkem");
	return(0);
}
