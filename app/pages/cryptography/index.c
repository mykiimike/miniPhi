/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2026  Michael VERGOZ                                      *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify    *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation; either version 3 of the License, or       *
 * (at your option) any later version.                                     *
 *                                                                         *
 * This program is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with this program; if not, write to the Free Software Foundation, *
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA       *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Home page: one button per class (hash, mac, cipher, kdf, rng, sign).
 * A class lists its algorithms. Opening one says whether the chip accelerates it.
 * A signature also shows its sizes and runs a self-test.
 * The list is include/gen/crypto.h, written with the build.
 */

#include "../index.h"

#ifdef SYSINFO_SCREEN

#define MP_CRYPTO_TABLE
#include <gen/crypto.h>

#if defined(SUPPORT_COMMON_CRYPTO_MAC_HMAC) && MP_CRYPTO_COUNT > 0
static const mp_tui_column_t _sysinfo_hmac_columns[] = {
	{ "Hash", 0, MP_TUI_ALIGN_LEFT },
	{ "Tag", 56, MP_TUI_ALIGN_RIGHT },
	{ "Hardware", 84, MP_TUI_ALIGN_RIGHT },
};

static const mp_tui_column_t _sysinfo_drbg_columns[] = {
	{ "Hash", 0, MP_TUI_ALIGN_LEFT },
	{ "Output", 72, MP_TUI_ALIGN_RIGHT },
	{ "Hardware", 84, MP_TUI_ALIGN_RIGHT },
};

static mp_bool_t _sysinfo_crypto_hardware(const char *name) {
	int n;

	for(n = 0; n < MP_CRYPTO_COUNT; n++)
		if(strcmp(mp_crypto[n].name, name) == 0)
			return(mp_crypto[n].hardware);
	return(NO);
}

/* HMAC and HMAC-DRBG each run over every hash of the build: list them */
static void _sysinfo_crypto_hashes(mp_tui_widget_t *page, sysinfo_t *sysinfo, int y, const mp_tui_column_t *columns) {
	const mp_hash_algo_t *algo;
	int rows = mp_hash_count();
	int n;

	if(rows > MP_CRYPTO_MAX)
		rows = MP_CRYPTO_MAX;
	mp_tui_table(&sysinfo->screen.cryptoTable, page, 8, y, page->rect.w - 16,
		columns, 3, &sysinfo->screen.cryptoCell[0][0], MP_CRYPTO_MAX);
	mp_tui_table_rows(&sysinfo->screen.cryptoTable, rows);
	for(n = 0; n < rows; n++) {
		algo = mp_hash_at(n);
		mp_tui_table_set(&sysinfo->screen.cryptoTable, n, 0, "%s", algo->label);
		mp_tui_table_set(&sysinfo->screen.cryptoTable, n, 1, "%u B", algo->size);
		mp_tui_table_set(&sysinfo->screen.cryptoTable, n, 2, "%s", _sysinfo_crypto_hardware(algo->name) == YES ? "yes" : "no");
	}
}
#endif

