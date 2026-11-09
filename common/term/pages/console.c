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
 * The Console page: an echo prompt with a transcript and history. The
 * transcript fills from the top, the prompt sits right under it. Up and
 * Down walk the history. The shell core owns the ring and the input
 * buffer in the mp_term_ui_t struct; this page only reads and writes
 * them.
 * @{
 */

#include <mp.h>

#ifdef SUPPORT_COMMON_TERM

#include <stdio.h>
#include <string.h>
#include <term/term.h>
#include "pages.h"

/* index of the age-th transcript line in the ring */
static int _ui_con_index(const mp_term_ui_t *ui, int age) {
	int i = ui->conHead - ui->conCount + age;

	while(i < 0)
		i += MP_TERM_UI_CON;
	return(i % MP_TERM_UI_CON);
}

static void _ui_con_push(mp_term_ui_t *ui, const char *text) {
	_ui_copy(ui->conLine[ui->conHead], MP_TERM_UI_COLS, text);
	ui->conHead = (ui->conHead + 1) % MP_TERM_UI_CON;
	if(ui->conCount < MP_TERM_UI_CON)
		ui->conCount++;
}

static void _ui_hist_push(mp_term_ui_t *ui, const char *text) {
	if(ui->conHistN > 0 && strcmp(ui->conHist[ui->conHistN - 1], text) == 0)
		return;
	if(ui->conHistN == MP_TERM_UI_HIST) {
		memmove(ui->conHist[0], ui->conHist[1],
			(size_t)(MP_TERM_UI_HIST - 1) * sizeof(ui->conHist[0]));
		ui->conHistN--;
	}
	_ui_copy(ui->conHist[ui->conHistN], MP_TERM_UI_COLS, text);
	ui->conHistN++;
}

static void _ui_con_load(mp_term_ui_t *ui, const char *text) {
	_ui_copy(ui->conInput, MP_TERM_UI_COLS, text);
	ui->conLen = (int)strlen(ui->conInput);
	ui->conCur = ui->conLen;
}

static void _ui_con_insert(mp_term_ui_t *ui, unsigned char c) {
	int room = ui->cols - 8;

	if(room > MP_TERM_UI_COLS - 1)
		room = MP_TERM_UI_COLS - 1;
	if(room < 1 || c < 0x20 || c > 0x7e || ui->conLen >= room)
		return;
	memmove(ui->conInput + ui->conCur + 1, ui->conInput + ui->conCur,
		(size_t)(ui->conLen - ui->conCur + 1));
	ui->conInput[ui->conCur] = (char)c;
	ui->conLen++;
	ui->conCur++;
}

static void _ui_con_submit(mp_term_ui_t *ui) {
	if(ui->conLen <= 0)
		return;
	_ui_con_push(ui, ui->conInput);
	_ui_hist_push(ui, ui->conInput);
	ui->conLen = 0;
	ui->conCur = 0;
	ui->conInput[0] = 0;
	ui->conHistAt = -1;
}

static int _ui_con_rows(const mp_term_ui_t *ui, const char *text) {
	int n = mp_term_ui_lines(ui, 6, text);

	return(n > 0 ? n : 1);
}

static void _ui_console_paint(mp_term_ui_t *ui, void *user) {
	int body = mp_term_ui_rows(ui);
	int room, used, start, i, row;
	char line[MP_TERM_UI_COLS + 8];

	(void)user;
	if(body < 2)
		return;
	/* the transcript from the top, the prompt on the next free row */
	room = body - _ui_con_rows(ui, ui->conInput);
	if(room < 1)
		room = 1;
	used = 0;
	start = ui->conCount;
	while(start > 0) {
		int rows;

		snprintf(line, sizeof(line), "%s", ui->conLine[_ui_con_index(ui, start - 1)]);
		rows = _ui_con_rows(ui, line);
		if(used + rows > room && used > 0)
			break;
		used += rows;
		start--;
		if(used >= room)
			break;
	}
	row = 0;
	for(i = start; i < ui->conCount && row < room; i++) {
		int span;

		snprintf(line, sizeof(line), "%s", ui->conLine[_ui_con_index(ui, i)]);
		mp_term_ui_put(ui, row, 1, "phi", 2, 0, 0);
		mp_term_ui_put(ui, row, 4, ">", 2, 0, 0);
		span = mp_term_ui_put(ui, row, 6, line, 7, 0, 0);
		row += span > 0 ? span : 1;
	}
	mp_term_ui_put(ui, row, 1, "phi", 2, 0, MP_TERM_UI_BOLD);
	mp_term_ui_put(ui, row, 4, ">", 7, 0, MP_TERM_UI_BOLD);
	mp_term_ui_put(ui, row, 6, ui->conInput, 7, 0, MP_TERM_UI_BOLD);
	mp_term_ui_cursor_at(ui, row, 6 + ui->conCur);
	mp_term_ui_hint(ui, "Enter send  Up history  Esc menu");
}

static int _ui_console_key(mp_term_ui_t *ui, const mp_term_event_t *event, void *user) {
	int i;

	(void)user;
	if(event->type == MP_TERM_EVENT_TEXT) {
		for(i = 0; i < event->len; i++)
			_ui_con_insert(ui, event->bytes[i]);
		return(TRUE);
	}
	if(event->type != MP_TERM_EVENT_KEY)
		return(FALSE);
	if(event->key == MP_TERM_KEY_ENTER) {
		_ui_con_submit(ui);
		return(TRUE);
	}
	if(event->key == MP_TERM_KEY_BACKSPACE) {
		if(ui->conCur > 0) {
			memmove(ui->conInput + ui->conCur - 1, ui->conInput + ui->conCur,
				(size_t)(ui->conLen - ui->conCur + 1));
			ui->conCur--;
			ui->conLen--;
		}
		return(TRUE);
	}
	if(event->key == MP_TERM_KEY_DELETE) {
		if(ui->conCur < ui->conLen) {
			memmove(ui->conInput + ui->conCur, ui->conInput + ui->conCur + 1,
				(size_t)(ui->conLen - ui->conCur));
			ui->conLen--;
		}
		return(TRUE);
	}
	if(event->key == MP_TERM_KEY_LEFT) {
		if(ui->conCur > 0)
			ui->conCur--;
		return(TRUE);
	}
	if(event->key == MP_TERM_KEY_RIGHT) {
		if(ui->conCur < ui->conLen)
			ui->conCur++;
		return(TRUE);
	}
	if(event->key == MP_TERM_KEY_HOME) {
		ui->conCur = 0;
		return(TRUE);
	}
	if(event->key == MP_TERM_KEY_END) {
		ui->conCur = ui->conLen;
		return(TRUE);
	}
	if(event->key == MP_TERM_KEY_UP) {
		if(ui->conHistN <= 0)
			return(TRUE);
		if(ui->conHistAt < 0) {
			_ui_copy(ui->conDraft, MP_TERM_UI_COLS, ui->conInput);
			ui->conHistAt = ui->conHistN;
		}
		if(ui->conHistAt > 0) {
			ui->conHistAt--;
			_ui_con_load(ui, ui->conHist[ui->conHistAt]);
		}
		return(TRUE);
	}
	if(event->key == MP_TERM_KEY_DOWN) {
		if(ui->conHistAt < 0)
			return(TRUE);
		ui->conHistAt++;
		if(ui->conHistAt >= ui->conHistN) {
			ui->conHistAt = -1;
			_ui_con_load(ui, ui->conDraft);
		}
		else
			_ui_con_load(ui, ui->conHist[ui->conHistAt]);
		return(TRUE);
	}
	return(FALSE);
}

const mp_term_page_t mp_term_page_console = {
	_ui_console_paint, _ui_console_key, 0, NULL,
};

/**@}*/

#endif
