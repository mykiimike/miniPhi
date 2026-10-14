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

#ifndef _HAVE_MP_COMMON_INPUT_H
	#define _HAVE_MP_COMMON_INPUT_H

	#ifdef SUPPORT_COMMON_INPUT

	/**
	 * @defgroup mpCommonInput Input devices
	 * @ingroup mpCommon
	 *
	 * An input is what a driver or a firmware feeds: a touch panel gives
	 * absolute positions, a mouse relative moves and a wheel, a keyboard
	 * keys. Feeding only records: transitions go in a small queue the
	 * interrupt side can fill, motion is kept as the last position.
	 * mp_input_read() turns that into gestures: down, click, drag start,
	 * drag, drop, wheel and keys. Every input is listed on the kernel, so a
	 * user interface reads all of them without knowing the drivers.
	 * @{
	 */

	#ifndef MP_INPUT_QUEUE
		#define MP_INPUT_QUEUE 16
	#endif

	#ifndef MP_INPUT_DRAG_THRESHOLD
		#define MP_INPUT_DRAG_THRESHOLD 6
	#endif

	/** an absolute pointer: touch panel, pen */
	#define MP_INPUT_CAP_TOUCH 0x01
	/** a relative pointer with a visible cursor */
	#define MP_INPUT_CAP_MOUSE 0x02
	#define MP_INPUT_CAP_WHEEL 0x04
	#define MP_INPUT_CAP_KEYS  0x08

	/** printable keys are their character; the others start at 0x100 */
	typedef enum {
		MP_INPUT_KEY_NONE = 0,
		MP_INPUT_KEY_BACKSPACE = 0x08,
		MP_INPUT_KEY_TAB = 0x09,
		MP_INPUT_KEY_ENTER = 0x0D,
		MP_INPUT_KEY_ESCAPE = 0x1B,
		MP_INPUT_KEY_UP = 0x100,
		MP_INPUT_KEY_DOWN,
		MP_INPUT_KEY_LEFT,
		MP_INPUT_KEY_RIGHT,
		MP_INPUT_KEY_HOME,
		MP_INPUT_KEY_END,
		MP_INPUT_KEY_PAGE_UP,
		MP_INPUT_KEY_PAGE_DOWN,
		MP_INPUT_KEY_DELETE,
	} mp_input_key_t;

	typedef enum {
		MP_INPUT_NONE = 0,
		/** the pointer moved without a button */
		MP_INPUT_MOVE,
		/** button or finger down at x, y */
		MP_INPUT_DOWN,
		/** released near where it went down */
		MP_INPUT_CLICK,
		/** moved beyond MP_INPUT_DRAG_THRESHOLD while down; x, y is the press point */
		MP_INPUT_DRAG_START,
		/** dx, dy since the previous drag event */
		MP_INPUT_DRAG,
		/** released after a drag; startX, startY is the press point */
		MP_INPUT_DROP,
		/** value is the number of steps, positive away from the user */
		MP_INPUT_WHEEL,
		/** value is an mp_input_key_t or a character */
		MP_INPUT_KEY,
	} mp_input_type_t;

	typedef struct {
		mp_input_type_t type;
		int x, y;
		int dx, dy;
		int startX, startY;
		int value;
	} mp_input_event_t;

	typedef struct mp_input_s mp_input_t;

	struct mp_input_s {
		mp_kernel_t *kernel;
		const char *who;
		int caps;

		/** set by mp_input_bind(): only that reader takes the events, NULL for any */
		void *owner;

		/** the pointer is clamped to 0..width-1, 0..height-1 when they are set */
		int width;
		int height;

		/* producer side, one producer per input */
		volatile int x, y;
		volatile int moved;
		volatile int down;
		struct {
			unsigned char type;
			unsigned char down;
			short x, y;
			int value;
		} queue[MP_INPUT_QUEUE];
		volatile unsigned char head;
		volatile unsigned char tail;
		volatile int dropped;

		/* reader side */
		int pressed;
		int dragging;
		int pressX, pressY;
		int lastX, lastY;
		int escape;
		int escapeValue;
		unsigned long escapeTick;
		int carriage;
		mp_input_event_t pending[4];
		int pendingCount;

		/** woken by every feed; the TUI puts its task there */
		struct mp_task_s *reader;

		/** the driver task, woken when the input stops dozing */
		struct mp_task_s *task;

		/** set by mp_input_doze(): nobody looks, the driver may sample slower */
		volatile int dozing;

		mp_list_item_t item;
	};

	/** lists the input on the kernel; caps are MP_INPUT_CAP_* */
	int mp_input_init(mp_kernel_t *kernel, mp_input_t *input, int caps, const char *who);
	void mp_input_fini(mp_input_t *input);
	void mp_input_bind(mp_input_t *input, void *owner);
	void mp_input_bounds(mp_input_t *input, int width, int height);

	/* feeding, from a driver task or an interrupt */

	/** absolute position, a touch panel; down says whether it is touched */
	void mp_input_touch(mp_input_t *input, int x, int y, int down);
	/** relative move of a mouse */
	void mp_input_motion(mp_input_t *input, int dx, int dy);
	/** the main button of a mouse at the current position */
	void mp_input_button(mp_input_t *input, int down);
	void mp_input_wheel(mp_input_t *input, int steps);
	/** one key press, an mp_input_key_t or a character */
	void mp_input_key(mp_input_t *input, int key);
	/** one byte of a terminal: characters, and the VT100 arrows and escape */
	void mp_input_terminal(mp_input_t *input, unsigned char c);

	/* reading */

	/** next gesture of that input; 0 when there is none */
	int mp_input_read(mp_input_t *input, mp_input_event_t *event);

	/** first input after `after` (NULL for the first) that `reader` may read */
	mp_input_t *mp_input_next(mp_kernel_t *kernel, mp_input_t *after, void *reader);

	/** ticks until mp_input_read() has something without a new feed, 0 for never */
	unsigned long mp_input_waiting(mp_input_t *input);

	/** the screen is off: the driver may only watch for a press */
	void mp_input_doze(mp_input_t *input, int dozing);

	/** @} */

	#endif
#endif
