/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2026  Michael VERGOZ                                      *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify    *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation, either version 3 of the License, or       *
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

/**
 * @defgroup mpCommonTerm Terminal codec
 * @ingroup mpCommon
 *
 * The base ANSI / VT100 protocol driver: cursor keys, the tilde table,
 * and the SGR emitters (move, clear, style, color). Every session that
 * asks for MP_TERM_CAP_VT100 loads this. Richer dialects (xterm, OSC,
 * and future ones) are separate drivers in this directory.
 * @{
 */

#include <mp.h>

#ifdef SUPPORT_COMMON_TERM
#ifdef MP_TERM_HAVE_VT100

#include <term/term.h>
#include "../term_internal.h"

extern int mp_term_nearest16(unsigned char r, unsigned char g, unsigned char b);
extern int mp_term_nearest256(unsigned char r, unsigned char g, unsigned char b);

/* the tilde codes CSI <n>~ maps to, when n is in range */
static const int _mp_term_tilde[] = {
	0, MP_TERM_KEY_HOME, 0, MP_TERM_KEY_DELETE,
	MP_TERM_KEY_END, MP_TERM_KEY_PAGE_UP, MP_TERM_KEY_PAGE_DOWN,
};

/* parse: a CSI final byte. Reads term->params / term->private_. */
static mp_bool_t _mp_term_vt100_csi(mp_term_t *term, mp_term_proto_t *self, unsigned char c) {
	int a = _mp_term_param(term, 0);

	(void)self;
	if(term->private_)
		return(FALSE); /* DEC private modes belong to other drivers */

	switch(c) {
		case 'A': _mp_term_key(term, MP_TERM_KEY_UP); return(TRUE);
		case 'B': _mp_term_key(term, MP_TERM_KEY_DOWN); return(TRUE);
		case 'C': _mp_term_key(term, MP_TERM_KEY_RIGHT); return(TRUE);
		case 'D': _mp_term_key(term, MP_TERM_KEY_LEFT); return(TRUE);
		case 'F': _mp_term_key(term, MP_TERM_KEY_END); return(TRUE);
		case 'H':
			/* Home with no params; a cursor move the host sent we ignore as input */
			if(term->paramCount == 0)
				_mp_term_key(term, MP_TERM_KEY_HOME);
			return(TRUE);
		case '~':
			if(a > 0 && a < (int)(sizeof(_mp_term_tilde) / sizeof(_mp_term_tilde[0])) && _mp_term_tilde[a])
				_mp_term_key(term, _mp_term_tilde[a]);
			return(TRUE);
		case 'R':
			/* CPR. The UTF-8 probe prints one box character at column 1, so
			 * its answer is row 1 and column 2, or column 4 when the peer
			 * counts the three bytes. A reset drops the first bytes and the
			 * size report arrives instead: a wide column must not latch
			 * "no UTF-8" and throw the size away. */
			if(term->probeUtf8 && term->paramCount >= 2 &&
					_mp_term_param(term, 0) == 1 &&
					(_mp_term_param(term, 1) == 2 || _mp_term_param(term, 1) == 4)) {
				term->probeUtf8 = NO;
				term->utf8 = _mp_term_param(term, 1) == 2 ? YES : NO;
				term->utf8Known = YES;
				return(TRUE);
			}
			/* After a corner probe the clamped position is the glass. */
			if(term->probe && term->paramCount >= 2) {
				mp_term_event_t event;
				int rows = _mp_term_param(term, 0);
				int cols = _mp_term_param(term, 1);

				term->probe = NO;
				if(rows > 0 && cols > 0) {
					term->rows = rows;
					term->cols = cols;
					_mp_term_flush_text(term);
					memset(&event, 0, sizeof(event));
					event.type = MP_TERM_EVENT_RESIZE;
					event.x = rows;
					event.y = cols;
					_mp_term_push(term, &event);
				}
			}
			return(TRUE);
	}
	return(FALSE);
}

void _mp_term_vt100_emit(mp_term_t *term, const unsigned char *b, int n) {
	_mp_term_emit(term, b, n);
}

void _mp_term_vt100_lit(mp_term_t *term, const char *s) {
	_mp_term_emit(term, (const unsigned char *)s, (int)strlen(s));
}

/* 0..7 is base+index (30/40); 8..15 is bright+(index-8) (90/100) */
void _mp_term_vt100_sgr16(mp_term_t *term, int index, int base, int bright) {
	unsigned char buf[8];
	int n;

	if(index < 0 || index > 15)
		return;
	if(index < 8)
		n = (int)snprintf((char *)buf, sizeof(buf), "\033[%dm", base + index);
	else
		n = (int)snprintf((char *)buf, sizeof(buf), "\033[%dm", bright + (index - 8));
	if(n > 0)
		_mp_term_vt100_emit(term, buf, n);
}

mp_bool_t _mp_term_vt100_move(mp_term_t *term, mp_term_proto_t *self, int row, int col) {
	unsigned char buf[24];
	int n;

	(void)self;
	if(row <= 0 && col <= 0)
		n = (int)snprintf((char *)buf, sizeof(buf), "\033[;H");
	else if(row <= 0)
		n = (int)snprintf((char *)buf, sizeof(buf), "\033[;%dH", col);
	else if(col <= 0)
		n = (int)snprintf((char *)buf, sizeof(buf), "\033[%d;H", row);
	else
		n = (int)snprintf((char *)buf, sizeof(buf), "\033[%d;%dH", row, col);
	if(n > 0)
		_mp_term_vt100_emit(term, buf, n);
	return(TRUE);
}

