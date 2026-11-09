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

#ifndef _HAVE_MP_PUI_H
	#define _HAVE_MP_PUI_H

	#include <pui/pui_font.h>

	/**
	 * @defgroup mpCommonPUI Pager UI
	 * @ingroup mpCommon
	 *
	 * PUI is a page-scrolled UI for a small screen. Widgets speak RGB888.
	 * The display names the band depth in bpp. Pages form a tree by path.
	 * The first page registered is home. Each mp_pui_step() draws one
	 * pixel row into a small band and hands it to the display. There is
	 * one buffer, the band.
	 * @{
	 */

	#ifndef MP_PUI_TEXT
		#define MP_PUI_TEXT 24
	#endif

	#ifndef MP_PUI_LIST_MAX
		#define MP_PUI_LIST_MAX 8
	#endif

	#ifndef MP_PUI_FOLDER_MAX
		#define MP_PUI_FOLDER_MAX 8
	#endif

	#ifndef MP_PUI_PATH
		#define MP_PUI_PATH 64
	#endif

	/** keys held until the PUI task drains them */
	#ifndef MP_PUI_KEY_QUEUE
		#define MP_PUI_KEY_QUEUE 8
	#endif

	/** milliseconds on a page before the tour advances, until a key arrives */
	#ifndef MP_PUI_ROTATE_MS
		#define MP_PUI_ROTATE_MS 5000
	#endif
	/** milliseconds the tour pauses on the current page after a key press */
	#ifndef MP_PUI_HOLD_MS
		#define MP_PUI_HOLD_MS 30000
	#endif

	#define MP_PUI_ALIGN_LEFT 0
	#define MP_PUI_ALIGN_CENTER 1
	#define MP_PUI_ALIGN_RIGHT 2

	/** RGB888 source color, stored 0x00RRGGBB. The band packs it at display->bpp. */
	typedef uint32_t mp_pui_color_t;

	/** white, the constructor foreground */
	#define MP_PUI_COLOR_ON ((mp_pui_color_t)0x00FFFFFFu)
	/** black, the cleared row and the constructor background */
	#define MP_PUI_COLOR_OFF ((mp_pui_color_t)0x00000000u)

	/**
	 * 1 when ((r * 299 + g * 587 + b * 114) >> 10) is above zero.
	 * bpp 1 stores that as the pixel bit.
	 */
	int mp_pui_color_lit(mp_pui_color_t color);

	typedef struct mp_pui_display_s mp_pui_display_t;

	/**
	 * What the panel provides. bpp is 1, 16, or 24 and packs one row,
	 * row-major. At bpp 1 each byte holds eight pixels, MSB first, and a
	 * lit pixel is bit 1. At bpp 16 each pixel is RGB565, low byte first.
	 * At bpp 24 each pixel is R, then G, then B. x, y is the top left.
	 * y grows downward. write receives that row.
	 */
	struct mp_pui_display_s {
		int width;
		int height;
		int bpp;
		void (*write)(mp_pui_display_t *display, int x, int y, int w, int h, const unsigned char *pixels);
		void (*flush)(mp_pui_display_t *display);
		void (*power)(mp_pui_display_t *display, int on);
		void (*backlight)(mp_pui_display_t *display, int pct);
		void *user;
	};

