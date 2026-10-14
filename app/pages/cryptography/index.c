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
 * Home page: one button per class (hash, mac, cipher, kdf, rng).
 * A class lists its algorithms. Opening one says whether the chip accelerates it.
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
