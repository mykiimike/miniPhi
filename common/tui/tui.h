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

#ifndef _HAVE_MP_TUI_H
	#define _HAVE_MP_TUI_H

	#include <stdint.h>
	#include <tui/font.h>

	/**
	 * @defgroup mpCommonTUI Things UI
	 * @ingroup mpCommon
	 *
	 * TUI, Things UI, is a user interface for small screens. When a screen
	 * starts, it shows miniPhi, the logo, the kernel version, the copyright
	 * and the license together, then a home of applications declared with
	 * mp_tui_app(). Opening one shows a scrollable page and a Back button
	 * that returns to the home.
	 * The tree is drawn into any display through one callback. There is no
	 * framebuffer: changes mark dirty rectangles, and each mp_tui_step()
	 * draws one band of a dirty rectangle into a small buffer, then hands
	 * it to the display. Every mp_tui_t is independent, so a firmware can
	 * drive several screens. Things are allocated by the caller and must
	 * outlive the screen they are on.
	 * @{
	 */

	#ifndef MP_TUI_TEXT_MAX
		#define MP_TUI_TEXT_MAX 32
	#endif

	#ifndef MP_TUI_DIRTY_MAX
		#define MP_TUI_DIRTY_MAX 4
	#endif

	#ifndef MP_TUI_APP_MAX
		#define MP_TUI_APP_MAX 8
	#endif

	/** RGB565 */
	typedef uint16_t mp_tui_color_t;

	#define MP_TUI_RGB(r, g, b) ((mp_tui_color_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xFF) >> 3)))

	/** RGB565 image; alpha is 4 bits per pixel, high nibble first, 0 is transparent */
	typedef struct {
		uint16_t w, h;
		const mp_tui_color_t *pixels;
		const uint8_t *alpha;
	} mp_tui_bitmap_t;

	/** splash logo, rasterized from config/assets/logo.svg */
	extern const mp_tui_bitmap_t mp_tui_logo;

	typedef struct {
		int x, y, w, h;
	} mp_tui_rect_t;

	typedef struct mp_tui_s mp_tui_t;
	typedef struct mp_tui_display_s mp_tui_display_t;
	typedef struct mp_tui_widget_s mp_tui_widget_t;
	typedef struct mp_tui_draw_s mp_tui_draw_t;

	/**
	 * What an LCD driver provides. write() receives w * h pixels, row
	 * after row, for the window at x, y. A monochrome or BGR panel
	 * converts them there.
	 */
	struct mp_tui_display_s {
		int width;
		int height;
		void (*write)(mp_tui_display_t *display, int x, int y, int w, int h, const mp_tui_color_t *pixels);
		/** optional, once every dirty rectangle is drawn */
		void (*flush)(mp_tui_display_t *display);
		void *user;
	};

	typedef struct {
		mp_tui_color_t background;
		mp_tui_color_t surface;
		mp_tui_color_t foreground;
		mp_tui_color_t muted;
		mp_tui_color_t accent;
		mp_tui_color_t border;
		mp_tui_color_t titleBackground;
		mp_tui_color_t titleForeground;
		const mp_tui_font_t *font;
	} mp_tui_theme_t;

	typedef enum {
		/** pressed and released on the widget */
		MP_TUI_EVENT_CLICK = 1,
		/** another widget was dragged and released on this one; source is it */
		MP_TUI_EVENT_DROP,
		/** value wheel steps, positive away from the user */
		MP_TUI_EVENT_WHEEL,
	} mp_tui_event_type_t;

	/** what a widget receives; it goes up to the parents until one takes it */
	typedef struct {
		mp_tui_event_type_t type;
		/** on the display */
		int x, y;
		int value;
		mp_tui_widget_t *source;
	} mp_tui_event_t;

	/** a widget type: a custom widget fills one and passes it to mp_tui_widget_init() */
	typedef struct {
		const char *name;
		/** draws inside widget->screen, through the mp_tui_draw_* calls */
		void (*draw)(mp_tui_widget_t *widget, mp_tui_draw_t *draw);
		/** optional: the area children are placed in, relative to the widget */
		void (*client)(mp_tui_widget_t *widget, mp_tui_rect_t *rect);
		/** optional: returns non-zero when the widget took the event */
		int (*event)(mp_tui_widget_t *widget, const mp_tui_event_t *event);
	} mp_tui_class_t;

	/** follows the pointer while dragged; the widget under the drop point gets MP_TUI_EVENT_DROP */
	#define MP_TUI_DRAGGABLE 0x01

	struct mp_tui_widget_s {
		const mp_tui_class_t *type;
		mp_tui_t *tui;
		mp_tui_widget_t *parent;
		mp_tui_widget_t *first;
		mp_tui_widget_t *last;
		mp_tui_widget_t *prev;
		mp_tui_widget_t *next;

		/** for mp_tui_find(), may be NULL */
		const char *name;

		/** position in the client area of the parent */
		mp_tui_rect_t rect;

		/** position on the display, kept up to date */
		mp_tui_rect_t screen;

		int hidden;
		/** MP_TUI_DRAGGABLE */
		int flags;
	};

	typedef enum {
		MP_TUI_ALIGN_LEFT,
		MP_TUI_ALIGN_CENTER,
		MP_TUI_ALIGN_RIGHT,
	} mp_tui_align_t;

	/** text over its parent, '\n' starts a line */
	typedef struct {
		mp_tui_widget_t widget;
		const mp_tui_font_t *font;
		mp_tui_color_t color;
		mp_tui_align_t align;
		char text[MP_TUI_TEXT_MAX];
	} mp_tui_label_t;

	typedef struct mp_tui_button_s mp_tui_button_t;

	/** called when the button is pressed; button->user is the pointer given by the firmware */
	typedef void (*mp_tui_button_on_t)(mp_tui_button_t *button);

	/** filled control; chevron draws a ">" on the right, selected draws the accent */
	struct mp_tui_button_s {
		mp_tui_widget_t widget;
		char text[MP_TUI_TEXT_MAX];
		mp_tui_button_on_t onPress;
		void *user;
		int id;
		int selected;
		int chevron;
		/** text only, no fill and no frame; for a control sitting in a bar */
		int plain;
		/** held down by a pointer */
		int pressed;
	};

	/** children can sit below the visible area; mp_tui_scroll() moves them */
	typedef struct {
		mp_tui_widget_t widget;
		int scroll;
		/** movement received while a band is on screen; applied when that rectangle finishes */
		int pending;
	} mp_tui_view_t;

	/** full-screen page with a header bar of `header` pixels */
	typedef struct {
		mp_tui_widget_t widget;
		int header;
	} mp_tui_page_t;

	/** fills the scrollable page of one application; the name must outlive the screen */
	typedef void (*mp_tui_app_build_t)(mp_tui_widget_t *page, void *user);

	/** clip and destination of the band being drawn */
	struct mp_tui_draw_s {
		mp_tui_t *tui;
		mp_tui_color_t *pixels;
		mp_tui_rect_t band;
		mp_tui_rect_t clip;
	};

	struct mp_tui_s {
		mp_tui_display_t *display;
		mp_tui_theme_t theme;
		mp_tui_widget_t root;

		mp_tui_color_t *buffer;
		unsigned int bufferSize;

		mp_tui_rect_t dirty[MP_TUI_DIRTY_MAX];
		int dirtyCount;

		/** the rectangle being drawn, and the next row of it */
		mp_tui_rect_t current;
		int currentRow;
		int drawing;

		/** set by mp_tui_attach() */
		struct mp_task_s *task;

		/** 0 while the splash is up, 3 once the screen itself is drawn */
		int splash;
		int splashHold;
		int splashRow;
		unsigned long splashUntil;

		/** home of applications; shell is set by the first mp_tui_app() */
		int shell;
		int appCount;
		int focus;
		/** index of the open application, or -1 on the home */
		int opened;
		struct {
			const char *name;
			mp_tui_app_build_t build;
			void *user;
		} app[MP_TUI_APP_MAX];
		mp_tui_label_t homeTitle;
		mp_tui_button_t homeButton[MP_TUI_APP_MAX];
		mp_tui_page_t page;
		mp_tui_button_t back;
		mp_tui_label_t pageTitle;
		mp_tui_view_t view;

		/** set by mp_tui_attach(); its inputs are read on every tick */
		struct mp_kernel_s *kernel;
		/** drawn over everything while a mouse is used */
		int cursorShown;
		int cursorX, cursorY;
		mp_tui_button_t *pressed;
		mp_tui_widget_t *dragged;
		mp_tui_view_t *dragView;
	};

	/* screen */

	/** buffer holds at least one display row: bufferSize >= display->width */
	int mp_tui_init(mp_tui_t *ui, mp_tui_display_t *display, mp_tui_color_t *buffer, unsigned int bufferSize);
	void mp_tui_fini(mp_tui_t *ui);
	mp_tui_widget_t *mp_tui_root(mp_tui_t *ui);

	/** marks the whole display dirty, for instance after a theme change */
	void mp_tui_refresh(mp_tui_t *ui);
	void mp_tui_invalidate_rect(mp_tui_t *ui, const mp_tui_rect_t *rect);
	int mp_tui_pending(mp_tui_t *ui);

	/** draws one band; returns non-zero while something is left to draw */
	int mp_tui_step(mp_tui_t *ui);

	/** draws everything that is dirty, blocking */
	void mp_tui_render(mp_tui_t *ui);

	/* widgets */

	#define MP_TUI_WIDGET(item) (&(item)->widget)

	void mp_tui_widget_init(mp_tui_widget_t *widget, const mp_tui_class_t *type, mp_tui_widget_t *parent, int x, int y, int w, int h);
	void mp_tui_widget_remove(mp_tui_widget_t *widget);
	void mp_tui_widget_name(mp_tui_widget_t *widget, const char *name);
	void mp_tui_invalidate(mp_tui_widget_t *widget);
	void mp_tui_move(mp_tui_widget_t *widget, int x, int y);
	void mp_tui_resize(mp_tui_widget_t *widget, int w, int h);
	void mp_tui_show(mp_tui_widget_t *widget);
	void mp_tui_hide(mp_tui_widget_t *widget);

	/** draws the widget above its siblings */
	void mp_tui_raise(mp_tui_widget_t *widget);

	/** first widget with that name, depth first, or NULL */
	mp_tui_widget_t *mp_tui_find(mp_tui_t *ui, const char *name);

	/** filled rectangle with an optional one pixel border */
	typedef struct {
		mp_tui_widget_t widget;
		mp_tui_color_t color;
		mp_tui_color_t borderColor;
		int border;
	} mp_tui_box_t;

	/** horizontal gauge from 0 to max */
	typedef struct {
		mp_tui_widget_t widget;
		mp_tui_color_t color;
		mp_tui_color_t background;
		int value;
		int max;
	} mp_tui_bar_t;

	/** framed panel with a title bar; children go below the title */
	typedef struct {
		mp_tui_widget_t widget;
		const mp_tui_font_t *font;
		char title[MP_TUI_TEXT_MAX];
	} mp_tui_window_t;

	extern const mp_tui_class_t mp_tui_box_class;
	extern const mp_tui_class_t mp_tui_label_class;
	extern const mp_tui_class_t mp_tui_bar_class;
	extern const mp_tui_class_t mp_tui_window_class;
	extern const mp_tui_class_t mp_tui_button_class;
	extern const mp_tui_class_t mp_tui_view_class;
	extern const mp_tui_class_t mp_tui_page_class;

	void mp_tui_box(mp_tui_box_t *box, mp_tui_widget_t *parent, int x, int y, int w, int h, mp_tui_color_t color);

	void mp_tui_label(mp_tui_label_t *label, mp_tui_widget_t *parent, int x, int y, int w, int h, const char *text);
	/** printf-like; redraws only when the text changes */
	void mp_tui_label_set(mp_tui_label_t *label, const char *format, ...);
	void mp_tui_label_font(mp_tui_label_t *label, const mp_tui_font_t *font);
	void mp_tui_label_color(mp_tui_label_t *label, mp_tui_color_t color);
	void mp_tui_label_align(mp_tui_label_t *label, mp_tui_align_t align);

	void mp_tui_bar(mp_tui_bar_t *bar, mp_tui_widget_t *parent, int x, int y, int w, int h, int max);
	void mp_tui_bar_set(mp_tui_bar_t *bar, int value);

	void mp_tui_window(mp_tui_window_t *window, mp_tui_widget_t *parent, int x, int y, int w, int h, const char *title);
	void mp_tui_window_title(mp_tui_window_t *window, const char *format, ...);

	void mp_tui_button(mp_tui_button_t *button, mp_tui_widget_t *parent, int x, int y, int w, int h, const char *text);
	void mp_tui_view(mp_tui_view_t *view, mp_tui_widget_t *parent, int x, int y, int w, int h);
	/** moves the content by dy pixels, kept between the top and the lowest child */
	void mp_tui_view_scroll(mp_tui_view_t *view, int dy);
	void mp_tui_page(mp_tui_page_t *page, mp_tui_widget_t *parent, int x, int y, int w, int h, int header);

	/**
	 * Declares an application on the home. build() fills the scrollable page
	 * each time it is opened. Returns 0 when the home is full or the name is missing.
	 */
	int mp_tui_app(mp_tui_t *ui, const char *name, mp_tui_app_build_t build, void *user);

	/** a click at x, y: the topmost widget under it, then its parents, get it */
	int mp_tui_press(mp_tui_t *ui, int x, int y);

	/** delivers an event to the topmost widget under event->x, y and up */
	int mp_tui_dispatch(mp_tui_t *ui, const mp_tui_event_t *event);

	/**
	 * Reads every input of the kernel given to mp_tui_attach() that is
	 * not bound to another reader, and applies it: click, drag of a
	 * draggable widget or of a view, drop, wheel, keys, mouse cursor.
	 * The TUI task calls it on every tick.
	 */
	void mp_tui_poll(mp_tui_t *ui);

	typedef enum {
		MP_TUI_KEY_UP,
		MP_TUI_KEY_DOWN,
		MP_TUI_KEY_ENTER,
		MP_TUI_KEY_BACK,
		MP_TUI_KEY_PAGE_UP,
		MP_TUI_KEY_PAGE_DOWN,
	} mp_tui_key_t;

	/** home: up and down move, enter opens. page: up and down scroll, back returns home */
	void mp_tui_key(mp_tui_t *ui, mp_tui_key_t key);

	/** moves the open page by dy pixels; a positive dy reveals what is below */
	void mp_tui_scroll(mp_tui_t *ui, int dy);

	/** by name, through mp_tui_find(); NO when there is no such widget of that type */
	int mp_tui_set_text(mp_tui_t *ui, const char *name, const char *format, ...);
	int mp_tui_set_value(mp_tui_t *ui, const char *name, int value);

	/* drawing, for widget draw callbacks; coordinates are on the display */

	void mp_tui_draw_fill(mp_tui_draw_t *draw, int x, int y, int w, int h, mp_tui_color_t color);
	void mp_tui_draw_frame(mp_tui_draw_t *draw, int x, int y, int w, int h, mp_tui_color_t color);
	void mp_tui_draw_hline(mp_tui_draw_t *draw, int x, int y, int w, mp_tui_color_t color);
	void mp_tui_draw_vline(mp_tui_draw_t *draw, int x, int y, int h, mp_tui_color_t color);
	/** y is the top of the line; returns the pen position after the text */
	int mp_tui_draw_text(mp_tui_draw_t *draw, const mp_tui_font_t *font, int x, int y, const char *text, mp_tui_color_t color);
	/** w and h are the destination size; the bitmap is scaled to fit */
	void mp_tui_draw_bitmap(mp_tui_draw_t *draw, const mp_tui_bitmap_t *bitmap, int x, int y, int w, int h);

	/* kernel */

	/** a task draws one band per tick while the screen is dirty */
	int mp_tui_attach(struct mp_kernel_s *kernel, mp_tui_t *ui, char *who);
	void mp_tui_detach(mp_tui_t *ui);

	/** @} */

#endif