mp_bool_t _mp_term_vt100_clear(mp_term_t *term, mp_term_proto_t *self) {
	(void)self;
	_mp_term_vt100_lit(term, "\033[2J");
	_mp_term_vt100_move(term, NULL, 1, 1);
	return(TRUE);
}

mp_bool_t _mp_term_vt100_clear_line(mp_term_t *term, mp_term_proto_t *self) {
	(void)self;
	_mp_term_vt100_lit(term, "\033[2K");
	return(TRUE);
}

mp_bool_t _mp_term_vt100_style(mp_term_t *term, mp_term_proto_t *self, unsigned int flags) {
	static const unsigned int bits[5] = {
		MP_TERM_STYLE_BOLD, MP_TERM_STYLE_ITALIC,
		MP_TERM_STYLE_UNDERLINE, MP_TERM_STYLE_REVERSE,
		MP_TERM_STYLE_DIM,
	};
	static const char code[5] = {'1', '3', '4', '7', '2'};
	unsigned char buf[16];
	int n = 2;
	int i;

	(void)self;
	if(flags == MP_TERM_STYLE_RESET) {
		_mp_term_vt100_lit(term, "\033[0m");
		return(TRUE);
	}
	buf[0] = 0x1b;
	buf[1] = '[';
	for(i = 0; i < 5; i++) {
		if((flags & bits[i]) == 0)
			continue;
		if(n > 2)
			buf[n++] = ';';
		buf[n++] = (unsigned char)code[i];
	}
	if(n == 2)
		return(TRUE);
	buf[n++] = 'm';
	_mp_term_vt100_emit(term, buf, n);
	return(TRUE);
}

mp_bool_t _mp_term_vt100_fg(mp_term_t *term, mp_term_proto_t *self, int index) {
	(void)self;
	if(term->colorLevel == MP_TERM_COLOR_NONE)
		return(TRUE);
	_mp_term_vt100_sgr16(term, index, 30, 90);
	return(TRUE);
}

mp_bool_t _mp_term_vt100_bg(mp_term_t *term, mp_term_proto_t *self, int index) {
	(void)self;
	if(term->colorLevel == MP_TERM_COLOR_NONE)
		return(TRUE);
	_mp_term_vt100_sgr16(term, index, 40, 100);
	return(TRUE);
}

mp_bool_t _mp_term_vt100_fg_rgb(mp_term_t *term, mp_term_proto_t *self, unsigned char r, unsigned char g, unsigned char b) {
	unsigned char buf[24];
	int n;

	(void)self;
	switch(term->colorLevel) {
	case MP_TERM_COLOR_NONE:
		return(TRUE);
	case MP_TERM_COLOR_16:
		_mp_term_vt100_sgr16(term, mp_term_nearest16(r, g, b), 30, 90);
		return(TRUE);
	case MP_TERM_COLOR_256:
		n = (int)snprintf((char *)buf, sizeof(buf), "\033[38;5;%dm", mp_term_nearest256(r, g, b));
		break;
	case MP_TERM_COLOR_TRUE:
	default:
		n = (int)snprintf((char *)buf, sizeof(buf), "\033[38;2;%d;%d;%dm", (int)r, (int)g, (int)b);
		break;
	}
	if(n > 0)
		_mp_term_vt100_emit(term, buf, n);
	return(TRUE);
}

mp_bool_t _mp_term_vt100_bg_rgb(mp_term_t *term, mp_term_proto_t *self, unsigned char r, unsigned char g, unsigned char b) {
	unsigned char buf[24];
	int n;

	(void)self;
	switch(term->colorLevel) {
	case MP_TERM_COLOR_NONE:
		return(TRUE);
	case MP_TERM_COLOR_16:
		_mp_term_vt100_sgr16(term, mp_term_nearest16(r, g, b), 40, 100);
		return(TRUE);
	case MP_TERM_COLOR_256:
		n = (int)snprintf((char *)buf, sizeof(buf), "\033[48;5;%dm", mp_term_nearest256(r, g, b));
		break;
	case MP_TERM_COLOR_TRUE:
	default:
		n = (int)snprintf((char *)buf, sizeof(buf), "\033[48;2;%d;%d;%dm", (int)r, (int)g, (int)b);
		break;
	}
	if(n > 0)
		_mp_term_vt100_emit(term, buf, n);
	return(TRUE);
}

const mp_term_proto_ops_t mp_term_proto_vt100 = {
	"vt100",
	MP_TERM_CAP_VT100,
	_mp_term_vt100_csi,
	NULL,                       /* esc: no VT100 single-shifts we treat as input */
	NULL,                       /* osc */
	_mp_term_vt100_move,
	_mp_term_vt100_clear,
	_mp_term_vt100_clear_line,
	_mp_term_vt100_style,
	_mp_term_vt100_fg,
	_mp_term_vt100_bg,
	_mp_term_vt100_fg_rgb,
	_mp_term_vt100_bg_rgb,
	NULL, NULL,                  /* title, link: OSC */
	NULL, NULL,                  /* mouse, alt: xterm */
	NULL, NULL, NULL, NULL,      /* region, insert_line, delete_line, scroll: VT102 */
};

#endif
#endif
