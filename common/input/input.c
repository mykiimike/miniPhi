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

#include <mp.h>

#ifdef SUPPORT_COMMON_INPUT

/**
@defgroup mpCommonInputCore Input queue and gestures

@ingroup mpCommonInput

@{
*/

enum {
	_MP_INPUT_Q_BUTTON = 1,
	_MP_INPUT_Q_WHEEL,
	_MP_INPUT_Q_KEY,
	_MP_INPUT_Q_BYTE,
};

/* a lone escape is sent once nothing followed it for that long */
#define _MP_INPUT_ESCAPE_MS 50

int mp_input_init(mp_kernel_t *kernel, mp_input_t *input, int caps, const char *who) {
	memset(input, 0, sizeof(*input));
	input->kernel = kernel;
	input->caps = caps;
	input->who = who;
	mp_list_add_last(&kernel->inputs, &input->item, input);
	return(1);
}

void mp_input_fini(mp_input_t *input) {
	if(!input->kernel)
		return;
	mp_list_remove(&input->kernel->inputs, &input->item);
	input->kernel = NULL;
}

void mp_input_bind(mp_input_t *input, void *owner) {
	input->owner = owner;
}

static int _mp_input_clamp(int value, int size) {
	if(size <= 0)
		return(value);
	if(value < 0)
		return(0);
	if(value >= size)
		return(size - 1);
	return(value);
}

void mp_input_bounds(mp_input_t *input, int width, int height) {
	input->width = width;
	input->height = height;
	input->x = _mp_input_clamp(input->x, width);
	input->y = _mp_input_clamp(input->y, height);
}

/* producer */

static void _mp_input_wake(mp_input_t *input) {
	if(input->reader)
		mp_task_wake(input->reader);
}

static void _mp_input_push(mp_input_t *input, int type, int down, int value) {
	unsigned char next = (unsigned char)((input->head + 1) % MP_INPUT_QUEUE);

	_mp_input_wake(input);
	if(next == input->tail) {
		input->dropped++;
		return;
	}
	input->queue[input->head].type = (unsigned char)type;
	input->queue[input->head].down = (unsigned char)down;
	input->queue[input->head].x = (short)input->x;
	input->queue[input->head].y = (short)input->y;
	input->queue[input->head].value = value;
	input->head = next;
}

void mp_input_touch(mp_input_t *input, int x, int y, int down) {
	x = _mp_input_clamp(x, input->width);
	y = _mp_input_clamp(y, input->height);
	if(x != input->x || y != input->y) {
		input->x = x;
		input->y = y;
		input->moved = 1;
		_mp_input_wake(input);
	}
	mp_input_button(input, down);
}

void mp_input_motion(mp_input_t *input, int dx, int dy) {
	if(dx == 0 && dy == 0)
		return;
	input->x = _mp_input_clamp(input->x + dx, input->width);
	input->y = _mp_input_clamp(input->y + dy, input->height);
	input->moved = 1;
	_mp_input_wake(input);
}

void mp_input_button(mp_input_t *input, int down) {
	down = down ? 1 : 0;
	if(down == input->down)
		return;
	input->down = down;
	_mp_input_push(input, _MP_INPUT_Q_BUTTON, down, 0);
}

void mp_input_wheel(mp_input_t *input, int steps) {
	if(steps)
		_mp_input_push(input, _MP_INPUT_Q_WHEEL, 0, steps);
}

void mp_input_key(mp_input_t *input, int key) {
	if(key)
		_mp_input_push(input, _MP_INPUT_Q_KEY, 0, key);
}

void mp_input_terminal(mp_input_t *input, unsigned char c) {
	_mp_input_push(input, _MP_INPUT_Q_BYTE, 0, c);
}

/* reader */

