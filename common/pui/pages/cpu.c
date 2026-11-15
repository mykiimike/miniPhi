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

#include "pages.h"

/* RAM: static data ends at _end, then the reserved heap; the stack sits at the top */
#if defined(__GNUC__)
extern uint32_t _end, _estack;
extern char __heap_size, __stack_size;
#endif

/* accent colors; on mono panels these all collapse to lit pixels */
#define _MP_PUI_CPU_CYAN   ((mp_pui_color_t)0x00FFFF00u)
#define _MP_PUI_CPU_AMBER  ((mp_pui_color_t)0x00FFB000u)
#define _MP_PUI_CPU_GREEN  ((mp_pui_color_t)0x0000FF00u)
#define _MP_PUI_CPU_RED    ((mp_pui_color_t)0x00FF3030u)
#define _MP_PUI_CPU_DIM    ((mp_pui_color_t)0x00303030u)

static mp_pui_page_t _mp_pui_cpu_page;
static mp_pui_label_t _mp_pui_cpu_caption;
static mp_pui_label_t _mp_pui_cpu_name;
static mp_pui_big_t _mp_pui_cpu_speed;
static mp_pui_bar_t _mp_pui_cpu_rambar;
static mp_pui_label_t _mp_pui_cpu_ram;
static int _mp_pui_cpu_on;

#if MP_CPUS > 1
/* one line and one bar a core when they fit between the clock and the
 * RAM bar, otherwise one line with every load */
static mp_pui_label_t _mp_pui_cpu_core[MP_CPUS];
static mp_pui_bar_t _mp_pui_cpu_corebar[MP_CPUS];
static int _mp_pui_cpu_rows;
static int _mp_pui_cpu_cores;
#endif

static mp_pui_color_t _mp_pui_cpu_ram_color(int usedPct) {
	if(usedPct < 60)
		return(_MP_PUI_CPU_GREEN);
	if(usedPct < 85)
		return(_MP_PUI_CPU_AMBER);
	return(_MP_PUI_CPU_RED);
}

static void _mp_pui_cpu_refresh(void) {
	unsigned long speed = mp_clock_get_speed();
	unsigned long total = (unsigned long)PHI_BUILD_RAM;
	unsigned long used = 0, usedKb, totalKb, pct;

	char line[MP_PUI_TEXT];

	snprintf(line, sizeof(line), "%lu MHz", speed / 1000000UL);
	mp_pui_big_set(&_mp_pui_cpu_speed, line);

#if defined(__GNUC__)
	{
		unsigned long origin = (unsigned long)&_estack - total;

		if((unsigned long)&_end >= origin) {
			used = (unsigned long)&_end - origin;
			used += (unsigned long)&__heap_size + (unsigned long)&__stack_size;
		}
	}
#endif
	if(used > total)
		used = total;
	totalKb = total / 1024UL;
	usedKb = (used + 1023UL) / 1024UL;
	if(usedKb > totalKb)
		usedKb = totalKb;
	pct = totalKb ? (usedKb * 100UL) / totalKb : 0;
	snprintf(line, sizeof(line), "RAM %lu/%luK", usedKb, totalKb);
	mp_pui_label_set(&_mp_pui_cpu_ram, line);
	mp_pui_bar_set(&_mp_pui_cpu_rambar, (int)((usedKb * 1000UL) / (totalKb ? totalKb : 1)));
	mp_pui_bar_color(&_mp_pui_cpu_rambar, _mp_pui_cpu_ram_color((int)pct), _MP_PUI_CPU_DIM);

#if MP_CPUS > 1
	if(_mp_pui_cpu_rows) {
		int c;

		for(c = 0; c < _mp_pui_cpu_cores; c++) {
			int load = mp_cpu_load(c);

			if(load < 0)
				snprintf(line, sizeof(line), "Core %d  --", c);
			else
				snprintf(line, sizeof(line), "Core %d  %d%%", c, load);
			mp_pui_label_set(&_mp_pui_cpu_core[c], line);
			mp_pui_bar_set(&_mp_pui_cpu_corebar[c], load < 0 ? 0 : load * 10);
			mp_pui_bar_color(&_mp_pui_cpu_corebar[c], _mp_pui_cpu_ram_color(load < 0 ? 0 : load), _MP_PUI_CPU_DIM);
		}
	}
	else if(_mp_pui_cpu_cores > 1) {
		int c;
		int at = snprintf(line, sizeof(line), "Load");

		for(c = 0; c < _mp_pui_cpu_cores && at > 0 && at < (int)sizeof(line); c++) {
			int load = mp_cpu_load(c);

			at += load < 0
				? snprintf(line + at, sizeof(line) - at, " --")
				: snprintf(line + at, sizeof(line) - at, " %d%%", load);
		}
		mp_pui_label_set(&_mp_pui_cpu_core[0], line);
	}
#endif
}

