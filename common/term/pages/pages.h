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
 * @defgroup mpCommonTermPages Shell pages
 * @ingroup mpCommonTerm
 *
 * The built-in pages the shell always carries: a Console and a Messages
 * page. Each lives in its own file under pages/. The shell core (ui.c)
 * registers them by name in mp_term_ui_init, so an app sees them on the
 * bar without adding them. The shared helpers the pages and the core
 * both touch are declared here.
 * @{
 */

#ifndef _HAVE_MP_TERM_PAGES_H
	#define _HAVE_MP_TERM_PAGES_H

	#include <term/ui.h>

	/** the echo prompt with history, registered as "Console" */
	extern const mp_term_page_t mp_term_page_console;
	/** the printk ring with a scrollback, registered as "Messages" */
	extern const mp_term_page_t mp_term_page_messages;

	/** bounded string copy the shell core shares with its pages */
	void _ui_copy(char *dst, int cap, const char *src);
	/** index of the age-th log line in the ring (shared with the splash) */
	int _ui_log_slot(const mp_term_ui_t *ui, int age);
	/** "mm:ss.mmm" stamp of a tick (shared with the splash) */
	void _ui_stamp(unsigned long ticks, char *dst, int n);

#endif

/**@}*/