#ifdef SUPPORT_COMMON_CRYPTO_SIGN
#if MP_RSA_BITS == 3072
/* PKCS#1 v1.5 SHA-256 signature of "miniPhi". The page verifies this and does not sign. */
static const uint8_t _sysinfo_rsa_pub[MP_RSA_PUB] = {
	0x79, 0x46, 0x20, 0x6b, 0x4d, 0x0e, 0xdd, 0xb4, 0x60, 0x7a, 0x85, 0x64,
	0xc1, 0x42, 0x41, 0xf9, 0x38, 0x87, 0x4e, 0xde, 0xdf, 0x04, 0xe8, 0xc5,
	0x70, 0xde, 0x1c, 0x1c, 0xd9, 0x1d, 0x04, 0x91, 0x7d, 0x65, 0xe8, 0x4b,
	0x6b, 0xa4, 0x75, 0x1f, 0xa4, 0x7c, 0xa7, 0x8b, 0x65, 0xf4, 0xde, 0x90,
	0x34, 0x62, 0x4f, 0x3d, 0xb5, 0x37, 0xe2, 0xc4, 0x3c, 0xf6, 0x7d, 0xef,
	0xb7, 0x5b, 0x21, 0xb3, 0xfc, 0xfe, 0x18, 0x67, 0xec, 0x85, 0x07, 0x84,
	0xb8, 0x0f, 0x4a, 0xfe, 0x4c, 0xe2, 0xd5, 0xdf, 0xc1, 0xf7, 0xee, 0x86,
	0xb8, 0xf3, 0xee, 0x73, 0xd8, 0xe1, 0x28, 0x8e, 0xee, 0x22, 0x5b, 0x6c,
	0x14, 0xb7, 0xdb, 0x8d, 0x27, 0xc7, 0xe8, 0x45, 0xa7, 0x52, 0x5f, 0x91,
	0x19, 0x67, 0xa8, 0xa5, 0x23, 0x28, 0x39, 0x28, 0x7c, 0x38, 0xef, 0x3e,
	0x3d, 0x69, 0x4f, 0x8c, 0x13, 0xf6, 0x1b, 0xc2, 0xdf, 0x79, 0xa0, 0xa8,
	0x36, 0xa3, 0xa3, 0x21, 0xc8, 0x33, 0x05, 0x64, 0x56, 0xa6, 0xb7, 0xea,
	0xad, 0x24, 0x7e, 0x49, 0x4f, 0x03, 0x23, 0xef, 0x37, 0x1f, 0xdc, 0x04,
	0x2f, 0xc8, 0x72, 0xb1, 0xc5, 0x59, 0x7d, 0xd1, 0x28, 0xa0, 0x8a, 0xc9,
	0x13, 0xc8, 0x41, 0x46, 0xfc, 0x98, 0x94, 0x4d, 0x52, 0xfc, 0xc2, 0x1d,
	0x8b, 0xfe, 0x04, 0x02, 0x84, 0x0d, 0x60, 0xe8, 0x20, 0x4a, 0x6a, 0x37,
	0x3b, 0xea, 0xcd, 0xf5, 0xa3, 0xc6, 0x01, 0xe3, 0xf7, 0xb3, 0xd3, 0x29,
	0x15, 0x1c, 0x07, 0xe1, 0x84, 0xf3, 0x27, 0x38, 0x73, 0x38, 0x51, 0x86,
	0xa3, 0x82, 0xee, 0x53, 0x16, 0xbb, 0xba, 0xa2, 0x2f, 0x38, 0xf2, 0xeb,
	0xfb, 0x5c, 0xa1, 0x86, 0xb5, 0x04, 0xa3, 0x6c, 0x96, 0xda, 0xb4, 0x72,
	0xc0, 0xcd, 0xf1, 0xe3, 0xf8, 0xad, 0x51, 0xa1, 0xbc, 0xc1, 0x3e, 0xb1,
	0x30, 0x5a, 0xc7, 0x44, 0x13, 0xd8, 0x9e, 0xf9, 0x0b, 0xf8, 0x7e, 0xb6,
	0x67, 0xc8, 0x15, 0xd1, 0x52, 0xc3, 0xec, 0xeb, 0x3f, 0x21, 0x93, 0x67,
	0x1d, 0x15, 0xb9, 0xb6, 0xcc, 0xc2, 0x07, 0x1c, 0x05, 0xfb, 0x7a, 0x75,
	0x98, 0xac, 0x13, 0x1b, 0x1c, 0x70, 0xc2, 0xed, 0x9d, 0xf1, 0xe3, 0x51,
	0xcf, 0x04, 0x1c, 0xa4, 0x72, 0x68, 0x33, 0x46, 0x18, 0x61, 0x17, 0xca,
	0xf0, 0x93, 0x91, 0xa5, 0x64, 0xdc, 0x16, 0xe6, 0xeb, 0x02, 0x6c, 0xfa,
	0xf3, 0xad, 0x7c, 0xde, 0xa3, 0x94, 0x80, 0x97, 0x05, 0xd8, 0x74, 0x83,
	0x16, 0xb7, 0x12, 0xac, 0xfb, 0x03, 0xbf, 0xad, 0x0e, 0xcb, 0xeb, 0x6d,
	0xc4, 0xcb, 0xad, 0x7d, 0xca, 0xb0, 0x3f, 0xa5, 0x8d, 0x24, 0xdc, 0x4b,
	0xa1, 0x99, 0xdd, 0x01, 0xbb, 0x5e, 0x5f, 0x02, 0xcb, 0x88, 0x84, 0xf4,
	0x80, 0x5e, 0x07, 0xcd, 0xdc, 0x16, 0x3a, 0xda, 0x45, 0x5e, 0xc4, 0x47,
	0x00, 0x01, 0x00, 0x01,
};

