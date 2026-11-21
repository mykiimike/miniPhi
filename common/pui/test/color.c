/* Host test of PUI band packing. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>

#define SUPPORT_COMMON_PUI

typedef struct mp_task_s mp_task_t;

#include <mp.h>
#include <pui/pui.h>

const mp_pui_font_t mp_pui_font_default = { "host", 8, 1, 8, 6, 0, NULL, NULL };
const mp_pui_font_t mp_pui_font_bold = { "host", 8, 1, 8, 6, 0, NULL, NULL };

void mp_pui_detach(mp_pui_t *ui) {
	(void)ui;
}

void mp_pui_page_glass_release(mp_pui_t *ui) {
	(void)ui;
}

#include "../pui_font.c"
#include "../pui.c"

/* Both files keep a private helper under these names. This test compiles
 * them as one unit, so the widget copies take a distinct name here. */
#define _mp_pui_line _mp_pui_widget_line
#define _mp_pui_invert _mp_pui_widget_invert
#include "../widgets.c"
#undef _mp_pui_line
#undef _mp_pui_invert

static int failed;
static unsigned char got[32];
static int gotN;
static int writes;
static mp_pui_bar_t bar;
static mp_pui_color_t ink;

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "fail: %s\n", msg);
}

static void capture(mp_pui_display_t *display, int x, int y, int w, int h, const unsigned char *pixels) {
	int n = mp_pui_stride(w, display->bpp);

	(void)x;
	(void)y;
	(void)h;
	writes++;
	gotN = n;
	memset(got, 0, sizeof(got));
	if(n > 0 && n <= (int)sizeof(got) && pixels)
		memcpy(got, pixels, (size_t)n);
}

static void build(mp_pui_page_t *page, void *user) {
	(void)user;
	mp_pui_bar(&bar, page, 3, 0, 1, 1, 1);
	expect(bar.fg == MP_PUI_COLOR_ON && bar.bg == MP_PUI_COLOR_OFF, "bar starts white on black");
	bar.fg = ink;
	mp_pui_bar_set(&bar, 1);
}

/* one row, one lit pixel at column 3, shipped through the display write */
static void ship(int bpp, mp_pui_color_t color, const unsigned char *want, int n, const char *msg) {
	mp_pui_t ui;
	mp_pui_display_t display;
	mp_pui_page_t page;
	unsigned char band[32];

	memset(&display, 0, sizeof(display));
	display.width = 8;
	display.height = 1;
	display.bpp = bpp;
	display.write = capture;
	ink = color;
	writes = 0;
	gotN = -1;
	if(!mp_pui_init(&ui, &display, band, sizeof(band))) {
		expect(0, "init");
		return;
	}
	if(mp_pui_page_register(&ui, &page, "pixel", build, NULL) != TRUE) {
		expect(0, "page");
		return;
	}
	expect(mp_pui_step(&ui) == 0, "one row finishes the glass");
	expect(writes == 1 && gotN == n && memcmp(got, want, (size_t)n) == 0, msg);
}

static void pack(void) {
	unsigned char row[24];
	mp_pui_display_t display;
	mp_pui_t ui;
	unsigned char band[384];

	memset(&display, 0, sizeof(display));
	display.width = 16;
	display.height = 1;
	display.write = capture;
	display.bpp = 1;
	memset(row, 0, sizeof(row));
	mp_pui_put(row, display.width, display.bpp, 0, 0, MP_PUI_COLOR_ON);
	mp_pui_put(row, display.width, display.bpp, 9, 0, 0x00FF0000);
	expect(row[0] == 0x80 && row[1] == 0x40, "msb and a red pixel are lit");
	expect(mp_pui_get(row, display.width, display.bpp, 0, 0) == MP_PUI_COLOR_ON, "get lit");
	mp_pui_put(row, display.width, display.bpp, 0, 0, MP_PUI_COLOR_OFF);
	expect(row[0] == 0x00 && mp_pui_get(row, display.width, display.bpp, 0, 0) == MP_PUI_COLOR_OFF, "black clears");
	row[0] = 0xFF;
	mp_pui_put(row, display.width, display.bpp, 0, 0, 0x00010101);
	expect((row[0] & 0x80) == 0, "luminance at zero stays off");
	mp_pui_put(row, display.width, display.bpp, 1, 0, 0x00000400);
	expect((row[0] & 0x40) != 0, "luminance above zero is on");

	display.bpp = 16;
	memset(row, 0, sizeof(row));
	mp_pui_put(row, 8, display.bpp, 1, 0, 0x00FF0000);
	mp_pui_put(row, 8, display.bpp, 0, 0, MP_PUI_COLOR_ON);
	expect(row[0] == 0xFF && row[1] == 0xFF, "rgb565 white");
	expect(row[2] == 0x00 && row[3] == 0xF8, "rgb565 red");
	expect(mp_pui_get(row, 8, display.bpp, 0, 0) == MP_PUI_COLOR_ON, "get white");
	expect(mp_pui_get(row, 8, display.bpp, 1, 0) == 0x00FF0000, "get red");
	mp_pui_put(row, 4, display.bpp, 0, 1, MP_PUI_COLOR_ON);
	expect(row[8] == 0xFF && row[9] == 0xFF, "rgb565 next row");

	display.bpp = 24;
	mp_pui_put(row, 4, display.bpp, 1, 0, 0x00AABBCC);
	expect(row[3] == 0xAA && row[4] == 0xBB && row[5] == 0xCC, "rgb888");
	expect(mp_pui_get(row, 4, display.bpp, 1, 0) == 0x00AABBCC, "get rgb888");

	display.width = 128;
	display.bpp = 1;
	expect(mp_pui_stride(128, 1) == 16, "1-bit stride");
	expect(mp_pui_init(&ui, &display, band, 16) == 1, "1-bit row");
	expect(mp_pui_init(&ui, &display, band, 15) == 0, "short 1-bit row");
	display.bpp = 16;
	expect(mp_pui_stride(128, 16) == 256, "rgb565 stride");
	expect(mp_pui_init(&ui, &display, band, 256) == 1, "rgb565 row");
	expect(mp_pui_init(&ui, &display, band, 255) == 0, "short rgb565 row");
	display.bpp = 24;
	expect(mp_pui_stride(128, 24) == 384, "rgb888 stride");
	expect(mp_pui_init(&ui, &display, band, 384) == 1, "rgb888 row");
	display.bpp = 0;
	expect(mp_pui_init(&ui, &display, band, sizeof(band)) == 0, "missing bpp");
}

int main(void) {
	static const unsigned char mono[1] = { 0x10 };
	static const unsigned char red[16] = {
		0, 0, 0, 0, 0, 0, 0x00, 0xF8,
		0, 0, 0, 0, 0, 0, 0, 0
	};
	static const unsigned char white[16] = {
		0, 0, 0, 0, 0, 0, 0xFF, 0xFF,
		0, 0, 0, 0, 0, 0, 0, 0
	};

	pack();
	ship(1, MP_PUI_COLOR_ON, mono, 1, "1-bit band");
	ship(1, 0x00FF0000, mono, 1, "1-bit band from red");
	ship(16, 0x00FF0000, red, 16, "rgb565 band");
	ship(16, MP_PUI_COLOR_ON, white, 16, "rgb565 white band");

	if(failed) {
		fprintf(stderr, "%d failed\n", failed);
		return(1);
	}
	printf("ok\n");
	return(0);
}
