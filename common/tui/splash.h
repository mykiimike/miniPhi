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

#ifndef _HAVE_MP_TUI_SPLASH_H
	#define _HAVE_MP_TUI_SPLASH_H

	/* include tui.h first */

	/**
	 * The logo rides a colour wave, then the rule and the version fade in,
	 * then the flags. A lockup pass on a 320-wide glass is about 20 ms, so
	 * 56 frames at a 33 ms period is near 30 fps and under two seconds.
	 */
	#define MP_TUI_SPLASH_LOGO 36
	#define MP_TUI_SPLASH_TEXT 10
	#define MP_TUI_SPLASH_FLAG 10
	#define MP_TUI_SPLASH_FRAMES (MP_TUI_SPLASH_LOGO + MP_TUI_SPLASH_TEXT + MP_TUI_SPLASH_FLAG)

	/**
	 * Where the splash sits on a glass. A zero flag height means the
	 * glass was too small for the Swiss and European flags.
	 */
	typedef struct {
		int logoX, logoY, logoW, logoH;
		int wordX, wordY;
		int ruleX, ruleY, ruleW, ruleH;
		int verX, verY, verW, verH;
		int swissX, swissY, swissS;
		int euX, euY, euW, euH;
		int stacked;
	} mp_tui_splash_place_t;

	/**
	 * PUI splash ratios, in the units of a 14 px word: the phi is 20, the
	 * gap before the m is 4, and the word sits one pixel under the bowl.
	 * Rounded so an em of 14 reproduces those pixels.
	 */
	static inline int mp_tui_splash_phi(int em) {
		int h;

		if(em < 1)
			em = 1;
		h = (em * 20 + 7) / 14;
		return(h > 0 ? h : 1);
	}

	static inline int mp_tui_splash_gap(int em) {
		int gap;

		if(em < 1)
			em = 1;
		gap = (em * 4 + 7) / 14;
		return(gap > 0 ? gap : 1);
	}

	static inline int mp_tui_splash_hang(int em) {
		if(em < 1)
			em = 1;
		return((em + 7) / 14);
	}

	/**
	 * @brief Place the phi, the word, the rule, the version and the flags
	 *
	 * wordSize is the wordmark em. The phi, the gap and the hang follow the
	 * PUI splash. markW and markH are the raster's own size; the phi is
	 * never drawn larger than that raster. The flags sit in the bottom
	 * corners, the Swiss square at five sixths of the European height.
	 */
	void mp_tui_splash_place(mp_tui_splash_place_t *out, int glassW, int glassH,
		int markW, int markH, int wordW, int wordH, int wordSize, int verW, int verH);

	/**
	 * @brief Colour of one lit pixel of the phi or the word
	 *
	 * During the logo frames the colour is a hue wave. It eases in from the
	 * background and eases back to the foreground. Later frames are the foreground.
	 */
	mp_tui_color_t mp_tui_splash_ink(int frame, int x, int y, mp_tui_color_t background, mp_tui_color_t foreground);

	/** one band of the splash; non-zero while it is still up */
	int mp_tui_splash_band(mp_tui_t *ui);

#endif