static const uint8_t _sysinfo_rsa_sig[MP_RSA_SIG] = {
	0x5c, 0xf3, 0xff, 0x5d, 0xc2, 0xbd, 0x22, 0xcc, 0x93, 0xa5, 0x7d, 0x60,
	0xe5, 0xa5, 0xb8, 0xe9, 0xe7, 0x1b, 0x2a, 0xfd, 0x1d, 0x14, 0x55, 0x2b,
	0x9a, 0x64, 0xb2, 0x8d, 0x7f, 0x5c, 0xef, 0xbb, 0x73, 0x52, 0xc4, 0x7a,
	0x44, 0x28, 0xc1, 0x64, 0xe9, 0x5f, 0x2d, 0x2a, 0x36, 0x9e, 0x3c, 0x50,
	0xaf, 0x75, 0x13, 0x47, 0xb1, 0x84, 0x03, 0x1e, 0x92, 0x25, 0xb6, 0x93,
	0xbc, 0x9b, 0xd5, 0xa4, 0x45, 0x5d, 0x54, 0x97, 0x18, 0xee, 0xff, 0xb6,
	0xbd, 0x39, 0xe4, 0x12, 0xb2, 0x4c, 0x7a, 0x72, 0xd8, 0x02, 0x2e, 0x2e,
	0x51, 0x28, 0x13, 0x0f, 0xc9, 0x57, 0xa0, 0xfd, 0xdf, 0x75, 0x38, 0x71,
	0xf0, 0x96, 0xc8, 0xcb, 0x3c, 0x65, 0xf1, 0x8f, 0xf4, 0x41, 0xdf, 0x93,
	0xc0, 0x04, 0xcb, 0x6b, 0x12, 0xa3, 0xae, 0x39, 0x9a, 0xf5, 0x44, 0xb3,
	0x70, 0xe6, 0x89, 0xf7, 0x54, 0xf3, 0x5c, 0x47, 0x69, 0xae, 0x4c, 0xb9,
	0xed, 0xed, 0x92, 0x66, 0xd7, 0xaa, 0x31, 0xc0, 0x64, 0x1c, 0x41, 0xad,
	0x33, 0xd2, 0x6f, 0x49, 0x0c, 0xa1, 0x5b, 0x20, 0x42, 0x42, 0x6a, 0xcc,
	0x8f, 0xee, 0x0e, 0x78, 0xc7, 0xa0, 0x48, 0xf1, 0x18, 0x2f, 0x64, 0x7f,
	0xbf, 0xf1, 0x17, 0x8c, 0x68, 0x9b, 0x3d, 0xe0, 0x92, 0xd0, 0xd1, 0x0a,
	0x24, 0x28, 0xc7, 0x27, 0x06, 0xb9, 0x97, 0x5b, 0x69, 0x3d, 0x9a, 0xd9,
	0x0b, 0x60, 0x5a, 0x5c, 0xd3, 0x98, 0xb1, 0x2b, 0x95, 0x49, 0xfa, 0x8f,
	0x2c, 0x9b, 0x5e, 0x4b, 0x0d, 0x39, 0xbb, 0xf5, 0x9c, 0x7f, 0x6e, 0xfb,
	0x9c, 0xcd, 0x5c, 0x2f, 0x22, 0x2e, 0x12, 0x8d, 0xe5, 0xa2, 0xc4, 0x92,
	0xfe, 0x48, 0x02, 0xa4, 0xb9, 0x7c, 0x7a, 0x0d, 0x8e, 0x40, 0xb7, 0x8c,
	0xab, 0xb9, 0xfa, 0x8d, 0x98, 0x1e, 0x3e, 0x45, 0x8f, 0x56, 0x51, 0x85,
	0xcf, 0x4f, 0x9d, 0x9e, 0xe2, 0x1d, 0x7c, 0xfc, 0x03, 0x82, 0xed, 0xb8,
	0x1c, 0x3d, 0x9e, 0x0b, 0x04, 0x39, 0x00, 0x02, 0x9e, 0x63, 0x11, 0xf3,
	0x52, 0xb3, 0x1b, 0xc0, 0x05, 0x68, 0x0e, 0x97, 0x93, 0x9e, 0xc8, 0xb8,
	0xfd, 0x91, 0xef, 0xd8, 0x1e, 0xfd, 0x97, 0x84, 0x06, 0x62, 0xc6, 0x3e,
	0x4a, 0xae, 0x9e, 0xa1, 0x54, 0x15, 0x16, 0x81, 0x3f, 0xbe, 0x59, 0xe4,
	0xe0, 0xaf, 0x2d, 0xf8, 0xfb, 0xc7, 0xda, 0x05, 0x0e, 0x8c, 0x65, 0xec,
	0x02, 0xd9, 0xf8, 0xae, 0x23, 0x72, 0xf0, 0x11, 0xd1, 0x8c, 0x9e, 0x26,
	0x9e, 0x3a, 0xdf, 0x0a, 0x8f, 0x52, 0xc5, 0x7d, 0xf0, 0x3a, 0x2a, 0xd3,
	0x5e, 0x7c, 0x94, 0xa3, 0x96, 0xb8, 0x91, 0x11, 0x74, 0x45, 0xef, 0x5b,
	0xa2, 0x02, 0x76, 0xfb, 0xa5, 0xa8, 0xac, 0xf8, 0x46, 0x03, 0x4e, 0x2c,
	0xf0, 0xf9, 0xc1, 0x87, 0x8e, 0x2e, 0xdf, 0xf3, 0x47, 0x2e, 0x1c, 0xb1,
};
#endif