static void _mp_input_emit(mp_input_t *input, mp_input_type_t type, int x, int y, int value) {
	mp_input_event_t *event;

	if(input->pendingCount >= (int)(sizeof(input->pending) / sizeof(input->pending[0])))
		return;
	event = &input->pending[input->pendingCount++];
	memset(event, 0, sizeof(*event));
	event->type = type;
	event->x = x;
	event->y = y;
	event->dx = x - input->lastX;
	event->dy = y - input->lastY;
	event->startX = input->pressX;
	event->startY = input->pressY;
	event->value = value;
}

static int _mp_input_far(int a, int b) {
	int d = a - b;

	return((d < 0 ? -d : d) > MP_INPUT_DRAG_THRESHOLD);
}

static void _mp_input_motion(mp_input_t *input, int x, int y) {
	if(x == input->lastX && y == input->lastY)
		return;

	if(input->pressed) {
		if(!input->dragging && (_mp_input_far(x, input->pressX) || _mp_input_far(y, input->pressY))) {
			input->dragging = 1;
			input->lastX = input->pressX;
			input->lastY = input->pressY;
			_mp_input_emit(input, MP_INPUT_DRAG_START, input->pressX, input->pressY, 0);
		}
		if(!input->dragging)
			return;
		_mp_input_emit(input, MP_INPUT_DRAG, x, y, 0);
	}
	else
		_mp_input_emit(input, MP_INPUT_MOVE, x, y, 0);

	input->lastX = x;
	input->lastY = y;
}

static void _mp_input_buttonEvent(mp_input_t *input, int down, int x, int y) {
	_mp_input_motion(input, x, y);
	input->lastX = x;
	input->lastY = y;

	if(down) {
		input->pressed = 1;
		input->dragging = 0;
		input->pressX = x;
		input->pressY = y;
		_mp_input_emit(input, MP_INPUT_DOWN, x, y, 0);
		return;
	}
	if(!input->pressed)
		return;
	if(input->dragging)
		_mp_input_emit(input, MP_INPUT_DROP, x, y, 0);
	else
		_mp_input_emit(input, MP_INPUT_CLICK, input->pressX, input->pressY, 0);
	input->pressed = 0;
	input->dragging = 0;
}

static void _mp_input_keyEvent(mp_input_t *input, int key) {
	_mp_input_emit(input, MP_INPUT_KEY, input->lastX, input->lastY, key);
}

static void _mp_input_byte(mp_input_t *input, unsigned char c) {
	static const int tilde[] = {
		MP_INPUT_KEY_NONE, MP_INPUT_KEY_HOME, MP_INPUT_KEY_NONE, MP_INPUT_KEY_DELETE,
		MP_INPUT_KEY_END, MP_INPUT_KEY_PAGE_UP, MP_INPUT_KEY_PAGE_DOWN,
	};

	if(input->escape == 1) {
		if(c == '[' || c == 'O') {
			input->escape = 2;
			return;
		}
		input->escape = 0;
		_mp_input_keyEvent(input, MP_INPUT_KEY_ESCAPE);
	}
	else if(input->escape == 2) {
		input->escape = 0;
		switch(c) {
			case 'A': _mp_input_keyEvent(input, MP_INPUT_KEY_UP); return;
			case 'B': _mp_input_keyEvent(input, MP_INPUT_KEY_DOWN); return;
			case 'C': _mp_input_keyEvent(input, MP_INPUT_KEY_RIGHT); return;
			case 'D': _mp_input_keyEvent(input, MP_INPUT_KEY_LEFT); return;
			case 'H': _mp_input_keyEvent(input, MP_INPUT_KEY_HOME); return;
			case 'F': _mp_input_keyEvent(input, MP_INPUT_KEY_END); return;
		}
		if(c >= '0' && c <= '9') {
			input->escape = 3;
			input->escapeValue = c - '0';
		}
		return;
	}
	else if(input->escape == 3) {
		if(c >= '0' && c <= '9') {
			input->escapeValue = input->escapeValue * 10 + (c - '0');
			return;
		}
		input->escape = 0;
		if(c == '~' && input->escapeValue < (int)(sizeof(tilde) / sizeof(tilde[0])) && tilde[input->escapeValue])
			_mp_input_keyEvent(input, tilde[input->escapeValue]);
		return;
	}

	if(c == '\n' && input->carriage) {
		input->carriage = 0;
		return;
	}
	input->carriage = c == '\r';

	if(c == 0x1B) {
		input->escape = 1;
		input->escapeTick = mp_clock_ticks();
	}
	else if(c == '\r' || c == '\n')
		_mp_input_keyEvent(input, MP_INPUT_KEY_ENTER);
	else if(c == 0x7F || c == 0x08)
		_mp_input_keyEvent(input, MP_INPUT_KEY_BACKSPACE);
	else if(c == '\t')
		_mp_input_keyEvent(input, MP_INPUT_KEY_TAB);
	else if(c >= 0x20)
		_mp_input_keyEvent(input, c);
}

