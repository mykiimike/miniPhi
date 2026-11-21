/* Shared by sig3072.c and sig4096.c. The public key is n || e. */

int main(void) {
	unsigned int i;
	uint8_t bad[MP_CRYPTO_RSA_SIG];
	uint8_t badpk[MP_CRYPTO_RSA_PUB];
	uint8_t msg[2048];
	int flipped = 0;

	for(i = 0; i < RSA_COUNT; i++) {
		int accepted = 0;

		if(rsa_vec[i].n[0] == MP_CRYPTO_RSA_PUB) {
			const uint8_t *m = rsa_vec[i].n[1] ? rsa_at(i, 1) : NULL;

			accepted = mp_crypto_rsa_verify(rsa_at(i, 0), m, rsa_vec[i].n[1], rsa_at(i, 2), rsa_vec[i].n[2]) == YES;
		}
		else if(rsa_vec[i].result == 1)
			return(host_fail("rsa pk"));
		if(host_expect("rsa", rsa_vec[i].result, accepted)) {
			fprintf(stderr, "id %u\n", rsa_vec[i].id);
			return(1);
		}
		if(accepted && !flipped && rsa_vec[i].n[2] == MP_CRYPTO_RSA_SIG) {
			memcpy(bad, rsa_at(i, 2), MP_CRYPTO_RSA_SIG);
			bad[0] ^= 1;
			if(mp_crypto_rsa_verify(rsa_at(i, 0), rsa_at(i, 1), rsa_vec[i].n[1], bad, MP_CRYPTO_RSA_SIG) == YES)
				return(host_fail("rsa sig"));
			if(rsa_vec[i].n[1] && rsa_vec[i].n[1] < sizeof(msg)) {
				memcpy(msg, rsa_at(i, 1), rsa_vec[i].n[1]);
				msg[0] ^= 1;
				if(mp_crypto_rsa_verify(rsa_at(i, 0), msg, rsa_vec[i].n[1], rsa_at(i, 2), MP_CRYPTO_RSA_SIG) == YES)
					return(host_fail("rsa msg"));
			}
			memcpy(badpk, rsa_at(i, 0), MP_CRYPTO_RSA_PUB);
			badpk[0] ^= 1;
			if(mp_crypto_rsa_verify(badpk, rsa_at(i, 1), rsa_vec[i].n[1], rsa_at(i, 2), MP_CRYPTO_RSA_SIG) == YES)
				return(host_fail("rsa pk flip"));
			flipped = 1;
		}
	}
	if(!flipped)
		return(host_fail("rsa no vector"));
	host_report("rsa");
	return(0);
}
