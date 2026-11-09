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

#ifndef _HAVE_MP_TERM_H
	#define _HAVE_MP_TERM_H

	/**
	 * @defgroup mpCommonTerm Terminal codec
	 * @ingroup mpCommon
	 *
	 * An ANSI / VT100 / xterm codec. The app emits by intent
	 * (mp_term_move, mp_term_fg_rgb, ...) and the codec writes the bytes
	 * the peer understands, degrading to its caps. Bytes received are
	 * fed through mp_term_rx() and read back as events (text, key, mouse,
	 * paste, resize, OSC). The codec is sink-only: it does not own the
	 * transport, it writes through a callback the caller provides.
	 * mp.h includes this header even when the module is off; the calls
	 * below then compile to nothing, so a call site needs no ifdef.
	 * @{
	 */

	#ifdef SUPPORT_COMMON_TERM

		#ifndef MP_TERM_PARAMS
			#define MP_TERM_PARAMS 16
		#endif
		#ifndef MP_TERM_OSC_MAX
			#define MP_TERM_OSC_MAX 128
		#endif
		#ifndef MP_TERM_EVENTS
			#define MP_TERM_EVENTS 8
		#endif
		#ifndef MP_TERM_ESCAPE_MS
			#define MP_TERM_ESCAPE_MS 50
		#endif
		#ifndef MP_TERM_PROTO_MAX
			#define MP_TERM_PROTO_MAX 8
		#endif
		/* the max color the build compiles; the setting emits 0..3 */
		#ifndef MP_TERM_COLOR
			#define MP_TERM_COLOR MP_TERM_COLOR_TRUE
		#endif

		typedef struct mp_term_s mp_term_t;

		/* the byte sink: a UART, USB CDC, RTT, a socket, a test buffer */
		typedef void (*mp_term_write_t)(void *user, const unsigned char *bytes, int n);

		/* runtime caps, masked at init by what the build compiles */
	#define MP_TERM_CAP_VT100  0x01
	#define MP_TERM_CAP_VT102  0x08
	#define MP_TERM_CAP_XTERM  0x02
	#define MP_TERM_CAP_OSC    0x04
	#define MP_TERM_CAP_ALL   (MP_TERM_CAP_VT100 | MP_TERM_CAP_VT102 | MP_TERM_CAP_XTERM | MP_TERM_CAP_OSC)

		typedef enum {
			MP_TERM_COLOR_NONE = 0,
			MP_TERM_COLOR_16 = 1,
			MP_TERM_COLOR_256 = 2,
			MP_TERM_COLOR_TRUE = 3,
		} mp_term_color_t;

		/**
		 * Special keys. Printable characters are themselves; the
		 * specials start at 0x100.
		 */
		typedef enum {
			MP_TERM_KEY_NONE = 0,
			MP_TERM_KEY_BACKSPACE = 0x08,
			MP_TERM_KEY_TAB = 0x09,
			MP_TERM_KEY_ENTER = 0x0d,
			MP_TERM_KEY_CTRL_L = 0x0c,
			MP_TERM_KEY_ESCAPE = 0x1b,
			MP_TERM_KEY_UP = 0x100,
			MP_TERM_KEY_DOWN,
			MP_TERM_KEY_LEFT,
			MP_TERM_KEY_RIGHT,
			MP_TERM_KEY_HOME,
			MP_TERM_KEY_END,
			MP_TERM_KEY_PAGE_UP,
			MP_TERM_KEY_PAGE_DOWN,
			MP_TERM_KEY_DELETE,
		} mp_term_key_t;

		/**
		 * Alt+key. Minicom sends ESC then the character. The value is
		 * MP_TERM_KEY_ALT('c'). 'O' is not claimed: ESC O starts SS3.
		 */
		#define MP_TERM_KEY_ALT(c)      (0x200 | ((int)(c) & 0x7f))
		#define MP_TERM_KEY_ALT_CHAR(k) ((int)(k) & 0x7f)
		#define MP_TERM_KEY_IS_ALT(k)   ((((int)(k)) & ~0x7f) == 0x200)

		/* combinable except RESET, which clears everything */
		#define MP_TERM_STYLE_RESET     0x00
		#define MP_TERM_STYLE_BOLD      0x01
		#define MP_TERM_STYLE_ITALIC    0x02
		#define MP_TERM_STYLE_UNDERLINE 0x04
		#define MP_TERM_STYLE_REVERSE   0x08
		#define MP_TERM_STYLE_DIM       0x10

		typedef enum {
			MP_TERM_EVENT_NONE = 0,
			/** bytes the user typed; bytes/len */
			MP_TERM_EVENT_TEXT,
			/** key is an mp_term_key_t or a character */
			MP_TERM_EVENT_KEY,
			/** x, y, button (low 3 bits) + state in value */
			MP_TERM_EVENT_MOUSE,
			/** bracketed paste start (value 1) or end (value 0) */
			MP_TERM_EVENT_PASTE,
			/** x = rows, y = cols */
			MP_TERM_EVENT_RESIZE,
			/** bytes/len is the OSC payload without the command prefix */
			MP_TERM_EVENT_OSC,
		} mp_term_event_type_t;

		typedef struct {
			mp_term_event_type_t type;
			int x, y;
			int key;
			int value;
			const unsigned char *bytes;
			int len;
		} mp_term_event_t;

		/* parser states */
		typedef enum {
			_MP_TERM_GROUND = 0,
			_MP_TERM_ESC,
			_MP_TERM_CSI,
			_MP_TERM_OSC,
			_MP_TERM_SS3,
			_MP_TERM_DCS,
		} _mp_term_state_t;

		/* a protocol driver. The caller keeps the mp_term_proto_t struct.
		 * Each callback returns TRUE when it claims the sequence or the
		 * intent; the core stops at the first driver that does. */
		typedef struct mp_term_proto_s mp_term_proto_t;
		typedef struct mp_term_proto_ops_s mp_term_proto_ops_t;

		struct mp_term_proto_ops_s {
			const char *name;
			unsigned int cap;          /* the cap bit this driver lights */

			/* parse: return TRUE if this driver handled the sequence */
			mp_bool_t (*csi)(mp_term_t *term, mp_term_proto_t *self, unsigned char final);
			mp_bool_t (*esc)(mp_term_t *term, mp_term_proto_t *self, unsigned char c);
			mp_bool_t (*osc)(mp_term_t *term, mp_term_proto_t *self, const unsigned char *buf, int len);

			/* emit: return TRUE if this driver emitted for the intent */
			mp_bool_t (*move)(mp_term_t *term, mp_term_proto_t *self, int row, int col);
			mp_bool_t (*clear)(mp_term_t *term, mp_term_proto_t *self);
			mp_bool_t (*clear_line)(mp_term_t *term, mp_term_proto_t *self);
			mp_bool_t (*style)(mp_term_t *term, mp_term_proto_t *self, unsigned int flags);
			mp_bool_t (*fg)(mp_term_t *term, mp_term_proto_t *self, int index);
			mp_bool_t (*bg)(mp_term_t *term, mp_term_proto_t *self, int index);
			mp_bool_t (*fg_rgb)(mp_term_t *term, mp_term_proto_t *self, unsigned char r, unsigned char g, unsigned char b);
			mp_bool_t (*bg_rgb)(mp_term_t *term, mp_term_proto_t *self, unsigned char r, unsigned char g, unsigned char b);
			mp_bool_t (*title)(mp_term_t *term, mp_term_proto_t *self, const char *title);
			mp_bool_t (*link)(mp_term_t *term, mp_term_proto_t *self, const char *uri, const char *label);
		mp_bool_t (*mouse)(mp_term_t *term, mp_term_proto_t *self, int on);
		mp_bool_t (*alt)(mp_term_t *term, mp_term_proto_t *self, int on);

		/* VT102 screen ops. NULL when the driver does not implement them;
		 * the core falls through to the next driver. */
		mp_bool_t (*region)(mp_term_t *term, mp_term_proto_t *self, int top, int bottom);
		mp_bool_t (*insert_line)(mp_term_t *term, mp_term_proto_t *self, int n);
		mp_bool_t (*delete_line)(mp_term_t *term, mp_term_proto_t *self, int n);
		/* n > 0 scrolls up n lines, n < 0 scrolls down |n| lines */
		mp_bool_t (*scroll)(mp_term_t *term, mp_term_proto_t *self, int n);
	};

		struct mp_term_proto_s {
			mp_list_item_t item;
			const mp_term_proto_ops_t *ops;
			void *self;               /* driver-private, passed back in callbacks */
		};

		struct mp_serial_s;

		struct mp_term_s {
			mp_kernel_t *kernel;

			mp_term_write_t write;
			void *user;

			unsigned int caps;        /* the set of registered drivers */
			mp_term_color_t colorLevel;

			/* the registered protocol drivers */
			mp_list_t protos;
			mp_term_proto_t _protoSlots[MP_TERM_PROTO_MAX];

			/* FSM */
			_mp_term_state_t state;
			int params[MP_TERM_PARAMS];
			int paramCount;
			mp_bool_t paramOpen;     /* a digit has started the current slot */
			mp_bool_t private_;       /* CSI ? / < private marker */
			mp_bool_t carriage;
			int escape;              /* 0 no, 1 after ESC, 2 after ESC [ */
			unsigned long escapeTick;

			/* OSC accumulator */
			unsigned char osc[MP_TERM_OSC_MAX];
			int oscLen;
			mp_bool_t oscString;     /* ST seen, else BEL ends */

			/* event ring */
			mp_term_event_t events[MP_TERM_EVENTS];
			unsigned char head;
			unsigned char tail;

			/* pending text run, flushed on a state change or a full buffer */
			unsigned char text[MP_TERM_OSC_MAX];
			int textLen;

			/* glass size, 0 until a probe or a resize reply. home is the
			 * first row of the scrolling area, set by mp_term_splash. */
			int rows, cols, home;
			mp_bool_t probe;         /* the next CPR is the clamped cursor */
			/* the next CPR answers mp_term_query_utf8; it goes before probe */
			mp_bool_t probeUtf8;
			/* the glass decodes UTF-8: a box line took one column. Only
			 * meaningful once utf8Known is YES. */
			mp_bool_t utf8, utf8Known;

			/* set by the shell task once it owns the line. NULL before. */
			struct mp_serial_s *line;
			mp_task_t *task;
			unsigned long probeAt;
		};

		/**
		 * Wire the codec. caps are masked by what the build compiles, so a
		 * runtime cap the binary lacks is dropped. colorLevel is clamped to
		 * the build's MP_TERM_COLOR. write is the only output path.
		 */
		mp_ret_t mp_term_init(mp_kernel_t *kernel, mp_term_t *term,
			mp_term_write_t write, void *user,
			unsigned int caps, mp_term_color_t colorLevel);

		/* change the runtime caps and color level of an open term */
		void mp_term_caps(mp_term_t *term, unsigned int caps);
		void mp_term_color(mp_term_t *term, mp_term_color_t colorLevel);
		unsigned int mp_term_caps_get(const mp_term_t *term);
		mp_term_color_t mp_term_color_get(const mp_term_t *term);

		/**
		 * Register a protocol driver. The caller keeps the mp_term_proto_t.
		 * Registering lights the driver's cap bit in term->caps. The core
		 * dispatches parse and emit calls to registered drivers in order.
		 */
		mp_ret_t mp_term_proto_register(mp_term_t *term, mp_term_proto_t *proto,
			const mp_term_proto_ops_t *ops, void *self);
		/** Drop a protocol driver and clear its cap bit. */
		mp_ret_t mp_term_proto_unregister(mp_term_t *term, mp_term_proto_t *proto);

		/* emission: intent, not bytes. Each degrades to the peer caps. */
		void mp_term_move(mp_term_t *term, int row, int col);
		void mp_term_clear(mp_term_t *term);
		void mp_term_clear_line(mp_term_t *term);
		void mp_term_style(mp_term_t *term, unsigned int flags);
		void mp_term_fg_rgb(mp_term_t *term, unsigned char r, unsigned char g, unsigned char b);
		void mp_term_bg_rgb(mp_term_t *term, unsigned char r, unsigned char g, unsigned char b);
		void mp_term_fg(mp_term_t *term, int index);   /* 0..15 */
		void mp_term_bg(mp_term_t *term, int index);
		void mp_term_title(mp_term_t *term, const char *title);
		void mp_term_link(mp_term_t *term, const char *uri, const char *label);
		void mp_term_mouse(mp_term_t *term, int on);
		void mp_term_alt(mp_term_t *term, int on);
		void mp_term_region(mp_term_t *term, int top, int bottom);
		void mp_term_insert_line(mp_term_t *term, int n);
		void mp_term_delete_line(mp_term_t *term, int n);
		void mp_term_scroll(mp_term_t *term, int n);
		/** show or hide the cursor */
		void mp_term_cursor(mp_term_t *term, int on);
		/**
		 * Ask the peer how big the glass is. Hides the cursor, parks it in
		 * the corner and requests a cursor-position report. The answer
		 * comes back as MP_TERM_EVENT_RESIZE. Minicom never sends a resize
		 * on its own; the corner clamps, so the report is the window size.
		 */
		void mp_term_query(mp_term_t *term);
		/**
		 * Ask whether the peer decodes UTF-8: prints one box line at the
		 * home cell and requests a report. One column moved means UTF-8;
		 * three means bytes. Sets utf8 and utf8Known on the answer. The
		 * home row is dirty afterwards. Minicom drops DEC graphics but
		 * passes UTF-8, so this picks how lines are drawn.
		 */
		void mp_term_query_utf8(mp_term_t *term);
		/** put the cursor back after a probe that did not change the size */
		void mp_term_settle(mp_term_t *term);
		/** DEC special graphics (ESC ( 0) or ASCII (ESC ( B). VT100 only. */
		void mp_term_graphic(mp_term_t *term, int on);
		void mp_term_puts(mp_term_t *term, const unsigned char *bytes, int n);
		void mp_term_putc(mp_term_t *term, unsigned char c);

		/* reception: the app feeds bytes from its reader task, never an IRQ */
		void mp_term_rx(mp_term_t *term, unsigned char c);
		/** 1 when an event is ready, 0 otherwise. *event is filled. */
		int mp_term_read(mp_term_t *term, mp_term_event_t *event);
		/** ticks to wait before the lone Escape fires, 0 when none pending */
		unsigned long mp_term_waiting(const mp_term_t *term);

		/** a banner line the app passes to mp_term_splash() */
		typedef struct {
			const char *label;
			const char *value;
		} mp_term_banner_line_t;

		/**
		 * Render a banner: a styled header, then label/value lines, then a
		 * section about the term itself (peer, caps, color, transport, size).
		 * Degrades to the peer caps; on a dumb peer it is plain text.
		 */
		void mp_term_splash(mp_term_t *term, const mp_term_banner_line_t *lines, int n);

		#include "ui.h"

	#else

		#define mp_term_init(...)        ((mp_ret_t)0)
		#define mp_term_caps(...)        ((void)0)
		#define mp_term_color(...)       ((void)0)
		#define mp_term_caps_get(...)    (0)
		#define mp_term_color_get(...)   ((mp_term_color_t)0)
		#define mp_term_move(...)        ((void)0)
		#define mp_term_clear(...)       ((void)0)
		#define mp_term_clear_line(...)  ((void)0)
		#define mp_term_style(...)       ((void)0)
		#define mp_term_fg_rgb(...)      ((void)0)
		#define mp_term_bg_rgb(...)      ((void)0)
		#define mp_term_fg(...)          ((void)0)
		#define mp_term_bg(...)          ((void)0)
		#define mp_term_title(...)       ((void)0)
		#define mp_term_link(...)        ((void)0)
	#define mp_term_mouse(...)       ((void)0)
	#define mp_term_alt(...)         ((void)0)
	#define mp_term_region(...)      ((void)0)
	#define mp_term_insert_line(...)  ((void)0)
	#define mp_term_delete_line(...)  ((void)0)
		#define mp_term_scroll(...)      ((void)0)
		#define mp_term_cursor(...)      ((void)0)
		#define mp_term_query(...)       ((void)0)
		#define mp_term_query_utf8(...)  ((void)0)
		#define mp_term_settle(...)      ((void)0)
		#define mp_term_graphic(...)     ((void)0)
		#define mp_term_puts(...)        ((void)0)
		#define mp_term_ui_init(...)     ((mp_ret_t)0)
		#define mp_term_ui_add(...)      ((mp_ret_t)0)
		#define mp_term_ui_remove(...)   ((void)0)
		#define mp_term_ui_start(...)    ((mp_ret_t)0)
		#define mp_term_ui_stop(...)     ((void)0)
		#define mp_term_ui_put(...)      (0)
		#define mp_term_ui_lines(...)    (0)
		#define mp_term_ui_fill(...)     ((void)0)
		#define mp_term_ui_clear(...)    ((void)0)
		#define mp_term_ui_hint(...)     ((void)0)
		#define mp_term_ui_rows(...)     (0)
		#define mp_term_ui_cols(...)     (0)
		#define mp_term_ui_cursor_at(...) ((void)0)
		#define mp_term_ui_place(...)    ((void)0)
		#define mp_term_ui_event(...)    ((void)0)
		#define mp_term_ui_flush(...)    ((void)0)
		#define mp_term_ui_log(...)      ((void)0)
		#define mp_term_ui_welcome(...)  ((void)0)
		#define mp_term_ui_replay(...)   ((void)0)
		#define mp_term_ui_host_ready(...) ((void)0)
		#define mp_term_ui_enter(...)    ((void)0)
		#define mp_term_ui_at(...)       (0)
		#define mp_term_ui_note_probe(...) ((void)0)
		#define mp_term_ui_waiting(...)  (0UL)
		#define mp_term_ui_redraw(...)   ((void)0)
		#define mp_term_putc(...)        ((void)0)
		#define mp_term_rx(...)          ((void)0)
		#define mp_term_read(...)        (0)
		#define mp_term_waiting(...)     (0)
		#define mp_term_splash(...)      ((void)0)

	#endif

#endif
