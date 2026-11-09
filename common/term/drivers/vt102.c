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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02130-1301  USA       *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**
 * @defgroup mpCommonTerm Terminal codec
 * @ingroup mpCommon
 *
 * The VT102 protocol driver. VT102 is a VT100 superset: the same
 * cursor keys and tilde table (the vt100 driver already parses those)
 * plus the screen ops a line-based TUI needs -- a scrolling region
 * (DECSTBM), insert/delete line and scroll up/down. The emitters
 * reuse the VT100 SGR helpers; this file only adds the screen ops and
 * claims the few ESC / DEC-private modes that VT102 defines, so they
 * are not misread as a lone Escape or an unknown CSI.
 * @{
 */

#include <mp.h>

#ifdef SUPPORT_COMMON_TERM
#ifdef MP_TERM_HAVE_VT102

#include <term/term.h>
#include "../term_internal.h"

/* parse: a CSI final byte. The base cursor keys (A/B/C/D/F/H/~) belong to
 * the vt100 driver; we only answer for the DEC-private modes VT102 adds. */
static mp_bool_t _mp_term_vt102_csi(mp_term_t *term, mp_term_proto_t *self, unsigned char c) {
	int a, b;

	(void)self;
	if(!term->private_)
		return(FALSE); /* ANSI modes belong to the vt100 driver */

	switch(c) {
	case 'h': /* DECSET: ?1 cursor-key app mode, ?6 origin mode, ?7 autowrap */
		/* sink-only: a terminal does not act on these; swallow cleanly */
		return(TRUE);
	case 'l': /* DECRST: ?1, ?6, ?7 off */
		return(TRUE);
	case 'r': /* DECSTBM: set scrolling region. top;bottom, 0;0 = full */
		/* the host sends this to configure us; we do not keep a region, so
		 * accept and ignore rather than report an unknown CSI. */
		a = _mp_term_param(term, 0);
		b = _mp_term_param(term, 1);
		(void)a; (void)b;
		return(TRUE);
	}
	return(FALSE);
}

/* parse: an ESC intermediate. Claim DECKPAM / DECKPNM so a stray ESC = or
 * ESC > is not reported as a lone Escape followed by a text byte. */
static mp_bool_t _mp_term_vt102_esc(mp_term_t *term, mp_term_proto_t *self, unsigned char c) {
	(void)term;
	(void)self;
	if(c == '=' || c == '>') /* DECKPAM / DECKPNM */
		return(TRUE);
	return(FALSE);
}

/* emit: DECSTBM. top/bottom <= 0 resets to the full screen. */
static mp_bool_t _mp_term_vt102_region(mp_term_t *term, mp_term_proto_t *self, int top, int bottom) {
	unsigned char buf[24];
	int n;

	(void)self;
	if(top <= 0 && bottom <= 0)
		n = (int)snprintf((char *)buf, sizeof(buf), "\033[r");
	else if(top <= 0)
		n = (int)snprintf((char *)buf, sizeof(buf), "\033[;%dr", bottom);
	else if(bottom <= 0)
		n = (int)snprintf((char *)buf, sizeof(buf), "\033[%dr", top);
	else
		n = (int)snprintf((char *)buf, sizeof(buf), "\033[%d;%dr", top, bottom);
	if(n > 0)
		_mp_term_vt100_emit(term, buf, n);
	return(TRUE);
}

/* emit: insert n blank lines at the cursor (default 1). */
static mp_bool_t _mp_term_vt102_insert_line(mp_term_t *term, mp_term_proto_t *self, int n) {
	unsigned char buf[16];
	int len;

	(void)self;
	if(n <= 1)
		_mp_term_vt100_lit(term, "\033[L");
	else {
		len = (int)snprintf((char *)buf, sizeof(buf), "\033[%dL", n);
		if(len > 0)
			_mp_term_vt100_emit(term, buf, len);
	}
	return(TRUE);
}

/* emit: delete n lines at the cursor (default 1). */
static mp_bool_t _mp_term_vt102_delete_line(mp_term_t *term, mp_term_proto_t *self, int n) {
	unsigned char buf[16];
	int len;

	(void)self;
	if(n <= 1)
		_mp_term_vt100_lit(term, "\033[M");
	else {
		len = (int)snprintf((char *)buf, sizeof(buf), "\033[%dM", n);
		if(len > 0)
			_mp_term_vt100_emit(term, buf, len);
	}
	return(TRUE);
}

/* emit: scroll up (n > 0) or down (n < 0) n lines inside the region. */
static mp_bool_t _mp_term_vt102_scroll(mp_term_t *term, mp_term_proto_t *self, int n) {
	unsigned char buf[16];
	int len;

	(void)self;
	if(n == 0)
		return(TRUE);
	if(n > 0)
		len = (int)snprintf((char *)buf, sizeof(buf), "\033[%dS", n);
	else
		len = (int)snprintf((char *)buf, sizeof(buf), "\033[%dT", -n);
	if(len > 0)
		_mp_term_vt100_emit(term, buf, len);
	return(TRUE);
}

const mp_term_proto_ops_t mp_term_proto_vt102 = {
	"vt102",
	MP_TERM_CAP_VT102,
	_mp_term_vt102_csi,
	_mp_term_vt102_esc,
	NULL,                       /* osc: OSC driver owns it */
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
	_mp_term_vt102_region,
	_mp_term_vt102_insert_line,
	_mp_term_vt102_delete_line,
	_mp_term_vt102_scroll,
};

#endif
#endif
