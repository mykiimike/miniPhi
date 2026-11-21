/* Host-only helpers for crypto tests. Not part of the firmware. */

#ifndef HOSTCRYPTO_H
#define HOSTCRYPTO_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define HOST_UNUSED __attribute__((unused))

static unsigned host_valid_n HOST_UNUSED;
static unsigned host_invalid_n HOST_UNUSED;
static unsigned host_accept_ok HOST_UNUSED;
static unsigned host_accept_no HOST_UNUSED;

static inline HOST_UNUSED int host_fail(const char *what) {
	fprintf(stderr, "%s\n", what);
	return(1);
}

static inline HOST_UNUSED int host_mem(const void *a, const void *b, unsigned int n) {
	if(!n)
		return(1);
	return(memcmp(a, b, n) == 0);
}

static inline HOST_UNUSED void host_fill(uint8_t *m, unsigned int n) {
	uint32_t x = 0xA5A5F00Du;
	unsigned int i;

	for(i = 0; i < n; i++) {
		x ^= x << 13;
		x ^= x >> 17;
		x ^= x << 5;
		m[i] = (uint8_t)x;
	}
}

static inline HOST_UNUSED uint32_t host_le32(const uint8_t *p) {
	return((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

/* result 1 valid, 2 invalid, 3 acceptable. acceptable may go either way. */
static inline HOST_UNUSED int host_expect(const char *what, int result, int ok) {
	if(result == 1) {
		host_valid_n++;
		if(!ok)
			return(host_fail(what));
		return(0);
	}
	if(result == 2 || result == 4) {
		host_invalid_n++;
		if(ok)
			return(host_fail(what));
		return(0);
	}
	if(result != 3)
		return(host_fail(what));
	if(ok)
		host_accept_ok++;
	else
		host_accept_no++;
	return(0);
}

static inline HOST_UNUSED void host_report(const char *name) {
	printf("%s valid %u invalid %u acceptable pass %u reject %u\n",
		name, host_valid_n, host_invalid_n, host_accept_ok, host_accept_no);
}

/*
 * One-shot versus every split of a fixed message.
 * Lengths 0, 1, block-1, block, block+1, 2*block+1, and 1009 bytes.
 * One split at every offset, 1-byte updates, a zero-length update between
 * bytes, and a block-1 / block / block+1 / 0 pattern.
 */
static inline HOST_UNUSED int host_chunk(const char *label, unsigned int block, unsigned int diglen,
		void *ctx,
		void (*init)(void *),
		void (*update)(void *, const void *, unsigned int),
		void (*final)(void *, uint8_t *),
		void (*shot)(const void *, unsigned int, uint8_t *)) {
	uint8_t msg[1100];
	uint8_t expect[64];
	uint8_t got[64];
	unsigned int lens[7];
	unsigned int li;

	if(diglen > sizeof(expect) || !block)
		return(host_fail(label));
	host_fill(msg, sizeof(msg));
	lens[0] = 0;
	lens[1] = 1;
	lens[2] = block > 1 ? block - 1 : 1;
	lens[3] = block;
	lens[4] = block + 1;
	lens[5] = block * 2 + 1;
	lens[6] = 1009;
	for(li = 0; li < 7; li++) {
		unsigned int len = lens[li];
		unsigned int off;
		unsigned int i;
		unsigned int pos;
		unsigned int left;
		unsigned int step;
		unsigned int seq[4];

		shot(msg, len, expect);
		for(off = 0; off <= len; off++) {
			init(ctx);
			update(ctx, msg, off);
			update(ctx, msg + off, len - off);
			final(ctx, got);
			if(!host_mem(expect, got, diglen))
				return(host_fail(label));
		}
		init(ctx);
		for(i = 0; i < len; i++)
			update(ctx, msg + i, 1);
		final(ctx, got);
		if(!host_mem(expect, got, diglen))
			return(host_fail(label));
		init(ctx);
		update(ctx, msg, 0);
		for(i = 0; i < len; i++) {
			update(ctx, msg, 0);
			update(ctx, msg + i, 1);
		}
		update(ctx, msg, 0);
		final(ctx, got);
		if(!host_mem(expect, got, diglen))
			return(host_fail(label));
		seq[0] = block > 1 ? block - 1 : 0;
		seq[1] = block;
		seq[2] = block + 1;
		seq[3] = 0;
		init(ctx);
		pos = 0;
		left = len;
		step = 0;
		while(left) {
			unsigned int take = seq[step % 4];

			if(!take)
				take = 1;
			if(take > left)
				take = left;
			update(ctx, msg + pos, take);
			pos += take;
			left -= take;
			step++;
		}
		final(ctx, got);
		if(!host_mem(expect, got, diglen))
			return(host_fail(label));
	}
	return(0);
}

/* Piece lengths that sum to len. run returns non-zero on a mismatch. */
static inline HOST_UNUSED int host_sched(unsigned int block,
		int (*run)(unsigned int len, const unsigned int *cuts, unsigned int ncuts, void *ud),
		void *ud) {
	unsigned int lens[7];
	unsigned int li;
	unsigned int cuts[2200];

	if(!block)
		return(host_fail("sched"));
	lens[0] = 0;
	lens[1] = 1;
	lens[2] = block > 1 ? block - 1 : 1;
	lens[3] = block;
	lens[4] = block + 1;
	lens[5] = block * 2 + 1;
	lens[6] = 1009;
	for(li = 0; li < 7; li++) {
		unsigned int len = lens[li];
		unsigned int off;
		unsigned int i;
		unsigned int n;
		unsigned int pos;
		unsigned int left;
		unsigned int step;
		unsigned int seq[4];

		cuts[0] = len;
		if(run(len, cuts, 1, ud))
			return(1);
		for(off = 0; off <= len; off++) {
			cuts[0] = off;
			cuts[1] = len - off;
			if(run(len, cuts, 2, ud))
				return(1);
		}
		for(i = 0; i < len; i++)
			cuts[i] = 1;
		if(run(len, cuts, len ? len : 1, ud))
			return(1);
		n = 0;
		cuts[n++] = 0;
		for(i = 0; i < len; i++) {
			cuts[n++] = 0;
			cuts[n++] = 1;
		}
		cuts[n++] = 0;
		if(run(len, cuts, n, ud))
			return(1);
		seq[0] = block > 1 ? block - 1 : 0;
		seq[1] = block;
		seq[2] = block + 1;
		seq[3] = 0;
		pos = 0;
		left = len;
		step = 0;
		n = 0;
		while(left) {
			unsigned int take = seq[step % 4];

			if(!take)
				take = 1;
			if(take > left)
				take = left;
			cuts[n++] = take;
			pos += take;
			left -= take;
			step++;
		}
		if(!n)
			cuts[n++] = 0;
		if(run(len, cuts, n, ud))
			return(1);
		(void)pos;
	}
	return(0);
}

#endif
