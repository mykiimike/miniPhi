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
 * The session: the event ring, the text accumulator, the lone-Escape
 * timer and the protocol driver registry. The FSM lives in parse.c and
 * the dialects live in drivers/, one file each; both call back here. The
 * struct is caller-owned and never allocated.
 * @{
 */

#include <mp.h>

#ifdef SUPPORT_COMMON_TERM

#include <term/term.h>
#include "term_internal.h"

static unsigned long _mp_term_ms(unsigned long ms) {
	unsigned long ticks = (mp_clock_tick_hz() * ms) / 1000;

	return(ticks ? ticks : 1);
}

/* the byte pipe is the only output path */
void _mp_term_emit(mp_term_t *term, const unsigned char *bytes, int n) {
	if(term->write)
		term->write(term->user, bytes, n);
}

mp_ret_t mp_term_init(mp_kernel_t *kernel, mp_term_t *term,
	mp_term_write_t write, void *user,
	unsigned int caps, mp_term_color_t colorLevel) {

	int i;

	memset(term, 0, sizeof(*term));
	term->kernel = kernel;
	term->write = write;
	term->user = user;

	/* clamp the color level to the build's max */
	if((int)colorLevel > (int)MP_TERM_COLOR)
		colorLevel = MP_TERM_COLOR;
	term->colorLevel = colorLevel;

	term->state = _MP_TERM_GROUND;
	term->escape = 0;
	term->oscLen = 0;
	term->textLen = 0;
	term->head = term->tail = 0;
	mp_list_init(&term->protos);

	/* auto-register the protocols compiled into this build whose cap
	 * the caller asked for. caps ends up the set that actually loaded. */
	for(i = 0; i < _mp_term_proto_builtins_count && i < MP_TERM_PROTO_MAX; i++) {
		const mp_term_proto_ops_t *ops = _mp_term_proto_builtins[i];

		if(ops == NULL || (caps & ops->cap) == 0)
			continue;
		/* the builtins are stateless ops tables; embed each in its own
		 * mp_term_proto_t kept in this session and pass the ops back as self. */
		mp_term_proto_register(term, &term->_protoSlots[i], ops, (void *)ops);
	}

	mp_printk("Initialize Terminal: caps=0x%x color=%d", (unsigned)term->caps, (int)colorLevel);
	return(TRUE);
}

void mp_term_caps(mp_term_t *term, unsigned int caps) {
	/* drop drivers whose cap is no longer requested */
	mp_list_item_t *item = term->protos.first;

	while(item) {
		mp_term_proto_t *proto = item->user;
		mp_list_item_t *next = item->next;

		if((caps & proto->ops->cap) == 0)
			mp_term_proto_unregister(term, proto);
		item = next;
	}
}

void mp_term_color(mp_term_t *term, mp_term_color_t colorLevel) {
	if((int)colorLevel > (int)MP_TERM_COLOR)
		colorLevel = MP_TERM_COLOR;
	term->colorLevel = colorLevel;
}

unsigned int mp_term_caps_get(const mp_term_t *term) {
	return(term->caps);
}

mp_term_color_t mp_term_color_get(const mp_term_t *term) {
	return(term->colorLevel);
}

/* the protocol registry */

mp_ret_t mp_term_proto_register(mp_term_t *term, mp_term_proto_t *proto,
	const mp_term_proto_ops_t *ops, void *self) {

	if(!term || !proto || !ops)
		return(FALSE);
	if(proto->ops != NULL)
		return(FALSE);            /* already registered */

	proto->ops = ops;
	proto->self = self;
	mp_list_add_last(&term->protos, &proto->item, proto);
	term->caps |= ops->cap;
	return(TRUE);
}

mp_ret_t mp_term_proto_unregister(mp_term_t *term, mp_term_proto_t *proto) {
	if(!term || !proto || proto->ops == NULL)
		return(FALSE);

	term->caps &= ~proto->ops->cap;
	mp_list_remove(&term->protos, &proto->item);
	proto->ops = NULL;
	proto->self = NULL;
	return(TRUE);
}

/* dispatch: walk the registered drivers in order, first that claims wins */

mp_bool_t _mp_term_dispatch_csi(mp_term_t *term, unsigned char final) {
	mp_list_item_t *item;

	for(item = term->protos.first; item; item = item->next) {
		mp_term_proto_t *p = item->user;

		if(p->ops->csi && p->ops->csi(term, p, final))
			return(TRUE);
	}
	return(FALSE);
}

mp_bool_t _mp_term_dispatch_esc(mp_term_t *term, unsigned char c) {
	mp_list_item_t *item;

	for(item = term->protos.first; item; item = item->next) {
		mp_term_proto_t *p = item->user;

		if(p->ops->esc && p->ops->esc(term, p, c))
			return(TRUE);
	}
	return(FALSE);
}

mp_bool_t _mp_term_dispatch_osc(mp_term_t *term, const unsigned char *buf, int len) {
	mp_list_item_t *item;

	for(item = term->protos.first; item; item = item->next) {
		mp_term_proto_t *p = item->user;

		if(p->ops->osc && p->ops->osc(term, p, buf, len))
			return(TRUE);
	}
	return(FALSE);
}

/* the public emitters: each dispatches to the first driver that handles it */

#define _mp_term_emit_dispatch(field, call) \
	do { \
		mp_list_item_t *_item; \
		for(_item = term->protos.first; _item; _item = _item->next) { \
			mp_term_proto_t *_p = _item->user; \
			if(_p->ops->field && _p->ops->field call) \
				return; \
		} \
	} while(0)

void mp_term_move(mp_term_t *term, int row, int col) {
	_mp_term_emit_dispatch(move, (term, _p, row, col));
}
void mp_term_clear(mp_term_t *term) {
	_mp_term_emit_dispatch(clear, (term, _p));
}
void mp_term_clear_line(mp_term_t *term) {
	_mp_term_emit_dispatch(clear_line, (term, _p));
}
void mp_term_style(mp_term_t *term, unsigned int flags) {
	_mp_term_emit_dispatch(style, (term, _p, flags));
}
void mp_term_fg(mp_term_t *term, int index) {
	_mp_term_emit_dispatch(fg, (term, _p, index));
}
void mp_term_bg(mp_term_t *term, int index) {
	_mp_term_emit_dispatch(bg, (term, _p, index));
}
void mp_term_fg_rgb(mp_term_t *term, unsigned char r, unsigned char g, unsigned char b) {
	_mp_term_emit_dispatch(fg_rgb, (term, _p, r, g, b));
}
void mp_term_bg_rgb(mp_term_t *term, unsigned char r, unsigned char g, unsigned char b) {
	_mp_term_emit_dispatch(bg_rgb, (term, _p, r, g, b));
}
void mp_term_title(mp_term_t *term, const char *title) {
	_mp_term_emit_dispatch(title, (term, _p, title));
}
void mp_term_link(mp_term_t *term, const char *uri, const char *label) {
	_mp_term_emit_dispatch(link, (term, _p, uri, label));
}
void mp_term_mouse(mp_term_t *term, int on) {
	_mp_term_emit_dispatch(mouse, (term, _p, on));
}
void mp_term_alt(mp_term_t *term, int on) {
	_mp_term_emit_dispatch(alt, (term, _p, on));
}

void mp_term_region(mp_term_t *term, int top, int bottom) {
	_mp_term_emit_dispatch(region, (term, _p, top, bottom));
}
void mp_term_insert_line(mp_term_t *term, int n) {
	_mp_term_emit_dispatch(insert_line, (term, _p, n));
}
void mp_term_delete_line(mp_term_t *term, int n) {
	_mp_term_emit_dispatch(delete_line, (term, _p, n));
}
void mp_term_scroll(mp_term_t *term, int n) {
	_mp_term_emit_dispatch(scroll, (term, _p, n));
}

/* raw byte pipe; not protocol-specific */
void mp_term_puts(mp_term_t *term, const unsigned char *bytes, int n) {
	if(n > 0)
		_mp_term_emit(term, bytes, n);
}

void mp_term_putc(mp_term_t *term, unsigned char c) {
	_mp_term_emit(term, &c, 1);
}

/* event ring */

void _mp_term_push(mp_term_t *term, const mp_term_event_t *event) {
	unsigned char next = (unsigned char)((term->head + 1) % MP_TERM_EVENTS);

	if(next == term->tail) {
		/* full: drop. The reader is behind. */
		return;
	}
	term->events[term->head] = *event;
	term->head = next;
}

void _mp_term_flush_text(mp_term_t *term) {
	mp_term_event_t event;

	if(term->textLen == 0)
		return;
	memset(&event, 0, sizeof(event));
	event.type = MP_TERM_EVENT_TEXT;
	event.bytes = term->text;
	event.len = term->textLen;
	_mp_term_push(term, &event);
	term->textLen = 0;
}

void _mp_term_text(mp_term_t *term, unsigned char c) {
	if(term->textLen >= (int)sizeof(term->text)) {
		_mp_term_flush_text(term);
		if(term->textLen >= (int)sizeof(term->text))
			return; /* still no room: drop */
	}
	term->text[term->textLen++] = c;
}

void _mp_term_key(mp_term_t *term, int key) {
	mp_term_event_t event;

	_mp_term_flush_text(term);
	memset(&event, 0, sizeof(event));
	event.type = MP_TERM_EVENT_KEY;
	event.key = key;
	_mp_term_push(term, &event);
}

void _mp_term_escape(mp_term_t *term) {
	term->escape = 0;
	/* back to ground, or the next key (itself an Esc sequence) is read as
	 * a second Escape and the shell closes the menu it just opened */
	term->state = _MP_TERM_GROUND;
	_mp_term_key(term, MP_TERM_KEY_ESCAPE);
}

void mp_term_rx(mp_term_t *term, unsigned char c) {
	_mp_term_parse(term, c);
}

int mp_term_read(mp_term_t *term, mp_term_event_t *event) {
	/* drain buffered text first so a run is readable without a following byte */
	if(term->head == term->tail && term->textLen > 0)
		_mp_term_flush_text(term);

	/* a pending lone Escape fires once its timeout has elapsed */
	if(term->escape == 1 && term->head == term->tail &&
			mp_clock_ticks() - term->escapeTick >= _mp_term_ms(MP_TERM_ESCAPE_MS)) {
		_mp_term_escape(term);
	}

	if(term->head == term->tail)
		return(0);

	*event = term->events[term->tail];
	term->tail = (unsigned char)((term->tail + 1) % MP_TERM_EVENTS);
	return(1);
}

/* 0..7 only: minicom's ANSI paints 30-37 / 40-47 and drops 38;2 and 90-97. */
static const int _mp_term_splash_fg[] = { 6, 6, 4, 5, 1, 3, 2, 7 };

/* room 0 writes the whole string; a positive room clips so a narrow glass does not wrap */
static void _mp_term_splash_text(mp_term_t *term, const char *s, int room) {
	int n;

	if(!s || !s[0])
		return;
	n = (int)strlen(s);
	if(room > 0 && n > room)
		n = room;
	mp_term_puts(term, (const unsigned char *)s, n);
}

void mp_term_splash(mp_term_t *term, const mp_term_banner_line_t *lines, int n) {
	int i, row, art, limit;

	if(!term || !term->write)
		return;
	if(!lines || n < 0)
		n = 0;

	mp_term_clear(term);
	mp_term_title(term, "miniPhi");

	/* a reverse bar. Bold + reverse shows even when the peer drops color. */
	mp_term_move(term, 1, 1);
	mp_term_style(term, MP_TERM_STYLE_BOLD | MP_TERM_STYLE_REVERSE);
	mp_term_fg(term, 6);
	mp_term_bg(term, 4);
	_mp_term_splash_text(term, " miniPhi ", term->cols);
	mp_term_style(term, MP_TERM_STYLE_RESET);
	mp_term_fg(term, 7);
	{
		char glass[32];

		if(term->rows > 0 && term->cols > 0)
			snprintf(glass, sizeof(glass), "  console %dx%d", term->rows, term->cols);
		else
			snprintf(glass, sizeof(glass), "  console");
		_mp_term_splash_text(term, glass, term->cols > 9 ? term->cols - 9 : 0);
	}

	/* stop one row short of the glass so the block cannot scroll itself off */
	row = 3;
	art = 0;
	limit = term->rows > 2 ? term->rows - 1 : 24;
	for(i = 0; i < n && row < limit; i++) {
		const char *label = lines[i].label ? lines[i].label : "";
		const char *value = lines[i].value ? lines[i].value : "";
		int used;

		mp_term_move(term, row, 1);
		if(label[0] == '\0') {
			mp_term_style(term, MP_TERM_STYLE_BOLD);
			mp_term_fg(term, _mp_term_splash_fg[art % 8]);
			art++;
			_mp_term_splash_text(term, value, term->cols);
		} else {
			used = (int)strlen(label);
			if(term->cols > 0 && used > term->cols)
				used = term->cols;
			mp_term_style(term, MP_TERM_STYLE_BOLD);
			mp_term_fg(term, 6);
			_mp_term_splash_text(term, label, term->cols);
			mp_term_style(term, MP_TERM_STYLE_RESET);
			mp_term_fg(term, 7);
			_mp_term_splash_text(term, value, term->cols > 0 ? term->cols - used : 0);
		}
		mp_term_style(term, MP_TERM_STYLE_RESET);
		row++;
	}

	/* pin the splash. 999 clamps to the glass until a probe has measured it. */
	row += 2;
	if(term->rows > 1 && row >= term->rows)
		row = term->rows - 1;
	if(row < 1)
		row = 1;
	term->home = row;
	mp_term_region(term, row, term->rows > 0 ? term->rows : 999);
	mp_term_move(term, row, 1);
	mp_term_cursor(term, 1);
}

void mp_term_cursor(mp_term_t *term, int on) {
	if(!term || !term->write)
		return;
	_mp_term_emit(term, (const unsigned char *)(on ? "\033[?25h" : "\033[?25l"), 6);
}

void mp_term_query(mp_term_t *term) {
	static const char probe[] = "\033[s\033[?25l\033[999;999H\033[6n";

	if(!term || !term->write)
		return;
	/* save, hide, park in the corner, ask where it landed */
	term->probe = YES;
	_mp_term_emit(term, (const unsigned char *)probe, (int)sizeof(probe) - 1);
}

void mp_term_query_utf8(mp_term_t *term) {
	/* home, one box line (3 bytes), ask where the cursor landed */
	static const char probe[] = "\033[1;1H\xe2\x94\x80\033[6n";

	if(!term || !term->write)
		return;
	term->probeUtf8 = YES;
	_mp_term_emit(term, (const unsigned char *)probe, (int)sizeof(probe) - 1);
}

void mp_term_settle(mp_term_t *term) {
	static const char back[] = "\033[u\033[?25h";

	if(!term || !term->write)
		return;
	term->probe = NO;
	_mp_term_emit(term, (const unsigned char *)back, (int)sizeof(back) - 1);
}

void mp_term_graphic(mp_term_t *term, int on) {
	if(!term || !term->write)
		return;
	if((term->caps & MP_TERM_CAP_VT100) == 0)
		return;
	_mp_term_emit(term, (const unsigned char *)(on ? "\033(0" : "\033(B"), 3);
}

unsigned long mp_term_waiting(const mp_term_t *term) {
	unsigned long wait = _mp_term_ms(MP_TERM_ESCAPE_MS);
	unsigned long gone;

	/* an event is ready, or text is buffered and a state change will flush it */
	if(term->head != term->tail || term->textLen > 0)
		return(1);

	if(term->escape != 1)
		return(0);

	gone = mp_clock_ticks() - term->escapeTick;
	return(gone >= wait ? 1 : wait - gone);
}

#endif
