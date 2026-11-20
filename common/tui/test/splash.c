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

/* Host test of the splash layout and the fade. Not linked into the firmware. */

#include <stdio.h>

#define SUPPORT_COMMON_TUI
#include <mp.h>
#include <tui/tui.h>

struct mp_task_s {
	unsigned long delay;
};

#include "../font.c"
#include "../draw.c"
#include "../splash.c"

static int failed;

static const uint8_t _mark_alpha[1] = { 0xF0 };
const mp_tui_bitmap_t mp_tui_mark = { 1, 1, NULL, _mark_alpha };
const mp_tui_font_t mp_tui_font_wordmark = { "word", 12, 4, 14, 11, 0, NULL, NULL };

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

void mp_tui_refresh(mp_tui_t *ui) {
	(void)ui;
}

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "fail: %s\n", msg);
}

static int gap_of(int a, int b) {
	return(a > b ? a - b : b - a);
}

static void inside(const mp_tui_splash_place_t *p, int gw, int gh, const char *name) {
	expect(p->logoX >= 0 && p->logoY >= 0 && p->logoW > 0 && p->logoH > 0, name);
	expect(p->logoX + p->logoW <= gw && p->wordX >= 0 && p->wordY >= 0, name);
	expect(p->ruleX >= 0 && p->ruleY >= p->logoY && p->ruleW > 0 && p->ruleH > 0, name);
	expect(p->verX >= 0 && p->verY >= p->ruleY + p->ruleH, name);
	expect(p->ruleX + p->ruleW <= gw, name);
	if(p->euH > 0) {
		expect(p->swissS == p->euH * 5 / 6, name);
		expect(p->euW == p->euH * 3 / 2, name);
		expect(p->swissX == 8 && p->euX + p->euW == gw - 8, name);
		expect(p->euY + p->euH == gh - 8, name);
		expect(p->swissY + p->swissS == p->euY + p->euH, name);
		expect(p->euH >= 18 && p->euH <= 22, name);
	}
	(void)gh;
}

static void centered(const mp_tui_splash_place_t *p, int gw, int wordW, const char *name) {
	int left = p->stacked ? (p->logoX < p->wordX ? p->logoX : p->wordX) : p->logoX;
	int right = p->wordX + wordW;
	int logoRight = p->logoX + p->logoW;
	int mid;

	if(logoRight > right)
		right = logoRight;
	mid = left + (right - left) / 2;
	expect(gap_of(mid, gw / 2) <= 1, name);
}

static void glass(int gw, int gh, int markW, int markH, int wordW, int wordH, int wordSize,
		int verW, int verH, int wantStacked, int wantFlags, const char *name) {
	mp_tui_splash_place_t p;
	int bottom, gap;

	mp_tui_splash_place(&p, gw, gh, markW, markH, wordW, wordH, wordSize, verW, verH);
	inside(&p, gw, gh, name);
	centered(&p, gw, wordW, name);
	expect(p.stacked == wantStacked, name);
	expect((p.euH > 0) == wantFlags, name);
	bottom = p.logoY + p.logoH;
	if(p.wordY + wordH > bottom)
		bottom = p.wordY + wordH;
	expect(p.ruleY >= bottom, name);
	gap = mp_tui_splash_gap(wordSize);
	if(!wantStacked)
		expect(p.wordX == p.logoX + p.logoW + gap, name);
	if(markW > 0 && markH > 0)
		expect(gap_of(p.logoW * markH, p.logoH * markW) <= markH, name);
	if(!wantStacked && markH >= mp_tui_splash_phi(wordSize))
		expect(p.logoH == mp_tui_splash_phi(wordSize), name);
}

static void wave(void) {
	mp_tui_color_t bg = MP_TUI_RGB(0x10, 0x14, 0x1C);
	mp_tui_color_t fg = MP_TUI_RGB(0xE8, 0xEA, 0xEE);
	int midFrame = MP_TUI_SPLASH_LOGO / 2;
	mp_tui_color_t open = mp_tui_splash_ink(0, 40, 20, bg, fg);
	mp_tui_color_t early = mp_tui_splash_ink(4, 40, 20, bg, fg);
	mp_tui_color_t mid = mp_tui_splash_ink(midFrame, 20, 30, bg, fg);
	mp_tui_color_t other = mp_tui_splash_ink(midFrame, 120, 30, bg, fg);
	mp_tui_color_t settled = mp_tui_splash_ink(MP_TUI_SPLASH_LOGO - 1, 20, 30, bg, fg);
	mp_tui_color_t end = mp_tui_splash_ink(MP_TUI_SPLASH_FRAMES - 1, 180, 20, bg, fg);

	expect(open == bg, "the wave opens on the background");
	expect(early != bg && early != mid, "the wave eases in");
	expect(mid != other, "the hue moves across the mark");
	expect(mid != fg, "the middle of the wave is not white yet");
	expect(settled == fg && end == fg, "the mark settles to the foreground");
	expect(_mp_tui_splash_swiss_cover(30, 30, 60) == 16, "the cross centre is white");
	expect(_mp_tui_splash_swiss_cover(1, 1, 60) == 0, "the swiss corner is red");
	expect(_mp_tui_splash_eu_cover(45, 10, 90, 60) == 16, "the top star is solid gold");
	expect(_mp_tui_splash_eu_cover(0, 0, 90, 60) == 0, "the eu corner is blue");
	expect(_mp_tui_splash_eu_cover(45, 30, 90, 60) == 0, "the middle of the ring is blue");
}

static void optics(void) {
	mp_tui_splash_place_t p;
	int left, right, shift;

	memset(&p, 0, sizeof(p));
	p.logoX = 10;
	p.wordX = 40;
	_mp_tui_splash_nudge(&p, 100, 10, 50);
	shift = (100 - 10 - 50 - 1) / 2;
	left = 10 + shift;
	right = 50 + shift;
	expect(p.logoX == 10 + shift && p.wordX == 40 + shift, "the nudge moves phi and word together");
	expect(gap_of(left, 100 - right - 1) <= 1, "inked margins match");
}

int main(void) {
	/* 320x240 landscape: the phi and the word share a line, flags in the corners */
	glass(320, 240, 35, 44, 150, 28, 24, 48, 14, 0, 1, "zlx landscape");
	/* 240x320 portrait: same type, still one line */
	glass(240, 320, 35, 44, 150, 28, 24, 48, 14, 0, 1, "portrait 240");
	/* 172 wide: the line does not fit, so the word sits under the phi */
	glass(172, 320, 30, 38, 150, 24, 20, 40, 12, 1, 1, "portrait 172");
	/* 320x172: short glass, the line fits, flags stay in the corners */
	glass(320, 172, 30, 38, 120, 24, 20, 40, 12, 0, 1, "rp2350 landscape");
	/* too small for flags */
	glass(96, 64, 16, 20, 40, 12, 10, 28, 10, 0, 0, "small");
	wave();
	optics();

	if(failed) {
		fprintf(stderr, "%d failed\n", failed);
		return(1);
	}
	printf("ok\n");
	return(0);
}