/* verify-only vectors for the post-quantum schemes; see vectors.c */
#if defined(SUPPORT_COMMON_CRYPTO_SIGN_LMS) && defined(MP_LMS_PUB)
extern const uint8_t _sysinfo_lms_pub[MP_LMS_PUB];
extern const uint8_t _sysinfo_lms_msg[131];
extern const uint8_t _sysinfo_lms_sig[MP_LMS_SIG];
#endif
#if defined(SUPPORT_COMMON_CRYPTO_SIGN_SLH) && defined(MP_SLH_PK_SIZE)
extern const uint8_t _sysinfo_slh_pub[MP_SLH_PK_SIZE];
extern const uint8_t _sysinfo_slh_sig[MP_SLH_SIG_SIZE];
#endif
#if defined(SUPPORT_COMMON_CRYPTO_SIGN_MLDSA) && defined(MP_MLDSA_PK_SIZE) && (MP_MLDSA_SIG_SIZE == 2420)
extern const uint8_t _sysinfo_mldsa_pub[MP_MLDSA_PK_SIZE];
extern const uint8_t _sysinfo_mldsa_msg[7];
extern const uint8_t _sysinfo_mldsa_sig[MP_MLDSA_SIG_SIZE];
#endif

static void _sysinfo_sign_test(sysinfo_t *sysinfo, const mp_sign_algo_t *algo) {
	static uint8_t priv[32];
	static uint8_t pub[64];
	static uint8_t sigbuf[64];
	static const uint8_t seed[32] = {
		1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
		17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
	};
	const char *msg = "miniPhi";
	unsigned long hz, ms, t0;
	unsigned int n;
	mp_bool_t ok = NO;
	int vector = 0;
	int stored = 1;

	mp_tui_label_set(&sysinfo->screen.signInfo, "Key %u/%u, signature %u",
		algo->privSize, algo->pubSize, algo->sigSize);
	t0 = mp_clock_ticks();
#ifdef SUPPORT_COMMON_CRYPTO_SIGN_RSA
	if(strcmp(algo->name, "RSA") == 0) {
		vector = 1;
#if MP_RSA_BITS == 3072
		ok = mp_sign_verify(algo->name, _sysinfo_rsa_pub, msg, 7, _sysinfo_rsa_sig, MP_RSA_SIG);
#else
		stored = 0;
#endif
	}
	else
#endif
#ifdef SUPPORT_COMMON_CRYPTO_SIGN_LMS
	if(strcmp(algo->name, "LMS") == 0) {
		vector = 1;
		ok = mp_sign_verify(algo->name, _sysinfo_lms_pub, _sysinfo_lms_msg, sizeof(_sysinfo_lms_msg), _sysinfo_lms_sig, MP_LMS_SIG);
	}
	else
#endif
#ifdef SUPPORT_COMMON_CRYPTO_SIGN_SLH
	if(strcmp(algo->name, "SLH") == 0) {
		vector = 1;
		ok = mp_sign_verify(algo->name, _sysinfo_slh_pub, msg, 7, _sysinfo_slh_sig, MP_SLH_SIG_SIZE);
	}
	else
#endif
#ifdef SUPPORT_COMMON_CRYPTO_SIGN_MLDSA
	if(strcmp(algo->name, "MLDSA") == 0) {
		vector = 1;
		ok = mp_sign_verify(algo->name, _sysinfo_mldsa_pub, _sysinfo_mldsa_msg, sizeof(_sysinfo_mldsa_msg), _sysinfo_mldsa_sig, MP_MLDSA_SIG_SIZE);
	}
	else
#endif
	if(mp_sign_keypair(algo->name, seed, sizeof(seed), priv, pub) == TRUE) {
		n = mp_sign(algo->name, priv, msg, 7, sigbuf);
		if(n && mp_sign_verify(algo->name, pub, msg, 7, sigbuf, n) == YES)
			ok = YES;
	}
	memset(priv, 0, sizeof(priv));
	memset(pub, 0, sizeof(pub));
	memset(sigbuf, 0, sizeof(sigbuf));
	hz = mp_clock_tick_hz();
	ms = hz ? (mp_clock_ticks() - t0) * 1000UL / hz : 0;
	if(!stored) {
		mp_tui_label_set(&sysinfo->screen.signResult, "No vector for this size");
		mp_tui_label_color(&sysinfo->screen.signResult, sysinfo->screen.tui.theme.muted);
	}
	else if(ok == YES) {
		mp_tui_label_set(&sysinfo->screen.signResult, vector ? "Vector verified, %lu ms" : "Signed and verified, %lu ms", ms);
		mp_tui_label_color(&sysinfo->screen.signResult, sysinfo->screen.tui.theme.accent);
	}
	else {
		mp_tui_label_set(&sysinfo->screen.signResult, "Failed");
		mp_tui_label_color(&sysinfo->screen.signResult, sysinfo->screen.tui.theme.muted);
	}
}
#endif

