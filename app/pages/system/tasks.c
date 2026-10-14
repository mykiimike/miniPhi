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

/*
 * Sub-page of System: the kernel tasks, their state and their period.
 */

#include "../index.h"

#ifdef SYSINFO_SCREEN

static const char *_sysinfo_task_state(mp_task_signal_t signal) {
	switch(signal) {
		case MP_TASK_SIG_SLEEP: return("sleep");
		case MP_TASK_SIG_PENDING: return("pending");
		case MP_TASK_SIG_STOP: return("stop");
		case MP_TASK_SIG_DEAD: return("dead");
		default: return("ready");
	}
}

static const mp_tui_column_t _sysinfo_task_columns[] = {
	{ "Task", 0, MP_TUI_ALIGN_LEFT },
	{ "State", 78, MP_TUI_ALIGN_LEFT },
	{ "Period", 72, MP_TUI_ALIGN_RIGHT },
};

static void _sysinfo_tasks_values(sysinfo_t *sysinfo) {
	mp_kernel_t *kernel = &sysinfo->kernel;
	mp_list_item_t *item;
	mp_task_t *task;
	unsigned long hz = mp_clock_tick_hz();
	int n = 0;
	int rows = (int)kernel->tasks.usedNumber;
	char period[16];

	if(rows > MP_TASK_MAX)
		rows = MP_TASK_MAX;
	mp_tui_table_rows(&sysinfo->screen.taskTable, rows);
	mp_tui_label_set(&sysinfo->screen.taskCount, "%u / %u",
		kernel->tasks.usedNumber, (unsigned int)MP_TASK_MAX);

	for(item = kernel->tasks.usedList.first; item != NULL && n < rows; item = item->next, n++) {
		task = item->user;
		if(!task->delay || !hz)
			snprintf(period, sizeof(period), "tick");
		else
			snprintf(period, sizeof(period), "%lu ms", (task->delay * 1000UL) / hz);
		mp_tui_table_set(&sysinfo->screen.taskTable, n, 0, "%s", task->name ? task->name : "(unnamed)");
		mp_tui_table_set(&sysinfo->screen.taskTable, n, 1, "%s", _sysinfo_task_state(task->signal));
		mp_tui_table_set(&sysinfo->screen.taskTable, n, 2, "%s", period);
	}
}

static void _sysinfo_tasks_page(mp_tui_widget_t *page, void *user) {
	sysinfo_t *sysinfo = user;
	const mp_tui_font_t *font = page->tui->theme.font;
	int line = font->lineHeight + 1;
	int width = page->rect.w - 16;

	mp_tui_label(&sysinfo->screen.taskCount, page, 8, 4, width, line, "");
	mp_tui_label_color(&sysinfo->screen.taskCount, page->tui->theme.accent);
	mp_tui_table(&sysinfo->screen.taskTable, page, 8, 4 + line + 4, width,
		_sysinfo_task_columns, 3, &sysinfo->screen.taskCell[0][0], MP_TASK_MAX);
	sysinfo->screen.tasksReady = 1;
	_sysinfo_tasks_values(sysinfo);
}

void _sysinfo_tasks_open(sysinfo_t *sysinfo) {
	mp_tui_t *tui = &sysinfo->screen.tui;

	sysinfo->screen.tasksIndex = -1;
	if(mp_tui_child(tui, "Task manager", _sysinfo_tasks_page, sysinfo))
		sysinfo->screen.tasksIndex = tui->appCount - 1;
}

void _sysinfo_tasks_refresh(sysinfo_t *sysinfo) {
	if(sysinfo->screen.tasksReady && sysinfo->screen.tui.opened == sysinfo->screen.tasksIndex)
		_sysinfo_tasks_values(sysinfo);
}

#endif
