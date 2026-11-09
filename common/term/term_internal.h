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

/* internal glue between the core (term.c, parse.c) and the protocol drivers.
 * Not part of the public API; not included by callers. */

#ifndef _HAVE_MP_TERM_INTERNAL_H
	#define _HAVE_MP_TERM_INTERNAL_H

	#ifdef SUPPORT_COMMON_TERM

		/* one FSM step. term.c calls it from mp_term_rx. */
		void _mp_term_parse(mp_term_t *term, unsigned char c);

		/* push an event on the ring (term.c). Drops when full. */
		void _mp_term_push(mp_term_t *term, const mp_term_event_t *event);

		/* accumulate a ground byte; flushes a TEXT event on state change */
		void _mp_term_text(mp_term_t *term, unsigned char c);
		void _mp_term_flush_text(mp_term_t *term);

		/* flush text then push a KEY event */
		void _mp_term_key(mp_term_t *term, int key);

		/* the lone-Escape timeout fired */
		void _mp_term_escape(mp_term_t *term);

		/* the only byte sink; drivers and the FSM write through it */
		void _mp_term_emit(mp_term_t *term, const unsigned char *bytes, int n);

		/* read a CSI parameter (0 when absent); drivers call this */
		int _mp_term_param(const mp_term_t *term, int index);

		/* a CSI final byte arrived (params/private_ already parsed).
		 * returns TRUE if a driver claimed it. */
		mp_bool_t _mp_term_dispatch_csi(mp_term_t *term, unsigned char final);

		/* an ESC intermediate byte arrived (not '[' / ']').
		 * returns TRUE if a driver claimed it. */
		mp_bool_t _mp_term_dispatch_esc(mp_term_t *term, unsigned char c);

		/* an OSC string ended (buf/len is the raw payload).
		 * returns TRUE if a driver claimed it. */
		mp_bool_t _mp_term_dispatch_osc(mp_term_t *term, const unsigned char *buf, int len);

	/* the built-in protocols compiled into this build. term.c registers
	 * the ones whose cap is requested at init. */
	extern const mp_term_proto_ops_t *const _mp_term_proto_builtins[];
	extern const int _mp_term_proto_builtins_count;

	/* vt100 emit helpers, shared with the vt102 driver (a VT102 superset
	 * that reuses the VT100 SGR emitters and adds the screen ops). */
	void _mp_term_vt100_emit(mp_term_t *term, const unsigned char *b, int n);
	void _mp_term_vt100_lit(mp_term_t *term, const char *s);
	void _mp_term_vt100_sgr16(mp_term_t *term, int index, int base, int bright);
	mp_bool_t _mp_term_vt100_move(mp_term_t *term, mp_term_proto_t *self, int row, int col);
	mp_bool_t _mp_term_vt100_clear(mp_term_t *term, mp_term_proto_t *self);
	mp_bool_t _mp_term_vt100_clear_line(mp_term_t *term, mp_term_proto_t *self);
	mp_bool_t _mp_term_vt100_style(mp_term_t *term, mp_term_proto_t *self, unsigned int flags);
	mp_bool_t _mp_term_vt100_fg(mp_term_t *term, mp_term_proto_t *self, int index);
	mp_bool_t _mp_term_vt100_bg(mp_term_t *term, mp_term_proto_t *self, int index);
	mp_bool_t _mp_term_vt100_fg_rgb(mp_term_t *term, mp_term_proto_t *self, unsigned char r, unsigned char g, unsigned char b);
	mp_bool_t _mp_term_vt100_bg_rgb(mp_term_t *term, mp_term_proto_t *self, unsigned char r, unsigned char g, unsigned char b);

	#endif

#endif