typedef enum {
	MP_PUI_KEY_NEXT,
	MP_PUI_KEY_PREV,
} mp_pui_key_t;

	typedef enum {
		MP_PUI_KIND_LABEL = 1,
		MP_PUI_KIND_VALUE,
		MP_PUI_KIND_BAR,
		MP_PUI_KIND_LIST,
		MP_PUI_KIND_ICON,
		MP_PUI_KIND_BADGE,
		MP_PUI_KIND_BIG,
	} mp_pui_kind_t;

	typedef struct mp_pui_s mp_pui_t;
	typedef struct mp_pui_page_s mp_pui_page_t;
	typedef struct mp_pui_widget_s mp_pui_widget_t;

	/** called on open to fill the page with widgets */
	typedef void (*mp_pui_build_t)(mp_pui_page_t *page, void *user);

	struct mp_pui_widget_s {
		mp_list_item_t item;
		mp_pui_page_t *page;
		mp_pui_kind_t kind;
		int x, y, w, h;
	};

	typedef struct {
		mp_pui_widget_t widget;
		char text[MP_PUI_TEXT];
		int align;
		/** foreground, MP_PUI_COLOR_ON from the constructor */
		mp_pui_color_t fg;
	} mp_pui_label_t;

	typedef struct {
		mp_pui_widget_t widget;
		char caption[MP_PUI_TEXT];
		char value[MP_PUI_TEXT];
		/** foreground, MP_PUI_COLOR_ON from the constructor */
		mp_pui_color_t fg;
	} mp_pui_value_t;

	typedef struct {
		mp_pui_widget_t widget;
		int value;
		int max;
		/** fill and frame, MP_PUI_COLOR_ON from the constructor */
		mp_pui_color_t fg;
		/** empty interior, MP_PUI_COLOR_OFF from the constructor */
		mp_pui_color_t bg;
	} mp_pui_bar_t;

	typedef struct {
		mp_pui_widget_t widget;
		char text[MP_PUI_LIST_MAX][MP_PUI_TEXT];
		int count;
		int focus;
	} mp_pui_list_t;

	typedef struct {
		mp_pui_widget_t widget;
		const unsigned char *bits;
		int bw, bh;
		/** foreground, MP_PUI_COLOR_ON from the constructor */
		mp_pui_color_t fg;
	} mp_pui_icon_t;

	typedef struct {
		mp_pui_widget_t widget;
		char text[MP_PUI_TEXT];
		/** pill fill, MP_PUI_COLOR_ON from the constructor */
		mp_pui_color_t fg;
		/** glyph holes, MP_PUI_COLOR_OFF from the constructor */
		mp_pui_color_t bg;
	} mp_pui_badge_t;

	typedef struct {
		mp_pui_widget_t widget;
		char text[MP_PUI_TEXT];
		/** NULL draws with the title font */
		const mp_pui_font_t *font;
		/** foreground, MP_PUI_COLOR_ON from the constructor */
		mp_pui_color_t fg;
	} mp_pui_big_t;

struct mp_pui_page_s {
	mp_list_item_t item;
	const char *path;
	mp_pui_build_t build;
	/** optional per-row painter; when set, the page draws itself instead of its widgets */
	void (*paint)(mp_pui_page_t *page, unsigned char *buffer, int bufW, int bpp, int y);
	void *user;
		int hidden;
		mp_pui_page_t *parent;
		mp_pui_t *ui;
		mp_list_t widgets;
		int scroll;
		int focus;
		/** last path component, inside path or storage */
		const char *name;
		int synthetic;
		char storage[MP_PUI_PATH];
	};

	struct mp_pui_s {
		mp_pui_display_t *display;
		mp_list_t pages;
		mp_pui_page_t *current;
		int scroll;
		int focus;
		unsigned char *buffer;
		unsigned int bufferSize;
		mp_task_t *task;
		mp_kernel_t *kernel;
		/** next scanline, while pending */
		int row;
		int pending;
		/** dirty row range that still needs repainting, inclusive */
		int dirty0;
		int dirty1;
		/** set by the first mp_pui_key(); the tour pauses until holdUntil */
		int keyed;
		unsigned long rotateAt;
		/** ticks before which the tour stays on the current page after a key */
		unsigned long holdUntil;
		/** keys queued from another task, drained by the PUI task */
		volatile unsigned char keyHead;
		volatile unsigned char keyTail;
		mp_pui_key_t keyQueue[MP_PUI_KEY_QUEUE];
		mp_pui_page_t folder[MP_PUI_FOLDER_MAX];
		unsigned char folderUsed[MP_PUI_FOLDER_MAX];
	};

	/** bufferSize is in bytes and holds one row at display->bpp. Returns 1 on success. */
	int mp_pui_init(mp_pui_t *ui, mp_pui_display_t *display, unsigned char *buffer, unsigned int bufferSize);
	void mp_pui_fini(mp_pui_t *ui);

	/**
	 * path is "System/Info". Missing parents are created.
	 * FALSE when the path or the page is missing, or the path is already registered.
	 */
	mp_ret_t mp_pui_page_register(mp_pui_t *ui, mp_pui_page_t *page, const char *path, mp_pui_build_t build, void *user);
	/**
	 * Built-in pages. Each one stays off the tour until the app turns it on.
	 * Register the about page first: it becomes home, and the others hang under it.
	 * FALSE when the page is already on, or when it cannot be registered.
	 */
	mp_ret_t mp_pui_page_about(mp_pui_t *ui);