static unsigned long _mp_input_ms(unsigned long ms) {
	unsigned long ticks = (mp_clock_tick_hz() * ms) / 1000;

	return(ticks ? ticks : 1);
}

int mp_input_read(mp_input_t *input, mp_input_event_t *event) {
	int a;

	while(1) {
		if(input->pendingCount > 0) {
			*event = input->pending[0];
			for(a=1; a<input->pendingCount; a++)
				input->pending[a - 1] = input->pending[a];
			input->pendingCount--;
			return(1);
		}

		if(input->tail != input->head) {
			int type = input->queue[input->tail].type;
			int down = input->queue[input->tail].down;
			int x = input->queue[input->tail].x;
			int y = input->queue[input->tail].y;
			int value = input->queue[input->tail].value;

			input->tail = (unsigned char)((input->tail + 1) % MP_INPUT_QUEUE);
			if(type == _MP_INPUT_Q_BUTTON)
				_mp_input_buttonEvent(input, down, x, y);
			else if(type == _MP_INPUT_Q_WHEEL)
				_mp_input_emit(input, MP_INPUT_WHEEL, input->lastX, input->lastY, value);
			else if(type == _MP_INPUT_Q_KEY)
				_mp_input_keyEvent(input, value);
			else if(type == _MP_INPUT_Q_BYTE)
				_mp_input_byte(input, (unsigned char)value);
			continue;
		}

		if(input->moved) {
			input->moved = 0;
			_mp_input_motion(input, input->x, input->y);
			if(input->pendingCount > 0)
				continue;
		}

		if(input->escape == 1 && mp_clock_ticks() - input->escapeTick >= _mp_input_ms(_MP_INPUT_ESCAPE_MS)) {
			input->escape = 0;
			_mp_input_keyEvent(input, MP_INPUT_KEY_ESCAPE);
			continue;
		}
		return(0);
	}
}

mp_input_t *mp_input_next(mp_kernel_t *kernel, mp_input_t *after, void *reader) {
	mp_list_item_t *item = after ? after->item.next : kernel->inputs.first;
	mp_input_t *input;

	for(; item; item = item->next) {
		input = item->user;
		if(!input->owner || input->owner == reader)
			return(input);
	}
	return(NULL);
}

unsigned long mp_input_waiting(mp_input_t *input) {
	unsigned long wait = _mp_input_ms(_MP_INPUT_ESCAPE_MS);
	unsigned long gone;

	if(input->pendingCount > 0 || input->tail != input->head || input->moved)
		return(1);
	if(input->escape != 1)
		return(0);
	gone = mp_clock_ticks() - input->escapeTick;
	return(gone >= wait ? 1 : wait - gone);
}

void mp_input_doze(mp_input_t *input, int dozing) {
	dozing = dozing ? 1 : 0;
	if(input->dozing == dozing)
		return;
	input->dozing = dozing;
	if(!dozing && input->task)
		mp_task_wake(input->task);
}

/**@}*/

#endif
