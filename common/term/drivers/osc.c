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
 * The OSC protocol driver: the title and hyperlink strings, both as
 * received events and as emitters. The payload after the command number
 * and ';' is delivered as an MP_TERM_EVENT_OSC.
 * @{
 */

#include <mp.h>

#ifdef SUPPORT_COMMON_TERM
#ifdef MP_TERM_HAVE_OSC

#include <term/term.h>
#include "../term_internal.h"

static mp_bool_t _mp_term_osc_osc(mp_term_t *term, mp_term_proto_t *self, const unsigned char *buf, int len) {
	int start = 0;

	(void)self;
	/* the payload starts after the command number and a ';' */
	while(start < len && buf[start] != ';')
		start++;
	start++;
	if(start >= len)
		return(TRUE);

	{
		mp_term_event_t event;
		_mp_term_flush_text(term);
		memset(&event, 0, sizeof(event));
		event.type = MP_TERM_EVENT_OSC;
		event.bytes = buf + start;
		event.len = len - start;
		_mp_term_push(term, &event);
	}
	return(TRUE);
}

static mp_bool_t _mp_term_osc_title(mp_term_t *term, mp_term_proto_t *self, const char *title) {
	(void)self;
	if(title == NULL)
		return(TRUE);
	_mp_term_emit(term, (const unsigned char *)"\033]0;", 4);
	_mp_term_emit(term, (const unsigned char *)title, (int)strlen(title));
	_mp_term_emit(term, (const unsigned char *)"\007", 1);
	return(TRUE);
}

static mp_bool_t _mp_term_osc_link(mp_term_t *term, mp_term_proto_t *self, const char *uri, const char *label) {
	(void)self;
	if(uri == NULL)
		return(TRUE);
	_mp_term_emit(term, (const unsigned char *)"\033]8;;", 5);
	_mp_term_emit(term, (const unsigned char *)uri, (int)strlen(uri));
	_mp_term_emit(term, (const unsigned char *)"\033\\", 2);
	if(label)
		_mp_term_emit(term, (const unsigned char *)label, (int)strlen(label));
	_mp_term_emit(term, (const unsigned char *)"\033]8;;\033\\", 7);
	return(TRUE);
}

const mp_term_proto_ops_t mp_term_proto_osc = {
	"osc",
	MP_TERM_CAP_OSC,
	NULL, NULL,                       /* csi, esc */
	_mp_term_osc_osc,
	NULL, NULL, NULL, NULL,            /* move, clear, clear_line, style */
	NULL, NULL, NULL, NULL,           /* fg, bg, fg_rgb, bg_rgb */
	_mp_term_osc_title,
	_mp_term_osc_link,
	NULL, NULL,                       /* mouse, alt: xterm */
	NULL, NULL, NULL, NULL,          /* region, insert_line, delete_line, scroll: VT102 */
};

#endif
#endif
