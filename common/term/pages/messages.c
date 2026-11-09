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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 0210-1301  USA       *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**
 * @ingroup mpCommonTermPages
 *
 * The Messages page: the printk ring. While it is open, a new line
 * repaints immediately. Up/Down/PgUp/PgDn walk the ring and hold the
 * view; End goes back to the tail. The ring keeps MP_TERM_UI_LOG lines.
 * _ui_log_slot and _ui_stamp are non-static because the splash ticker
 * in ui.c reuses them to show the last line under the card.
 * @{
 */

#include <mp.h>

#ifdef SUPPORT_COMMON_TERM

#include <stdio.h>
#include <string.h>
#include <term/term.h>
#include "pages.h"

int _ui_log_slot(const mp_term_ui_t *ui, int age) {
	int i = ui->logHead - ui->logCount + age;

	while(i < 0)
		i += MP_TERM_UI_LOG;
	return(i % MP_TERM_UI_LOG);
}

void _ui_stamp(unsigned long ticks, char *dst, int n) {
	unsigned long hz = mp_clock_tick_hz();
	unsigned long sec = hz ? ticks / hz : ticks / 1000UL;
	unsigned long ms = hz ? ((ticks % hz) * 1000UL) / hz : ticks % 1000UL;

	snprintf(dst, (size_t)n, "%02lu:%02lu.%03lu", (sec / 60UL) % 100UL, sec % 60UL, ms);
}

static int _ui_has(const char *text, const char *word) {
	return(strstr(text, word) != NULL);
}

/* "TAG: rest". The tag is yellow, a failure is red, the rest is white. */
static int _ui_log_line(mp_term_ui_t *ui, int row, int col, const char *text) {
	int tag = 0;
	int fg = 7;
	int flags = 0;
	int rows;
	int i;

	for(i = 0; text[i] && i < 16; i++) {
		char c = text[i];

		if(c == ':') {
			tag = i + 1;
			break;
		}
		if(!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == ' '))
			break;
	}
	if(_ui_has(text, "failed") || _ui_has(text, "error") || _ui_has(text, "FAIL")) {
		fg = 1;
		flags = MP_TERM_UI_BOLD;
	}
	else if(_ui_has(text, " passed") || _ui_has(text, " ok"))
		fg = 2;
	if(tag > 0) {
		char head[20];

		_ui_copy(head, tag + 1, text);
		mp_term_ui_put(ui, row, col, head, 3, 0, 0);
	}
	rows = mp_term_ui_put(ui, row, col + tag, text + tag, fg, 0, flags);
	return(rows > 0 ? rows : 1);
}

static int _ui_entry_rows(const mp_term_ui_t *ui, const char *text) {
	int n = mp_term_ui_lines(ui, 12, text);

	return(n > 0 ? n : 1);
}

/* how many entries ending at `end` fit in the body, newest kept whole */
static int _ui_fit(const mp_term_ui_t *ui, int end) {
	int body = mp_term_ui_rows(ui);
	int used = 0;
	int n = 0;
	int i = end;

	while(i > 0) {
		int slot = _ui_log_slot(ui, i - 1);
		int rows = _ui_entry_rows(ui, ui->log[slot].text);

		if(used + rows > body && n > 0)
			break;
		used += rows;
		n++;
		i--;
		if(used >= body)
			break;
	}
	return(n);
}

static int _ui_max_skip(const mp_term_ui_t *ui) {
	int fit = _ui_fit(ui, ui->logCount);

	if(ui->logCount > fit)
		return(ui->logCount - fit);
	return(0);
}

static void _ui_messages_paint(mp_term_ui_t *ui, void *user) {
	int body = mp_term_ui_rows(ui);
	int skip, end, start, i, row, max;
	unsigned long seq;

	(void)user;
	MP_INTERRUPT_SAFE_BEGIN
	seq = ui->logSeq;
	max = _ui_max_skip(ui);
	/* lines that arrived while the view was held must not pull it */
	if(ui->logFollow != YES && seq != ui->paintedLog) {
		unsigned long arrived = seq - ui->paintedLog;

		if(arrived >= (unsigned long)MP_TERM_UI_LOG)
			ui->logSkip = max;
		else
			ui->logSkip += (int)arrived;
	}
	if(ui->logSkip < 0)
		ui->logSkip = 0;
	if(ui->logSkip > max)
		ui->logSkip = max;
	skip = ui->logFollow == YES ? 0 : ui->logSkip;
	end = ui->logCount - skip;
	start = end - _ui_fit(ui, end);
	if(start < 0)
		start = 0;
	mp_term_ui_clear(ui);
	for(i = start, row = 0; i < end && row < body; i++) {
		char stamp[16];
		int slot = _ui_log_slot(ui, i);
		int used;

		_ui_stamp(ui->log[slot].ticks, stamp, (int)sizeof(stamp));
		mp_term_ui_put(ui, row, 1, stamp, 4, 0, MP_TERM_UI_BOLD);
		used = _ui_log_line(ui, row, 12, ui->log[slot].text);
		row += used > 0 ? used : 1;
	}
	ui->paintedLog = seq;
	ui->logUnread = 0;
	if(ui->logNode >= 0 && ui->node[ui->logNode].used == YES)
		ui->node[ui->logNode].badge = 0;
	MP_INTERRUPT_SAFE_END
	mp_term_ui_cursor_at(ui, -1, -1);
	if(ui->logFollow == YES)
		mp_term_ui_hint(ui, "Up scroll  End follow  Esc menu");
	else
		mp_term_ui_hint(ui, "Up scroll  End follow  paused");
}

static int _ui_messages_key(mp_term_ui_t *ui, const mp_term_event_t *event, void *user) {
	int max, body;

	(void)user;
	if(event->type != MP_TERM_EVENT_KEY)
		return(FALSE);
	max = _ui_max_skip(ui);
	body = mp_term_ui_rows(ui);
	if(event->key == MP_TERM_KEY_UP) {
		if(ui->logSkip < max) {
			ui->logFollow = NO;
			ui->logSkip++;
		}
		return(TRUE);
	}
	if(event->key == MP_TERM_KEY_DOWN) {
		if(ui->logSkip > 0)
			ui->logSkip--;
		if(ui->logSkip <= 0) {
			ui->logSkip = 0;
			ui->logFollow = YES;
		}
		return(TRUE);
	}
	if(event->key == MP_TERM_KEY_END) {
		ui->logFollow = YES;
		ui->logSkip = 0;
		return(TRUE);
	}
	if(event->key == MP_TERM_KEY_PAGE_UP) {
		if(ui->logSkip < max) {
			ui->logFollow = NO;
			ui->logSkip += body > 0 ? body : 1;
			if(ui->logSkip > max)
				ui->logSkip = max;
		}
		return(TRUE);
	}
	if(event->key == MP_TERM_KEY_PAGE_DOWN) {
		ui->logSkip -= body > 0 ? body : 1;
		if(ui->logSkip <= 0) {
			ui->logSkip = 0;
			ui->logFollow = YES;
		}
		return(TRUE);
	}
	return(FALSE);
}

const mp_term_page_t mp_term_page_messages = {
	_ui_messages_paint, _ui_messages_key, 0, NULL,
};

/**@}*/

#endif
