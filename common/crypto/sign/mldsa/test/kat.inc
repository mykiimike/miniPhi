/* Shared by l2.c, l3.c and l5.c.
 * Verify is empty-context only. Sign is deterministic (rnd = 0). */

#if MP_CRYPTO_MLDSA_LEVEL == 2
	#include "mldsa44.h"
	#include "mldsa44_sign.h"
	#define V_COUNT MLDSA44_COUNT
	#define v_vec mldsa44_vec
	#define v_at mldsa44_at
	#define S_COUNT MLDSA44S_COUNT
	#define s_vec mldsa44s_vec
	#define s_at mldsa44s_at
#elif MP_CRYPTO_MLDSA_LEVEL == 3
	#include "mldsa65.h"
	#include "mldsa65_sign.h"
	#define V_COUNT MLDSA65_COUNT
	#define v_vec mldsa65_vec
	#define v_at mldsa65_at
	#define S_COUNT MLDSA65S_COUNT
	#define s_vec mldsa65s_vec
	#define s_at mldsa65s_at
#elif MP_CRYPTO_MLDSA_LEVEL == 5
	#include "mldsa87.h"
	#include "mldsa87_sign.h"
	#define V_COUNT MLDSA87_COUNT
	#define v_vec mldsa87_vec
	#define v_at mldsa87_at
	#define S_COUNT MLDSA87S_COUNT
	#define s_vec mldsa87s_vec
	#define s_at mldsa87s_at
#else
	#error MP_CRYPTO_MLDSA_LEVEL
#endif

int main(void) {
	unsigned int i;
	uint8_t priv[MP_CRYPTO_MLDSA_SK_SIZE];
	uint8_t pub[MP_CRYPTO_MLDSA_PK_SIZE];
	uint8_t sig[MP_CRYPTO_MLDSA_SIG_SIZE];
	uint8_t badpk[MP_CRYPTO_MLDSA_PK_SIZE];
	uint8_t msg[4096];
	int flipped = 0;

	for(i = 0; i < V_COUNT; i++) {
		int accepted = 0;

		if(v_vec[i].n[0] == MP_CRYPTO_MLDSA_PK_SIZE) {
			const uint8_t *m = v_vec[i].n[1] ? v_at(i, 1) : NULL;

			accepted = mp_crypto_mldsa_verify(v_at(i, 0), m, v_vec[i].n[1], v_at(i, 2), v_vec[i].n[2]) == YES;
		}
		else if(v_vec[i].result == 1)
			return(host_fail("mldsa pk"));
		if(host_expect("mldsa", v_vec[i].result, accepted)) {
			fprintf(stderr, "id %u\n", v_vec[i].id);
			return(1);
		}
		if(accepted && !flipped && v_vec[i].n[2] == MP_CRYPTO_MLDSA_SIG_SIZE) {
			memcpy(sig, v_at(i, 2), MP_CRYPTO_MLDSA_SIG_SIZE);
			sig[0] ^= 1;
			if(mp_crypto_mldsa_verify(v_at(i, 0), v_at(i, 1), v_vec[i].n[1], sig, MP_CRYPTO_MLDSA_SIG_SIZE) == YES)
				return(host_fail("mldsa sig"));
			if(v_vec[i].n[1] && v_vec[i].n[1] < sizeof(msg)) {
				memcpy(msg, v_at(i, 1), v_vec[i].n[1]);
				msg[0] ^= 1;
				if(mp_crypto_mldsa_verify(v_at(i, 0), msg, v_vec[i].n[1], v_at(i, 2), MP_CRYPTO_MLDSA_SIG_SIZE) == YES)
					return(host_fail("mldsa msg"));
			}
			memcpy(badpk, v_at(i, 0), MP_CRYPTO_MLDSA_PK_SIZE);
			badpk[0] ^= 1;
			if(mp_crypto_mldsa_verify(badpk, v_at(i, 1), v_vec[i].n[1], v_at(i, 2), MP_CRYPTO_MLDSA_SIG_SIZE) == YES)
				return(host_fail("mldsa pk flip"));
			flipped = 1;
		}
	}
	if(!flipped)
		return(host_fail("mldsa no vector"));
	for(i = 0; i < S_COUNT; i++) {
		unsigned int n;

		if(s_vec[i].n[0] != 32 || s_vec[i].n[2] != MP_CRYPTO_MLDSA_SIG_SIZE)
			return(host_fail("mldsa sign shape"));
		if(mp_crypto_mldsa_keypair(s_at(i, 0), 32, priv, pub) != TRUE)
			return(host_fail("mldsa keypair"));
		if(mp_crypto_mldsa_verify(pub, s_at(i, 1), s_vec[i].n[1], s_at(i, 2), s_vec[i].n[2]) != YES)
			return(host_fail("mldsa sign verify"));
		n = mp_crypto_mldsa_sign(priv, s_at(i, 1), s_vec[i].n[1], sig);
		if(n != MP_CRYPTO_MLDSA_SIG_SIZE || memcmp(sig, s_at(i, 2), n) != 0)
			return(host_fail("mldsa sign"));
		host_valid_n++;
	}
	host_report("mldsa");
	return(0);
}