/** play the boot rainbow splash on the about page; call after mp_pui_attach */
void mp_pui_page_about_start(mp_pui_t *ui);
	/** chip name, clock, and a live RAM bar, at miniPhi/CPU */
	mp_ret_t mp_pui_page_cpu(mp_pui_t *ui);
	/** the first temperature sensor, at miniPhi/Temp: value, unit, gauge, band */
	mp_ret_t mp_pui_page_temp(mp_pui_t *ui);
	/** the first clock, at miniPhi/Clock: date, time, running badge */
	mp_ret_t mp_pui_page_clock(mp_pui_t *ui);
	/** the first charger, at miniPhi/Charge: battery glyph and cycle state */
	mp_ret_t mp_pui_page_charge(mp_pui_t *ui);
	/** the first voltage sensor, at miniPhi/Volt: reading and a 0..5 V gauge */
	mp_ret_t mp_pui_page_volt(mp_pui_t *ui);
	/** the first other measure, at miniPhi/Measure: a gauge, or three axes */
	mp_ret_t mp_pui_page_measure(mp_pui_t *ui);
	/** one page per measure device except temperature, which has its own page */
	void mp_pui_page_faces(mp_pui_t *ui);
	/** add a page for a device that arrived, drop one that left */
	void mp_pui_page_faces_sync(mp_pui_t *ui);
	/** refresh the device page that is on the glass */
	void mp_pui_page_faces_refresh(mp_pui_t *ui);
	/** refresh the built-in page that is on the glass; the app calls this from its tick */
	void mp_pui_pages_refresh(mp_pui_t *ui);
	/** refresh the temperature page when it is current */
	void mp_pui_page_temp_refresh(mp_pui_t *ui);
	/** refresh the clock page when it is current */
	void mp_pui_page_clock_refresh(mp_pui_t *ui);
	/** refresh the charge page when it is current */
	void mp_pui_page_charge_refresh(mp_pui_t *ui);
	/** refresh the voltage page when it is current */
	void mp_pui_page_volt_refresh(mp_pui_t *ui);
	/** refresh the measure page when it is current */
	void mp_pui_page_measure_refresh(mp_pui_t *ui);
	/** the temperature page, so the app can show or hide it */
	mp_pui_page_t *mp_pui_page_temp_page(void);
	/** 1 when a temperature device is in the registry */
	int mp_pui_page_temp_present(mp_kernel_t *kernel);
	/** the clock page, so the app can show or hide it */
	mp_pui_page_t *mp_pui_page_clock_page(void);
	/** 1 when a clock device is in the registry */
	int mp_pui_page_clock_present(mp_kernel_t *kernel);
	/** the charge page, so the app can show or hide it */
	mp_pui_page_t *mp_pui_page_charge_page(void);
	/** 1 when a charger device is in the registry */
	int mp_pui_page_charge_present(mp_kernel_t *kernel);
	/** the voltage page, so the app can show or hide it */
	mp_pui_page_t *mp_pui_page_volt_page(void);
	/** 1 when a voltage device is in the registry */
	int mp_pui_page_volt_present(mp_kernel_t *kernel);
	/** the measure page, so the app can show or hide it */
	mp_pui_page_t *mp_pui_page_measure_page(void);
	/** 1 when a measure other than temperature or voltage is in the registry */
	int mp_pui_page_measure_present(mp_kernel_t *kernel);
	void mp_pui_page_unregister(mp_pui_t *ui, mp_pui_page_t *page);
	/** on is non-zero to show the page */
	void mp_pui_page_show(mp_pui_page_t *page, int on);
	void mp_pui_page_refresh(mp_pui_page_t *page);

	void mp_pui_key(mp_pui_t *ui, mp_pui_key_t key);
	/** drain the queued keys; called from the PUI task */
	void mp_pui_pump_keys(mp_pui_t *ui);

	/** draws one band; returns non-zero while a row is still waiting */
	int mp_pui_step(mp_pui_t *ui);
	int mp_pui_pending(mp_pui_t *ui);
	void mp_pui_refresh(mp_pui_t *ui);
	/** mark rows y0..y1 (inclusive) for repaint; the whole glass when they span it */
	void mp_pui_refresh_rows(mp_pui_t *ui, int y0, int y1);

	/** ticks until the tour moves, or 0 while it sleeps for a key */
	unsigned long mp_pui_idle(mp_pui_t *ui);

	int mp_pui_attach(mp_kernel_t *kernel, mp_pui_t *ui, char *who);
	void mp_pui_detach(mp_pui_t *ui);

	void mp_pui_page_add(mp_pui_page_t *page, mp_pui_widget_t *widget);

	void mp_pui_label(mp_pui_label_t *label, mp_pui_page_t *page, int x, int y, int w, int h, const char *text);
	void mp_pui_label_set(mp_pui_label_t *label, const char *text);
	void mp_pui_label_align(mp_pui_label_t *label, int align);
	void mp_pui_label_color(mp_pui_label_t *label, mp_pui_color_t fg);

	void mp_pui_value(mp_pui_value_t *value, mp_pui_page_t *page, int x, int y, int w, int h, const char *caption, const char *text);
	void mp_pui_value_set(mp_pui_value_t *value, const char *text);
	void mp_pui_value_color(mp_pui_value_t *value, mp_pui_color_t fg);

	void mp_pui_bar(mp_pui_bar_t *bar, mp_pui_page_t *page, int x, int y, int w, int h, int max);
	void mp_pui_bar_set(mp_pui_bar_t *bar, int value);
	void mp_pui_bar_color(mp_pui_bar_t *bar, mp_pui_color_t fg, mp_pui_color_t bg);

	void mp_pui_list(mp_pui_list_t *list, mp_pui_page_t *page, int x, int y, int w, int h);
	void mp_pui_list_add(mp_pui_list_t *list, const char *text);

	void mp_pui_icon(mp_pui_icon_t *icon, mp_pui_page_t *page, int x, int y, int w, int h);
	/** bits are 1 bit per pixel, row-major, MSB first, stride (bw + 7) / 8. A set bit takes the icon foreground. */
	void mp_pui_icon_set(mp_pui_icon_t *icon, const unsigned char *bits, int bw, int bh);
	void mp_pui_icon_color(mp_pui_icon_t *icon, mp_pui_color_t fg);

	void mp_pui_badge(mp_pui_badge_t *badge, mp_pui_page_t *page, int x, int y, int w, int h, const char *text);
	void mp_pui_badge_set(mp_pui_badge_t *badge, const char *text);
	void mp_pui_badge_color(mp_pui_badge_t *badge, mp_pui_color_t fg, mp_pui_color_t bg);

	void mp_pui_big(mp_pui_big_t *big, mp_pui_page_t *page, int x, int y, int w, int h, const char *text);
	void mp_pui_big_set(mp_pui_big_t *big, const char *text);
	/** font NULL returns to the title font */
	void mp_pui_big_font(mp_pui_big_t *big, const mp_pui_font_t *font);
	void mp_pui_big_color(mp_pui_big_t *big, mp_pui_color_t fg);

	/** bytes in one row at this width and bpp */
	int mp_pui_stride(int width, int bpp);

	/**
	 * Writes color at x, y. bpp 1 stores a bit, MSB first, lit when
	 * mp_pui_color_lit is 1. bpp 16 stores RGB565, low byte first.
	 * bpp 24 stores R, G, B.
	 */
	void mp_pui_put(unsigned char *buffer, int bufW, int bpp, int x, int y, mp_pui_color_t color);
	/** The pixel at x, y. bpp 1 returns MP_PUI_COLOR_ON or MP_PUI_COLOR_OFF. */
	mp_pui_color_t mp_pui_get(const unsigned char *buffer, int bufW, int bpp, int x, int y);

	/**
	 * Draws text into the band. y is the top of the line.
	 * The bitmap holds rows y through y + lineHeight - 1, packed at bpp.
	 * A set glyph bit is written in color.
	 */
	void mp_pui_draw_text(unsigned char *buffer, int bufW, int bpp, int x, int y, const mp_pui_font_t *font, const char *text, mp_pui_color_t color);

	/**
	 * One scanline of a text line into the band. lineRow is 0 at the top
	 * of the line. right is the first column past the run. A set glyph
	 * bit is written in color.
	 */
	void mp_pui_paint_text(unsigned char *buffer, int bufW, int bpp, int x, int right, int destY, int lineRow, const mp_pui_font_t *font, const char *text, mp_pui_color_t color);

	int mp_pui_widget_rows(mp_pui_widget_t *widget);
	void mp_pui_widget_focus(mp_pui_widget_t *widget, int row);
	const char *mp_pui_widget_row(mp_pui_widget_t *widget, int row);
	void mp_pui_widget_draw(mp_pui_widget_t *widget, unsigned char *buffer, int bufW, int bpp, int local);

	/** @} */

#endif
