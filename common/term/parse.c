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
 * The parser. One ECMA-48 state machine; the dialects are protocol drivers
 * in drivers/, one file each, called back at the decision points. The
 * session (term.c) owns the event ring and the text accumulator; this
 * file only steps the FSM and dispatches.
 * @{
 */

#include <mp.h>

#ifdef SUPPORT_COMMON_TERM

#include <term/term.h>
#include "term_internal.h"

int _mp_term_param(const mp_term_t *term, int index) {
	if(index < 0 || index >= term->paramCount || index >= MP_TERM_PARAMS)
		return(0);
	return(term->params[index]);
}

static void _mp_term_param_put(mp_term_t *term, unsigned char c) {
	if(c >= '0' && c <= '9') {
		if(!term->paramOpen && term->paramCount < MP_TERM_PARAMS) {
			term->paramCount++;
			term->paramOpen = YES;
		}
		if(term->paramOpen)
			term->params[term->paramCount - 1] =
				term->params[term->paramCount - 1] * 10 + (c - '0');
		return;
	}
	if(c == ';') {
		/* an empty slot still counts as a param: CSI 1;;3 has three */
		if(!term->paramOpen && term->paramCount < MP_TERM_PARAMS)
			term->paramCount++;
		term->paramOpen = NO;
		return;
	}
}

void _mp_term_parse(mp_term_t *term, unsigned char c) {
	switch(term->state) {
	case _MP_TERM_GROUND:
		term->escape = 0;
		if(c == 0x1b) {
			_mp_term_flush_text(term);
			term->state = _MP_TERM_ESC;
			term->escape = 1;
			term->escapeTick = mp_clock_ticks();
			return;
		}
		if(c == '\r' || c == '\n')
			_mp_term_key(term, MP_TERM_KEY_ENTER);
		else if(c == 0x7f || c == 0x08)
			_mp_term_key(term, MP_TERM_KEY_BACKSPACE);
		else if(c == '\t')
			_mp_term_key(term, MP_TERM_KEY_TAB);
		else if(c >= 0x20)
			_mp_term_text(term, c);
		else if(c != 0)
			_mp_term_key(term, c); /* Ctrl+letter: the control code is the key */
		return;

	case _MP_TERM_ESC:
		term->escape = 0; /* a real sequence follows */
		if(c == '[') {
			term->state = _MP_TERM_CSI;
			term->paramCount = 0;
			term->paramOpen = NO;
			term->private_ = NO;
			memset(term->params, 0, sizeof(term->params));
			return;
		}
		if(c == ']') {
			term->state = _MP_TERM_OSC;
			term->oscLen = 0;
			term->oscString = NO;
			return;
		}
		if(c == 'O') { /* SS3: ESC O then one final byte */
			term->state = _MP_TERM_SS3;
			return;
		}
		/* ESC <something else>: hand it to the drivers; if none claims it,
		 * treat it as a lone Escape and re-feed the byte so it is not lost. */
		if(_mp_term_dispatch_esc(term, c)) {
			term->state = _MP_TERM_GROUND;
			return;
		}
		/* Alt+key. Minicom sends ESC then the character. ESC O is SS3. */
		if((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
				(c >= 'A' && c <= 'Z' && c != 'O')) {
			term->state = _MP_TERM_GROUND;
			_mp_term_key(term, MP_TERM_KEY_ALT(c));
			return;
		}
		term->state = _MP_TERM_GROUND;
		_mp_term_key(term, MP_TERM_KEY_ESCAPE);
		_mp_term_parse(term, c);
		return;

	case _MP_TERM_CSI:
		if(c == '?' || c == '<') {
			term->private_ = YES;
			return;
		}
		if(c >= '0' && c <= '9') {
			_mp_term_param_put(term, c);
			return;
		}
		if(c == ';') {
			_mp_term_param_put(term, c);
			return;
		}
		/* intermediate or final byte */
		term->state = _MP_TERM_GROUND;
		_mp_term_dispatch_csi(term, c);
		return;

	case _MP_TERM_SS3:
		/* a direct terminal may send cursor keys this way. Without this,
		 * ESC O B reads as Escape then "OB" and closes the menu it just
		 * opened. Minicom rewrites these to CSI, others do not. */
		term->state = _MP_TERM_GROUND;
		switch(c) {
		case 'A': _mp_term_key(term, MP_TERM_KEY_UP); return;
		case 'B': _mp_term_key(term, MP_TERM_KEY_DOWN); return;
		case 'C': _mp_term_key(term, MP_TERM_KEY_RIGHT); return;
		case 'D': _mp_term_key(term, MP_TERM_KEY_LEFT); return;
		case 'F': _mp_term_key(term, MP_TERM_KEY_END); return;
		case 'H': _mp_term_key(term, MP_TERM_KEY_HOME); return;
		default: return;
		}

	case _MP_TERM_OSC:
		if(c == 0x07) {
			term->state = _MP_TERM_GROUND;
			_mp_term_dispatch_osc(term, term->osc, term->oscLen);
			term->oscLen = 0;
			return;
		}
		if(c == 0x1b) {
			term->oscString = YES;
			return;
		}
		if(term->oscString && c == '\\') {
			term->state = _MP_TERM_GROUND;
			term->oscString = NO;
			_mp_term_dispatch_osc(term, term->osc, term->oscLen);
			term->oscLen = 0;
			return;
		}
		if(term->oscLen < (int)sizeof(term->osc))
			term->osc[term->oscLen++] = c;
		return;

	default:
		term->state = _MP_TERM_GROUND;
		return;
	}
}

/**@}*/

#endif
