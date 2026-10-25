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
	 * drive several screens. mp_tui_fullscreen() covers one of them with
	 * an image scaled to fit and a close cross, reading the caller's
	 * pixels while those bands are drawn. Things are allocated by the
	 * caller and must outlive the screen they are on.
	 * @{
	 */

	#ifndef MP_TUI_TEXT_MAX
		#define MP_TUI_TEXT_MAX 32
	#endif

	#ifndef MP_TUI_DIRTY_MAX
		#define MP_TUI_DIRTY_MAX 4
	#endif

	#ifndef MP_TUI_HOME_MAX
		#define MP_TUI_HOME_MAX 8
	#endif

	/* sub-pages share this list and have no home button */
	#ifndef MP_TUI_APP_MAX
		#define MP_TUI_APP_MAX 12
	#endif

	/* screen saver, in seconds without input; 0 never */
	#ifndef MP_TUI_DIM_AFTER
		#define MP_TUI_DIM_AFTER 30
	#endif

	#ifndef MP_TUI_OFF_AFTER
		#define MP_TUI_OFF_AFTER 90
	#endif

	/* backlight while dimmed, in % */
	#ifndef MP_TUI_DIM_LEVEL
		#define MP_TUI_DIM_LEVEL 15
	#endif

	/* backlight while the screen is off, in %; 0 is dark */
	#ifndef MP_TUI_OFF_LEVEL
		#define MP_TUI_OFF_LEVEL 0
	#endif

	/* ms the level stays HIGH after an input */
	#ifndef MP_TUI_BOOST
		#define MP_TUI_BOOST 300
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

	typedef enum {
		MP_TUI_IMAGE_GRAY8,
		MP_TUI_IMAGE_RGB565,
	} mp_tui_image_format_t;

	/** caller's pixels, row after row; the TUI only reads them while drawing */
	typedef struct {
		const void *pixels;
		uint16_t w, h;
		mp_tui_image_format_t format;
	} mp_tui_image_t;

	typedef void (*mp_tui_close_t)(mp_tui_t *ui, void *user);

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
		/** optional, 0 to 100 %; the screen saver dims with it */
		void (*backlight)(mp_tui_display_t *display, int percent);
		/** optional, the panel itself; off keeps what it shows for the next on */
		void (*power)(mp_tui_display_t *display, int on);
		void *user;
	};

	typedef enum {
		MP_TUI_AWAKE = 0,
		MP_TUI_DIMMED,
		MP_TUI_OFF,
	} mp_tui_saver_t;

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

	typedef struct mp_tui_switch_s mp_tui_switch_t;

	/** called after a click flipped the switch; sw->on is the new state */
	typedef void (*mp_tui_switch_on_t)(mp_tui_switch_t *sw);

	/** text on the left, an on/off track on the right; a click flips it */
	struct mp_tui_switch_s {
		mp_tui_widget_t widget;
		char text[MP_TUI_TEXT_MAX];
		int on;
		mp_tui_switch_on_t onChange;
		void *user;
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
		/** how many of app[] have a button on the home; the rest are sub-pages */
		int homeCount;
		int focus;
		/** index of the open application, or -1 on the home */
		int opened;
		/** pages opened from a page; Back steps back through them */
		int stack[MP_TUI_APP_MAX];
		int depth;
		struct {
			const char *name;
			mp_tui_app_build_t build;
			void *user;
		} app[MP_TUI_APP_MAX];
		mp_tui_label_t homeTitle;
		mp_tui_button_t homeButton[MP_TUI_HOME_MAX];
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
		/** a widget with its own event, a switch: a tap on it survives a noisy finger like a button */
		mp_tui_widget_t *tapped;
		mp_tui_widget_t *dragged;
		mp_tui_view_t *dragView;

		/** screen saver: last input, and the gesture that woke the screen is not applied */
		mp_tui_saver_t saver;
		unsigned long activeAt;
		int swallow;
		/** HIGH for MP_TUI_BOOST after an input */
		mp_power_hold_t boost;
		/** MEDIUM while the screen is up, so the governor does not sit in Sleep */
		mp_power_hold_t rest;

		/** covers the display while shown; hidden until mp_tui_fullscreen() */
		mp_tui_widget_t fullscreen;
		mp_tui_image_t fullscreenImage;
		mp_tui_close_t fullscreenClose;
		void *fullscreenUser;
		/** 0 idle, 1 finger on the armed cross, 2 the press slid off it */
		int fullscreenPressed;
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

	typedef enum {
		/** 240-degree dial with a needle and the value in the middle */
		MP_TUI_GAUGE_DIAL,
		/** vertical thermometer: bulb, tube filled to the value */
		MP_TUI_GAUGE_THERMO,
		/** horizontal segmented bar with min/max ticks */
		MP_TUI_GAUGE_BAR,
	} mp_tui_gauge_style_t;

	/**
	 * value, min, max, warn and alarm are milli-units (m°C, mV).
	 * unit and label are not copied. warn = alarm = max disables the zones.
	 */
	typedef struct {
		mp_tui_widget_t widget;
		mp_tui_gauge_style_t style;
		long value, min, max;
		long warn, alarm;
		unsigned char decimals;
		const char *unit;
		const char *label;
		int valid;
	} mp_tui_gauge_t;

	#ifndef MP_TUI_TABLE_CELL
		/** bytes kept for one table cell, including the trailing 0 */
		#define MP_TUI_TABLE_CELL 24
	#endif

	/** one column of a table; width 0 shares what the fixed columns leave */
	typedef struct {
		const char *title;
		int width;
		mp_tui_align_t align;
	} mp_tui_column_t;

	/** one cell; the caller owns the array, row by row */
	typedef struct {
		char text[MP_TUI_TABLE_CELL];
	} mp_tui_cell_t;

	/**
	 * Header plus rows. columns and cells outlive the table.
	 * At most 8 columns are drawn.
	 */
	typedef struct {
		mp_tui_widget_t widget;
		const mp_tui_column_t *columns;
		mp_tui_cell_t *cells;
		int columnCount;
		int capacity;
		int rows;
	} mp_tui_table_t;

	#ifndef MP_TUI_COLUMNS_MAX
		#define MP_TUI_COLUMNS_MAX 4
	#endif

	/** one column of a layout: width 0 shares what the fixed ones leave; never under minWidth */
	typedef struct {
		int width;
		int minWidth;
	} mp_tui_column_spec_t;

	/**
	 * Columns side by side when each gets its minWidth, one under the other
	 * otherwise, so a page follows the orientation of the screen. Children go
	 * in mp_tui_columns_at(); mp_tui_columns_fit() sizes everything once they are in.
	 */
	typedef struct {
		mp_tui_widget_t widget;
		const mp_tui_column_spec_t *specs;
		int count;
		int gap;
		int stacked;
		mp_tui_widget_t column[MP_TUI_COLUMNS_MAX];
	} mp_tui_columns_t;

	/** framed panel with a title bar; children go below the title */
	typedef struct {
		mp_tui_widget_t widget;
		const mp_tui_font_t *font;
		char title[MP_TUI_TEXT_MAX];
	} mp_tui_window_t;

	extern const mp_tui_class_t mp_tui_box_class;
	extern const mp_tui_class_t mp_tui_label_class;
	extern const mp_tui_class_t mp_tui_bar_class;
	extern const mp_tui_class_t mp_tui_gauge_class;
	extern const mp_tui_class_t mp_tui_table_class;
	extern const mp_tui_class_t mp_tui_columns_class;
	extern const mp_tui_class_t mp_tui_window_class;
	extern const mp_tui_class_t mp_tui_button_class;
	extern const mp_tui_class_t mp_tui_switch_class;
	extern const mp_tui_class_t mp_tui_view_class;
	extern const mp_tui_class_t mp_tui_page_class;

	void mp_tui_box(mp_tui_box_t *box, mp_tui_widget_t *parent, int x, int y, int w, int h, mp_tui_color_t color);

	void mp_tui_label(mp_tui_label_t *label, mp_tui_widget_t *parent, int x, int y, int w, int h, const char *text);
	/**
	 * Like mp_tui_label(), but a line that does not fit w continues underneath
	 * and *y moves down by the height it took. line is the step of one row.
	 * font NULL measures with the theme font.
	 */
	void mp_tui_label_wrap(mp_tui_label_t *label, mp_tui_widget_t *parent, int x, int *y, int w, int line, const mp_tui_font_t *font, const char *text);
	/** printf-like; redraws only when the text changes */
	void mp_tui_label_set(mp_tui_label_t *label, const char *format, ...);
	void mp_tui_label_font(mp_tui_label_t *label, const mp_tui_font_t *font);
	void mp_tui_label_color(mp_tui_label_t *label, mp_tui_color_t color);
	void mp_tui_label_align(mp_tui_label_t *label, mp_tui_align_t align);

	void mp_tui_bar(mp_tui_bar_t *bar, mp_tui_widget_t *parent, int x, int y, int w, int h, int max);
	void mp_tui_bar_set(mp_tui_bar_t *bar, int value);

	void mp_tui_gauge(mp_tui_gauge_t *gauge, mp_tui_widget_t *parent, int x, int y, int w, int h, mp_tui_gauge_style_t style, long min, long max, unsigned char decimals, const char *unit);
	/** green below warn, amber through alarm, red above */
	void mp_tui_gauge_zones(mp_tui_gauge_t *gauge, long warn, long alarm);
	void mp_tui_gauge_label(mp_tui_gauge_t *gauge, const char *label);
	/** redraws only when the value or validity changes */
	void mp_tui_gauge_set(mp_tui_gauge_t *gauge, long value, int valid);

	/**
	 * A table of columnCount columns and up to capacity rows.
	 * The height follows the row count, header included. cells is row-major.
	 */
	void mp_tui_table(mp_tui_table_t *table, mp_tui_widget_t *parent, int x, int y, int w, const mp_tui_column_t *columns, int columnCount, mp_tui_cell_t *cells, int capacity);
	/** how many data rows are shown, clamped to capacity; resizes the table */
	void mp_tui_table_rows(mp_tui_table_t *table, int rows);
	/** printf-like; redraws only when that cell changes */
	void mp_tui_table_set(mp_tui_table_t *table, int row, int column, const char *format, ...);

	/** specs outlives the layout; at most MP_TUI_COLUMNS_MAX columns */
	void mp_tui_columns(mp_tui_columns_t *columns, mp_tui_widget_t *parent, int x, int y, int w, const mp_tui_column_spec_t *specs, int count, int gap);
	/** the parent of what goes in column index; its rect.w is the width to lay out in */
	mp_tui_widget_t *mp_tui_columns_at(mp_tui_columns_t *columns, int index);
	/** each column as tall as its children, stacked ones moved under each other; returns the height */
	int mp_tui_columns_fit(mp_tui_columns_t *columns);

	void mp_tui_window(mp_tui_window_t *window, mp_tui_widget_t *parent, int x, int y, int w, int h, const char *title);
	void mp_tui_window_title(mp_tui_window_t *window, const char *format, ...);

	void mp_tui_button(mp_tui_button_t *button, mp_tui_widget_t *parent, int x, int y, int w, int h, const char *text);

	void mp_tui_switch(mp_tui_switch_t *sw, mp_tui_widget_t *parent, int x, int y, int w, int h, const char *text, int on);
	/** does not call onChange; redraws only when the state changes */
	void mp_tui_switch_set(mp_tui_switch_t *sw, int on);
	void mp_tui_view(mp_tui_view_t *view, mp_tui_widget_t *parent, int x, int y, int w, int h);
	/** moves the content by dy pixels, kept between the top and the lowest child */
	void mp_tui_view_scroll(mp_tui_view_t *view, int dy);
	void mp_tui_page(mp_tui_page_t *page, mp_tui_widget_t *parent, int x, int y, int w, int h, int header);

	/**
	 * Declares an application on the home. build() fills the scrollable page
	 * each time it is opened. Returns 0 when the home is full or the name is missing.
	 */
	int mp_tui_app(mp_tui_t *ui, const char *name, mp_tui_app_build_t build, void *user);

	/**
	 * A page that does not appear on the home. Open it with mp_tui_enter()
	 * from a button on another page. Back returns to that page.
	 * Returns 0 when the list is full or the name is missing.
	 */
	int mp_tui_child(mp_tui_t *ui, const char *name, mp_tui_app_build_t build, void *user);

	/** opens a page declared with mp_tui_app() or mp_tui_child(), above the one on screen */
	void mp_tui_enter(mp_tui_t *ui, int index);

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

	/** home: up and down move, enter opens. page: up and down scroll, back steps out */
	void mp_tui_key(mp_tui_t *ui, mp_tui_key_t key);

	/** covers the display with image scaled to fit, a close cross top right; 0 when one is already up */
	int mp_tui_fullscreen(mp_tui_t *ui, const mp_tui_image_t *image, mp_tui_close_t onClose, void *user);
	/** redraws the image after the caller changed its pixels */
	void mp_tui_fullscreen_update(mp_tui_t *ui);
	/** same as a tap on the cross: onClose, then the page under it again */
	void mp_tui_fullscreen_close(mp_tui_t *ui);
	/** YES/non-zero while the fullscreen image is up */
	int mp_tui_fullscreen_shown(mp_tui_t *ui);

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
	/** clipped Bresenham line; coordinates are on the display */
	void mp_tui_draw_line(mp_tui_draw_t *draw, int x0, int y0, int x1, int y1, mp_tui_color_t color);
	/** y is the top of the line; returns the pen position after the text */
	int mp_tui_draw_text(mp_tui_draw_t *draw, const mp_tui_font_t *font, int x, int y, const char *text, mp_tui_color_t color);
	/** w and h are the destination size; the bitmap is scaled to fit */
	void mp_tui_draw_bitmap(mp_tui_draw_t *draw, const mp_tui_bitmap_t *bitmap, int x, int y, int w, int h);
	/** nearest neighbor into w by h, clipped to the band; gray8 is MP_TUI_RGB(v, v, v) */
	void mp_tui_draw_image(mp_tui_draw_t *draw, const mp_tui_image_t *image, int x, int y, int w, int h);

	/* kernel */

	/**
	 * A task draws one band per tick while the screen is dirty, then
	 * sleeps until an input, an invalidation or the screen saver.
	 */
	int mp_tui_attach(struct mp_kernel_s *kernel, mp_tui_t *ui, char *who);
	void mp_tui_detach(mp_tui_t *ui);

	/**
	 * Screen saver: dims after MP_TUI_DIM_AFTER s without input. After
	 * MP_TUI_OFF_AFTER s it turns the backlight to MP_TUI_OFF_LEVEL % and,
	 * when the display provides it, powers the panel down, but only while
	 * mp_power_hibernating() is YES. Otherwise it stays dimmed, which holds
	 * the reduced supply. Only with a display that has backlight() or
	 * power(). Off, nothing is drawn, accelerometers sleep, and the touch
	 * panel dozes; the first gesture only wakes the screen.
	 */
	void mp_tui_wake(mp_tui_t *ui);
	int mp_tui_awake(mp_tui_t *ui);

	/** applies the screen saver; ticks until it moves again or an input wants a read, 0 for never */
	unsigned long mp_tui_idle(mp_tui_t *ui);

	/** @} */

#endif
