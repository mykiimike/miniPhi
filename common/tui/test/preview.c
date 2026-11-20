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

/* Host preview of the splash. Not linked into the firmware.
 * Compile after a TUI build has written build/include/gen:
 *   gcc -std=c99 -Wall -Wextra -O2 -I common/test -I common -I build/include \
 *       common/tui/test/preview.c -o /tmp/splash-preview
 * Draws the same 4-row bands the glass gets and writes PPM files.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SUPPORT_COMMON_TUI
#include <mp.h>
#include <tui/tui.h>

struct mp_task_s {
	unsigned long delay;
};
#include <gen/tui_font_default.h>
#include <gen/tui_wordmark.h>
#include <gen/tui_logo.h>
#include "../font.c"
#include "../draw.c"
#include "../splash.c"

#define PREVIEW_ROWS 4
#define PREVIEW_SPI_HZ 40000000.0

static mp_tui_color_t *frame;
static int glassW, glassH;
static unsigned long pixels;
static int lamp;

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

void mp_tui_refresh(mp_tui_t *ui) {
	(void)ui;
}

static void preview_write(mp_tui_display_t *display, int x, int y, int w, int h, const mp_tui_color_t *src) {
	int row, col;

	(void)display;
	if(x < 0 || y < 0 || w < 1 || h < 1 || x + w > glassW || y + h > glassH)
		return;
	for(row = 0; row < h; row++) {
		mp_tui_color_t *dst = frame + (y + row) * glassW + x;

		for(col = 0; col < w; col++)
			dst[col] = src[row * w + col];
	}
	pixels += (unsigned long)(w * h);
}

static void preview_lamp(mp_tui_display_t *display, int percent) {
	(void)display;
	lamp = percent;
}

static void preview_rgb(mp_tui_color_t color, int swap, unsigned char *r, unsigned char *g, unsigned char *b) {
	unsigned int rr = ((color >> 11) & 31) * 255 / 31;
	unsigned int gg = ((color >> 5) & 63) * 255 / 63;
	unsigned int bb = (color & 31) * 255 / 31;

	/* MADCTL BGR keeps the same 565 bits and reads red as blue */
	if(swap) {
		unsigned int tmp = rr;

		rr = bb;
		bb = tmp;
	}
	*r = (unsigned char)rr;
	*g = (unsigned char)gg;
	*b = (unsigned char)bb;
}

static void preview_ppm(const char *path, int swap) {
	FILE *out = fopen(path, "wb");
	int i;

	if(!out) {
		perror(path);
		return;
	}
	fprintf(out, "P6\n%d %d\n255\n", glassW, glassH);
	for(i = 0; i < glassW * glassH; i++) {
		unsigned char rgb[3];

		preview_rgb(frame[i], swap, &rgb[0], &rgb[1], &rgb[2]);
		fwrite(rgb, 1, 3, out);
	}
	fclose(out);
}

static void preview_shot(const char *tag, int index) {
	char path[128];

	snprintf(path, sizeof(path), "/tmp/splash-%s-f%d.ppm", tag, index);
	preview_ppm(path, 0);
	snprintf(path, sizeof(path), "/tmp/splash-%s-f%d-bgr.ppm", tag, index);
	preview_ppm(path, 1);
	fprintf(stderr, "frame %d lamp %d\n", index, lamp);
}

static void preview_run(int w, int h, const char *tag) {
	mp_tui_color_t band[320 * PREVIEW_ROWS];
	mp_tui_display_t display;
	mp_tui_t ui;
	mp_kernel_t kernel;
	struct mp_task_s task;
	int seen, guard;
	unsigned long painted = 0;

	if(w > 320) {
		fprintf(stderr, "%s wider than the band\n", tag);
		return;
	}
	glassW = w;
	glassH = h;
	frame = calloc((size_t)w * (size_t)h, sizeof(*frame));
	if(!frame)
		return;
	memset(frame, 0x00, (size_t)w * (size_t)h * sizeof(*frame));

	memset(&display, 0, sizeof(display));
	memset(&ui, 0, sizeof(ui));
	memset(&kernel, 0, sizeof(kernel));
	memset(&task, 0, sizeof(task));
	kernel.version = "2.0.0";
	display.width = w;
	display.height = h;
	display.write = preview_write;
	display.backlight = preview_lamp;
	ui.display = &display;
	ui.buffer = band;
	ui.bufferSize = (unsigned int)(w * PREVIEW_ROWS);
	ui.theme.background = MP_TUI_RGB(0x10, 0x14, 0x1C);
	ui.theme.foreground = MP_TUI_RGB(0xE8, 0xEA, 0xEE);
	ui.theme.muted = MP_TUI_RGB(0x8A, 0x92, 0xA0);
	ui.theme.accent = MP_TUI_RGB(0x2E, 0xB8, 0x8A);
	ui.theme.font = &mp_tui_font_default;
	ui.kernel = &kernel;
	ui.task = &task;
	lamp = 0;
	pixels = 0;
	seen = -1;

	for(guard = 0; guard < 100000 && ui.splash < 3 && !ui.splashHold; guard++) {
		int before = ui.splashFrame;

		mp_tui_splash_band(&ui);
		if(ui.splashFrame != before) {
			int done = before;

			if(done == 0 || done == 6 || done == MP_TUI_SPLASH_LOGO / 2
				|| done == MP_TUI_SPLASH_LOGO - 4
				|| done == MP_TUI_SPLASH_LOGO + MP_TUI_SPLASH_TEXT / 2
				|| done == MP_TUI_SPLASH_LOGO + MP_TUI_SPLASH_TEXT + MP_TUI_SPLASH_FLAG / 2
				|| done == MP_TUI_SPLASH_FRAMES - 1)
				preview_shot(tag, done);
			painted += pixels;
			pixels = 0;
			seen = done;
		}
	}
	fprintf(stderr, "%s %dx%d frames %d pixels %lu spi-equiv %.1f ms\n",
		tag, w, h, seen + 1, painted, (double)painted * 2.0 * 8.0 / PREVIEW_SPI_HZ * 1000.0);
	free(frame);
	frame = NULL;
}

int main(void) {
	preview_run(320, 240, "320x240");
	preview_run(240, 320, "240x320");
	preview_run(172, 320, "172x320");
	return(0);
}