static void _sysinfo_crypto_detail(mp_tui_widget_t *page, void *user) {
	sysinfo_t *sysinfo = user;
	const mp_tui_font_t *font = page->tui->theme.font;
	int line = font->lineHeight + 1;
	int pick = sysinfo->screen.cryptoPick;
	int accelerated = 0;

#if MP_CRYPTO_COUNT > 0
	if(pick >= 0 && pick < MP_CRYPTO_COUNT && mp_crypto[pick].hardware == YES)
		accelerated = 1;
#else
	(void)pick;
#endif
	mp_tui_label(&sysinfo->screen.cryptoState, page, 8, 12, page->rect.w - 16, line,
		accelerated ? "Accelerated" : "Not accelerated");
	mp_tui_label_color(&sysinfo->screen.cryptoState, accelerated ? page->tui->theme.accent : page->tui->theme.muted);
#ifdef SUPPORT_COMMON_CRYPTO_SIGN
	if(pick >= 0 && pick < MP_CRYPTO_COUNT && strcmp(mp_crypto[pick].family, "sign") == 0) {
		const mp_sign_algo_t *algo = mp_sign_find(mp_crypto[pick].name);
		int y = 12 + line + 8;

		mp_tui_label(&sysinfo->screen.signInfo, page, 8, y, page->rect.w - 16, line, "");
		y += line + 4;
		mp_tui_label(&sysinfo->screen.signResult, page, 8, y, page->rect.w - 16, line, "");
		if(algo)
			_sysinfo_sign_test(sysinfo, algo);
		else {
			mp_tui_label_set(&sysinfo->screen.signResult, "Failed");
			mp_tui_label_color(&sysinfo->screen.signResult, page->tui->theme.muted);
		}
	}
#endif
#if defined(SUPPORT_COMMON_CRYPTO_MAC_HMAC) && MP_CRYPTO_COUNT > 0
	if(pick >= 0 && pick < MP_CRYPTO_COUNT && strcmp(mp_crypto[pick].name, "HMAC") == 0)
		_sysinfo_crypto_hashes(page, sysinfo, 12 + line + 10, _sysinfo_hmac_columns);
	else if(pick >= 0 && pick < MP_CRYPTO_COUNT && strcmp(mp_crypto[pick].name, "HMAC_DRBG") == 0)
		_sysinfo_crypto_hashes(page, sysinfo, 12 + line + 10, _sysinfo_drbg_columns);
#endif
}

