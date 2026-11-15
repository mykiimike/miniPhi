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

#ifdef SUPPORT_COMMON_PUI

/* one band per wake, so a full glass never holds the kernel.
 * The backlight stays off until the first frame has landed, so the
 * opening paint is not a visible sweep. */
static int _mp_pui_shown;

static void _mp_pui_task_run(mp_task_t *task);

static MP_TASK(_mp_pui_task) {
	int _mp_pui_key = mp_pui_lock();
	_mp_pui_task_run(task);
	mp_pui_unlock(_mp_pui_key);
}

static void _mp_pui_task_run(mp_task_t *task) {
	mp_pui_t *ui = task->user;
	unsigned long wait;

	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}
	mp_pui_pump_keys(ui);
	if(mp_pui_pending(ui)) {
		/* paint in time-bounded chunks so a long glass does not hold the
		 * kernel: the cooperative scheduler runs the term between chunks */
		unsigned long start = mp_clock_us();
		while(mp_pui_step(ui)) {
			if((unsigned long)(mp_clock_us() - start) >= 2000UL)
				break;
		}
		if(mp_pui_pending(ui)) {
			mp_task_delay(task, 1);
			return;
		}
		if(!_mp_pui_shown && ui->display && ui->display->backlight) {
			ui->display->backlight(ui->display, 100);
			_mp_pui_shown = 1;
		}
	}

	/* also after a paint: the scheduler restarts the old delay from this
	 * pass, so a short repaint would otherwise push the tour back */
	wait = mp_pui_idle(ui);
	/* the tour opened a page: paint it on the next tick, not after the old delay */
	if(mp_pui_pending(ui)) {
		mp_task_delay(task, 1);
		return;
	}
	if(wait)
		mp_task_delay(task, wait);
	else
		mp_task_signal(task, MP_TASK_SIG_SLEEP);
}

int mp_pui_attach(mp_kernel_t *kernel, mp_pui_t *ui, char *who) {
	ui->kernel = kernel;
	if(ui->task)
		return(1);
	ui->task = mp_task_create(&kernel->tasks, who, _mp_pui_task, ui, 1);
	mp_printk("PUI ready, %s", who && who[0] ? who : "pui");
	return(ui->task != NULL);
}

void mp_pui_detach(mp_pui_t *ui) {
	if(!ui || !ui->task)
		return;
	mp_task_destroy(ui->task);
	ui->task = NULL;
}

#endif
