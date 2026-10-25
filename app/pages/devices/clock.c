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
 * Clock view of Devices. Subscribing is what makes the driver read the chip.
 * Leaving the view releases that subscription.
 */

#include "../index.h"

#ifdef SYSINFO_SCREEN
#ifdef SUPPORT_COMMON_DEVICE

static void _sysinfo_clock_lines(mp_tui_widget_t *page, sysinfo_t *sysinfo) {
	const mp_tui_font_t *font = page->tui->theme.font;
	int line = font->lineHeight + 1;
	int width = page->rect.w - 48;
	int x = 24;
	int y = 48;

	mp_tui_label(&sysinfo->screen.accelName, page, x, y, width, line, "--");
	mp_tui_label_align(&sysinfo->screen.accelName, MP_TUI_ALIGN_CENTER);
	mp_tui_label_color(&sysinfo->screen.accelName, page->tui->theme.accent);
	y += line + 8;
	mp_tui_label(&sysinfo->screen.accelX, page, x, y, width, line, "");
	mp_tui_label_align(&sysinfo->screen.accelX, MP_TUI_ALIGN_CENTER);
	y += line + 4;
	mp_tui_label(&sysinfo->screen.accelY, page, x, y, width, line, "");
	mp_tui_label_align(&sysinfo->screen.accelY, MP_TUI_ALIGN_CENTER);
}

void _sysinfo_dev_clock_release(sysinfo_t *sysinfo) {
	if(!sysinfo)
		return;
	if(sysinfo->screen.clockSub.device)
		mp_device_subscribe_end(&sysinfo->screen.clockSub);
}

mp_bool_t _sysinfo_dev_clock_shown(sysinfo_t *sysinfo) {
	if(!sysinfo || sysinfo->screen.devView < 0)
		return(NO);
	if(sysinfo->screen.tui.opened != sysinfo->screen.devView)
		return(NO);
	return(sysinfo->screen.devClass == MP_DEVICE_CLOCK ? YES : NO);
}

void _sysinfo_dev_clock_build(mp_tui_widget_t *page, sysinfo_t *sysinfo) {
	mp_device_registry_t *reg = NULL;

	_sysinfo_dev_current(sysinfo, &reg, NULL);
	if(sysinfo->screen.clockSub.device && sysinfo->screen.clockSub.device != reg)
		mp_device_subscribe_end(&sysinfo->screen.clockSub);
	if(reg)
		mp_device_clock(reg, &sysinfo->screen.clockSub);
	_sysinfo_clock_lines(page, sysinfo);
	_sysinfo_dev_clock_refresh(sysinfo);
}

void _sysinfo_dev_clock_refresh(sysinfo_t *sysinfo) {
	mp_device_time_t now;
	char text[32];

	if(!sysinfo->screen.clockSub.device || mp_device_clock_read(sysinfo->screen.clockSub.device->instance, &now) != TRUE) {
		mp_tui_label_set(&sysinfo->screen.accelName, "%s", "--");
		mp_tui_label_set(&sysinfo->screen.accelX, "%s", "");
		mp_tui_label_set(&sysinfo->screen.accelY, "%s", "");
		return;
	}
	snprintf(text, sizeof(text), "%04d-%02d-%02d", now.year, now.month, now.day);
	mp_tui_label_set(&sysinfo->screen.accelName, "%s", text);
	snprintf(text, sizeof(text), "%02d:%02d:%02d", now.hour, now.minute, now.second);
	mp_tui_label_set(&sysinfo->screen.accelX, "%s", text);
	mp_tui_label_set(&sysinfo->screen.accelY, "%s", now.running == YES ? "running" : "stopped");
}

#endif
#endif