#if MP_CRYPTO_COUNT > 0
static int _sysinfo_crypto_same(const char *a, const char *b) {
	if(!a || !b)
		return(0);
	return(strcmp(a, b) == 0);
}

static void _sysinfo_crypto_buttons(mp_tui_widget_t *page, sysinfo_t *sysinfo, int y, int matchFamily, mp_tui_button_on_t onPress) {
	const mp_tui_font_t *font = page->tui->theme.font;
	int line = font->lineHeight + 1;
	int width = page->rect.w - 16;
	int h = line + 18;
	int n, slot, seen;
	int first[MP_CRYPTO_MAX];
	int count = MP_CRYPTO_COUNT;

	if(count > MP_CRYPTO_MAX)
		count = MP_CRYPTO_MAX;
	slot = 0;
	for(n = 0; n < count && slot < MP_CRYPTO_MAX; n++) {
		if(matchFamily) {
			if(!_sysinfo_crypto_same(mp_crypto[n].family, sysinfo->screen.cryptoFamily))
				continue;
		}
		else {
			for(seen = 0; seen < slot; seen++)
				if(_sysinfo_crypto_same(mp_crypto[first[seen]].family, mp_crypto[n].family))
					break;
			if(seen < slot)
				continue;
			first[slot] = n;
		}
		mp_tui_button(&sysinfo->screen.cryptoButton[slot], page, 8, y, width, h,
			matchFamily ? mp_crypto[n].label : mp_crypto[n].familyLabel);
		sysinfo->screen.cryptoButton[slot].chevron = 1;
		sysinfo->screen.cryptoButton[slot].onPress = onPress;
		sysinfo->screen.cryptoButton[slot].user = sysinfo;
		sysinfo->screen.cryptoButton[slot].id = n;
		y += h + 6;
		slot++;
	}
}

static void _sysinfo_on_algo(mp_tui_button_t *button) {
	sysinfo_t *sysinfo = button->user;
	int pick = button->id;

	if(!sysinfo || sysinfo->screen.cryptoDetail < 0)
		return;
	if(pick < 0 || pick >= MP_CRYPTO_COUNT)
		return;
	sysinfo->screen.cryptoPick = pick;
	sysinfo->screen.tui.app[sysinfo->screen.cryptoDetail].name = mp_crypto[pick].label;
	mp_tui_enter(&sysinfo->screen.tui, sysinfo->screen.cryptoDetail);
}

#ifdef SUPPORT_COMMON_CRYPTO_RNG
#define SYSINFO_RNG_BYTES 12

static void _sysinfo_rng_draw(sysinfo_t *sysinfo) {
	uint8_t bytes[SYSINFO_RNG_BYTES];
	char text[SYSINFO_RNG_BYTES * 2 + 1];
	int n;

	sysinfo->screen.rngLast = mp_clock_ticks();
	if(mp_rng_bytes(bytes, sizeof(bytes)) != TRUE) {
		mp_tui_label_set(&sysinfo->screen.rngValue, "Seeding...");
		mp_tui_label_color(&sysinfo->screen.rngValue, sysinfo->screen.tui.theme.muted);
		return;
	}
	for(n = 0; n < SYSINFO_RNG_BYTES; n++)
		snprintf(text + n * 2, 3, "%02X", bytes[n]);
	memset(bytes, 0, sizeof(bytes));
	mp_tui_label_set(&sysinfo->screen.rngValue, "%s", text);
	mp_tui_label_color(&sysinfo->screen.rngValue, sysinfo->screen.tui.theme.foreground);
}

