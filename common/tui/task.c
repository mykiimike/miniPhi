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

#include <mp.h>

#ifdef SUPPORT_COMMON_TUI

/* inputs first, then one band per wake up, so a large redraw never holds the kernel */
static MP_TASK(_mp_tui_task) {
	mp_tui_t *ui = task->user;

	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}
	mp_tui_poll(ui);
	mp_tui_step(ui);
}

int mp_tui_attach(mp_kernel_t *kernel, mp_tui_t *ui, char *who) {
	ui->kernel = kernel;
	if(ui->task)
		return(1);
	ui->task = mp_task_create(&kernel->tasks, who, _mp_tui_task, ui, 1);
	return(ui->task != NULL);
}

void mp_tui_detach(mp_tui_t *ui) {
	if(!ui->task)
		return;
	mp_task_destroy(ui->task);
	ui->task = NULL;
}

#endif
