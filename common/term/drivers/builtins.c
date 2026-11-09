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
 * The wiring file. This is the only place that knows which protocol
 * drivers are compiled in; the core and the FSM never reference a
 * dialect by name. Adding a protocol is one entry here and one .c file
 * in this directory.
 * @{
 */

#include <mp.h>

#ifdef SUPPORT_COMMON_TERM

#include <term/term.h>

#ifdef MP_TERM_HAVE_VT100
	extern const mp_term_proto_ops_t mp_term_proto_vt100;
#endif
#ifdef MP_TERM_HAVE_VT102
	extern const mp_term_proto_ops_t mp_term_proto_vt102;
#endif
#ifdef MP_TERM_HAVE_XTERM
	extern const mp_term_proto_ops_t mp_term_proto_xterm;
#endif
#ifdef MP_TERM_HAVE_OSC
	extern const mp_term_proto_ops_t mp_term_proto_osc;
#endif

/* order is priority: the base dialect first, richer ones after. The core
 * stops at the first driver that claims a sequence or an intent. */
const mp_term_proto_ops_t *const _mp_term_proto_builtins[] = {
#ifdef MP_TERM_HAVE_VT100
	&mp_term_proto_vt100,
#endif
#ifdef MP_TERM_HAVE_VT102
	&mp_term_proto_vt102,
#endif
#ifdef MP_TERM_HAVE_XTERM
	&mp_term_proto_xterm,
#endif
#ifdef MP_TERM_HAVE_OSC
	&mp_term_proto_osc,
#endif
};

const int _mp_term_proto_builtins_count =
	(int)(sizeof(_mp_term_proto_builtins) / sizeof(_mp_term_proto_builtins[0]));

/** @} */

#endif