/* the generator in use, a live number, then a heading over the algorithms; returns where they start */
static int _sysinfo_rng_head(mp_tui_widget_t *page, sysinfo_t *sysinfo) {
	const mp_tui_font_t *font = page->tui->theme.font;
	int line = font->lineHeight + 1;
	int width = page->rect.w - 16;
	const char *hash = mp_rng_hash();
	int y = 8;

	mp_tui_label(&sysinfo->screen.rngGenerator, page, 8, y, width, line, "");
	if(hash)
		mp_tui_label_set(&sysinfo->screen.rngGenerator, "%s / %s", mp_rng_generator(), hash);
	else
		mp_tui_label_set(&sysinfo->screen.rngGenerator, "%s", mp_rng_generator());
	mp_tui_label_color(&sysinfo->screen.rngGenerator, page->tui->theme.accent);
	y += line + 6;

	mp_tui_label(&sysinfo->screen.rngValue, page, 8, y, width, line, "");
	y += line + 12;

	mp_tui_label(&sysinfo->screen.rngHeading, page, 8, y, width, line, "Available");
	mp_tui_label_color(&sysinfo->screen.rngHeading, page->tui->theme.muted);
	y += line + 6;

	sysinfo->screen.rngShown = 1;
	_sysinfo_rng_draw(sysinfo);
	return(y);
}
#endif

static void _sysinfo_crypto_class(mp_tui_widget_t *page, void *user) {
	sysinfo_t *sysinfo = user;
	int y = 8;

#ifdef SUPPORT_COMMON_CRYPTO_RNG
	sysinfo->screen.rngShown = 0;
	if(_sysinfo_crypto_same(sysinfo->screen.cryptoFamily, "rng"))
		y = _sysinfo_rng_head(page, sysinfo);
#endif
	_sysinfo_crypto_buttons(page, sysinfo, y, 1, _sysinfo_on_algo);
}

static void _sysinfo_on_class(mp_tui_button_t *button) {
	sysinfo_t *sysinfo = button->user;
	int pick = button->id;

	if(!sysinfo || sysinfo->screen.cryptoClass < 0)
		return;
	if(pick < 0 || pick >= MP_CRYPTO_COUNT)
		return;
	sysinfo->screen.cryptoFamily = mp_crypto[pick].family;
	sysinfo->screen.tui.app[sysinfo->screen.cryptoClass].name = mp_crypto[pick].familyLabel;
	mp_tui_enter(&sysinfo->screen.tui, sysinfo->screen.cryptoClass);
}
#endif

static void _sysinfo_crypto_page(mp_tui_widget_t *page, void *user) {
	sysinfo_t *sysinfo = user;

#if MP_CRYPTO_COUNT > 0
	_sysinfo_crypto_buttons(page, sysinfo, 8, 0, _sysinfo_on_class);
#else
	{
		const mp_tui_font_t *font = page->tui->theme.font;

		mp_tui_label(&sysinfo->screen.cryptoState, page, 8, 12, page->rect.w - 16, font->lineHeight + 1, "None");
		mp_tui_label_color(&sysinfo->screen.cryptoState, page->tui->theme.muted);
	}
#endif
}

void _sysinfo_crypto_open(sysinfo_t *sysinfo) {
	mp_tui_t *tui = &sysinfo->screen.tui;

	mp_tui_app(tui, "Cryptography", _sysinfo_crypto_page, sysinfo);
	sysinfo->screen.cryptoClass = -1;
	sysinfo->screen.cryptoDetail = -1;
	sysinfo->screen.cryptoPick = 0;
	sysinfo->screen.cryptoFamily = NULL;
#ifdef SUPPORT_COMMON_CRYPTO_RNG
	sysinfo->screen.rngShown = 0;
#endif
#if MP_CRYPTO_COUNT > 0
	if(mp_tui_child(tui, "Class", _sysinfo_crypto_class, sysinfo))
		sysinfo->screen.cryptoClass = tui->appCount - 1;
#endif
	if(mp_tui_child(tui, "Algorithm", _sysinfo_crypto_detail, sysinfo))
		sysinfo->screen.cryptoDetail = tui->appCount - 1;
}

void _sysinfo_crypto_refresh(sysinfo_t *sysinfo) {
#ifdef SUPPORT_COMMON_CRYPTO_RNG
	if(!sysinfo->screen.rngShown || sysinfo->screen.tui.opened != sysinfo->screen.cryptoClass)
		return;
	if(mp_clock_ticks() - sysinfo->screen.rngLast < _sysinfo_ticks(500))
		return;
	_sysinfo_rng_draw(sysinfo);
#else
	(void)sysinfo;
#endif
}

#endif