static void _mp_pui_cpu_build(mp_pui_page_t *page, void *user) {
	mp_pui_t *ui = user;
	mp_kernel_t *kernel = ui ? ui->kernel : NULL;
	char line[MP_PUI_TEXT];
	int glassW = ui && ui->display ? ui->display->width : 128;
	int glassH = ui && ui->display ? ui->display->height : 64;
	int body = _mp_pui_body();
	int title = _mp_pui_title_h();
	int barW = MP_PUI_DP(_mp_pui_units(glassW) > 128 ? 120 : 96);
	int barY = glassH - body - MP_PUI_DP(8);
	int ramY = barY + MP_PUI_DP(8);
	int speedY = _mp_pui_units(glassH) > 64 ? (glassH - title) / 2 - title / 2 : MP_PUI_DP(26);

	mp_pui_label(&_mp_pui_cpu_caption, page, 0, 0, glassW, body, "CPU / RAM");
	mp_pui_label_align(&_mp_pui_cpu_caption, MP_PUI_ALIGN_CENTER);
	mp_pui_label_color(&_mp_pui_cpu_caption, _MP_PUI_CPU_CYAN);

#if MP_CPUS > 1
	_mp_pui_cpu_cores = mp_task_cpus();
	if(_mp_pui_cpu_cores > 1)
		snprintf(line, sizeof(line), "%s x%d", kernel && kernel->mcuName ? kernel->mcuName : "MCU",
			_mp_pui_cpu_cores);
	else
#endif
	snprintf(line, sizeof(line), "%s", kernel && kernel->mcuName ? kernel->mcuName : "MCU");
	mp_pui_label(&_mp_pui_cpu_name, page, 0, body, glassW, body, line);
	mp_pui_label_align(&_mp_pui_cpu_name, MP_PUI_ALIGN_CENTER);
	mp_pui_label_color(&_mp_pui_cpu_name, _MP_PUI_CPU_AMBER);

	mp_pui_big(&_mp_pui_cpu_speed, page, 0, speedY, glassW, title, "--");
	mp_pui_big_color(&_mp_pui_cpu_speed, _MP_PUI_CPU_GREEN);

	mp_pui_bar(&_mp_pui_cpu_rambar, page, (glassW - barW) / 2, barY, barW, MP_PUI_DP(6), 1000);
	mp_pui_label(&_mp_pui_cpu_ram, page, 0, ramY, glassW, body, "--");
	mp_pui_label_align(&_mp_pui_cpu_ram, MP_PUI_ALIGN_CENTER);

#if MP_CPUS > 1
	_mp_pui_cpu_rows = 0;
	if(_mp_pui_cpu_cores > 1) {
		int top = speedY + title + MP_PUI_DP(6);
		int bottom = barY - MP_PUI_DP(6);
		int step = body + MP_PUI_DP(10);
		int c, y;

		if(_mp_pui_cpu_cores * step <= bottom - top) {
			_mp_pui_cpu_rows = 1;
			y = top + (bottom - top - _mp_pui_cpu_cores * step) / 2;
			for(c = 0; c < _mp_pui_cpu_cores; c++, y += step) {
				mp_pui_label(&_mp_pui_cpu_core[c], page, 0, y, glassW, body, "--");
				mp_pui_label_align(&_mp_pui_cpu_core[c], MP_PUI_ALIGN_CENTER);
				mp_pui_bar(&_mp_pui_cpu_corebar[c], page, (glassW - barW) / 2, y + body + MP_PUI_DP(2),
					barW, MP_PUI_DP(4), 1000);
			}
		}
		else if(bottom - top >= body) {
			mp_pui_label(&_mp_pui_cpu_core[0], page, 0, top + (bottom - top - body) / 2, glassW, body, "--");
			mp_pui_label_align(&_mp_pui_cpu_core[0], MP_PUI_ALIGN_CENTER);
		}
		else
			_mp_pui_cpu_cores = 0;
	}
#endif
	_mp_pui_cpu_refresh();
}

mp_ret_t mp_pui_page_cpu(mp_pui_t *ui) {
	mp_ret_t ok = mp_pui_page_register(ui, &_mp_pui_cpu_page, "miniPhi/CPU", _mp_pui_cpu_build, ui);

	if(ok == TRUE)
		_mp_pui_cpu_on = 1;
	return(ok);
}

void mp_pui_pages_refresh(mp_pui_t *ui) {
	int _mp_pui_key = mp_pui_lock();
	mp_pui_page_temp_refresh(ui);
	mp_pui_page_clock_refresh(ui);
	mp_pui_page_charge_refresh(ui);
	mp_pui_page_volt_refresh(ui);
	mp_pui_page_measure_refresh(ui);
	mp_pui_page_faces_refresh(ui);
	mp_pui_page_glass_refresh(ui);
	if(_mp_pui_cpu_on && ui && ui->current == &_mp_pui_cpu_page)
		_mp_pui_cpu_refresh();
	mp_pui_unlock(_mp_pui_key);
}

#endif
