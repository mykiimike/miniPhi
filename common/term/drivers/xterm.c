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
 * The xterm protocol driver: SGR mouse (1006), bracketed paste (2004),
 * the resize reply, and the alt-screen / mouse-mode emitters.
 * @{
 */

#include <mp.h>

#ifdef SUPPORT_COMMON_TERM
#ifdef MP_TERM_HAVE_XTERM

#include <term/term.h>
#include "../term_internal.h"

static mp_bool_t _mp_term_xterm_csi(mp_term_t *term, mp_term_proto_t *self, unsigned char c) {
	int a = _mp_term_param(term, 0);
	int b = _mp_term_param(term, 1);

	(void)self;

	if(term->private_) {
		/* mouse 1006: CSI <button;col;row M or m */
		if(c == 'M' || c == 'm') {
			mp_term_event_t event;
			_mp_term_flush_text(term);
			memset(&event, 0, sizeof(event));
			event.type = MP_TERM_EVENT_MOUSE;
			event.x = b;
			event.y = _mp_term_param(term, 2);
			event.value = (a & 7) | (c == 'm' ? 0x10 : 0);
			_mp_term_push(term, &event);
			return(TRUE);
		}
		/* bracketed paste: ?2004h starts, ?2004l ends */
		if((c == 'h' || c == 'l') && a == 2004) {
			mp_term_event_t event;
			_mp_term_flush_text(term);
			memset(&event, 0, sizeof(event));
			event.type = MP_TERM_EVENT_PASTE;
			event.value = (c == 'h') ? 1 : 0;
			_mp_term_push(term, &event);
			return(TRUE);
		}
		return(FALSE);
	}

	/* non-private: reply to CSI 18t is CSI 8 ; rows ; cols t */
	if(c == 't' && term->paramCount >= 3) {
		mp_term_event_t event;
		_mp_term_flush_text(term);
		memset(&event, 0, sizeof(event));
		event.type = MP_TERM_EVENT_RESIZE;
		event.x = b;                 /* rows */
		event.y = _mp_term_param(term, 2); /* cols */
		if(event.x > 0 && event.y > 0) {
			term->rows = event.x;
			term->cols = event.y;
		}
		term->probe = NO;
		_mp_term_push(term, &event);
		return(TRUE);
	}

	return(FALSE);
}

static void _mp_term_xterm_lit(mp_term_t *term, const char *s) {
	_mp_term_emit(term, (const unsigned char *)s, (int)strlen(s));
}

static mp_bool_t _mp_term_xterm_mouse(mp_term_t *term, mp_term_proto_t *self, int on) {
	(void)self;
	_mp_term_xterm_lit(term, on ? "\033[?1000;1002;1006h" : "\033[?1006;1002;1000l");
	return(TRUE);
}

static mp_bool_t _mp_term_xterm_alt(mp_term_t *term, mp_term_proto_t *self, int on) {
	(void)self;
	_mp_term_xterm_lit(term, on ? "\033[?1049h" : "\033[?1049l");
	return(TRUE);
}

const mp_term_proto_ops_t mp_term_proto_xterm = {
	"xterm",
	MP_TERM_CAP_XTERM,
	_mp_term_xterm_csi,
	NULL, NULL,                       /* esc, osc */
	NULL, NULL, NULL, NULL,           /* move, clear, clear_line, style */
	NULL, NULL, NULL, NULL,           /* fg, bg, fg_rgb, bg_rgb */
	NULL, NULL,                       /* title, link: OSC */
	_mp_term_xterm_mouse,
	_mp_term_xterm_alt,
	NULL, NULL, NULL, NULL,          /* region, insert_line, delete_line, scroll: VT102 */
};

#endif
#endif
