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

/* Host preview of the real PUI splash painter (pages/about.c).
 * Not linked into the firmware and not part of make test.
 * Assets are the ZLX scale: ppi 143, MP_PUI_SCALE 16/16, so dp() is identity.
 *
 *   node /tmp/gen-pui-host.mjs
 *   gcc -std=c99 -Wall -Wextra -Werror -O2 \
 *       -I /tmp/pui-host/include -I common \
 *       common/pui/test/preview.c -o /tmp/pui-splash-preview
 *   /tmp/pui-splash-preview /tmp/pui-splash.ppm 75
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SUPPORT_COMMON_PUI
#include <mp.h>
#include <pui/pui.h>

void mp_pui_detach(mp_pui_t *ui) {
	(void)ui;
}

void mp_pui_page_glass_release(mp_pui_t *ui) {
	(void)ui;
}

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

#include "../pui_font.c"
#include "../pui_font_default.c"
#include "../pui.c"

#define _mp_pui_line _mp_pui_widget_line
#define _mp_pui_invert _mp_pui_widget_invert
#include "../widgets.c"
#undef _mp_pui_line
#undef _mp_pui_invert

#include "../pages/about.c"

#define PREVIEW_W 320
#define PREVIEW_H 240

static int preview_frame = 75;

static void preview_keep(void) {
	if(preview_frame < 0)
		_mp_pui_about_tick(NULL);
}

static void preview_ppm(const char *path, const unsigned char *rgb) {
	FILE *out = fopen(path, "wb");

	if(!out) {
		perror(path);
		exit(1);
	}
	fprintf(out, "P6\n%d %d\n255\n", PREVIEW_W, PREVIEW_H);
	if(fwrite(rgb, 1, (size_t)PREVIEW_W * PREVIEW_H * 3, out) != (size_t)PREVIEW_W * PREVIEW_H * 3) {
		perror(path);
		exit(1);
	}
	fclose(out);
}

int main(int argc, char **argv) {
	mp_pui_t ui;
	mp_pui_display_t display;
	mp_pui_page_t page;
	mp_kernel_t kernel;
	unsigned char *rgb;
	unsigned char *row;
	const char *path = argc > 1 ? argv[1] : "/tmp/pui-splash.ppm";
	int y;

	if(argc > 2)
		preview_frame = atoi(argv[2]);
	memset(&ui, 0, sizeof(ui));
	memset(&display, 0, sizeof(display));
	memset(&page, 0, sizeof(page));
	memset(&kernel, 0, sizeof(kernel));
	kernel.version = "2.0.0";
	display.width = PREVIEW_W;
	display.height = PREVIEW_H;
	display.bpp = 24;
	ui.display = &display;
	ui.kernel = &kernel;
	page.ui = &ui;
	_mp_pui_about_build(&page, NULL);
	_mp_pui_about_frame = preview_frame;
	rgb = calloc((size_t)PREVIEW_W * PREVIEW_H * 3, 1);
	row = calloc((size_t)PREVIEW_W * 3, 1);
	if(!rgb || !row)
		return(1);
	for(y = 0; y < PREVIEW_H; y++) {
		memset(row, 0, (size_t)PREVIEW_W * 3);
		_mp_pui_about_paint(&page, row, PREVIEW_W, 24, y);
		memcpy(rgb + (size_t)y * PREVIEW_W * 3, row, (size_t)PREVIEW_W * 3);
	}
	preview_ppm(path, rgb);
	printf("frame %d logo %d,%d %dx%d block %d,%d %dx%d rule %d,%d %dx%d ver y %d h %d\n",
		preview_frame,
		_mp_pui_about_logoX, _mp_pui_about_logoY, MP_PUI_LOGO_W, MP_PUI_LOGO_H,
		_mp_pui_about_logoX, _mp_pui_about_logoY, _mp_pui_about_logoW, _mp_pui_about_logoH,
		_mp_pui_about_ruleX, _mp_pui_about_ruleY, _mp_pui_about_ruleW, _mp_pui_about_ruleH,
		_mp_pui_about_verY, _mp_pui_about_verH);
	printf("flags y %d h %d swiss %d,%d %d eu %d %dx%d textW %d gap %d\n",
		_mp_pui_about_flagY, _mp_pui_about_flagH,
		_mp_pui_about_swissX, _mp_pui_about_flagY + _mp_pui_about_flagH - _mp_pui_about_swissH,
		_mp_pui_about_swissH,
		_mp_pui_about_euX, _mp_pui_about_euW, _mp_pui_about_flagH,
		mp_pui_text_width(&mp_pui_font_wordmark, "miniPhi"), MP_PUI_DP(4));
	printf("word size %d line %d baseline %d\n",
		mp_pui_font_wordmark.size, mp_pui_font_wordmark.lineHeight, mp_pui_font_wordmark.baseline);
	preview_keep();
	free(row);
	free(rgb);
	return(0);
}
