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
 * Sub-page of System: the board devices, their class and whether they are open.
 */

#include "../index.h"

#ifdef SYSINFO_SCREEN

static const char *_sysinfo_device_class(mp_device_class_t type) {
	switch(type) {
		case MP_DEVICE_DISPLAY: return("display");
		case MP_DEVICE_TOUCH: return("touch");
		case MP_DEVICE_ACCELEROMETER: return("accelerometer");
		case MP_DEVICE_LED: return("led");
		default: return("?");
	}
}

static const mp_tui_column_t _sysinfo_device_columns[] = {
	{ "Device", 0, MP_TUI_ALIGN_LEFT },
	{ "Class", 120, MP_TUI_ALIGN_LEFT },
	{ "State", 64, MP_TUI_ALIGN_LEFT },
};

static void _sysinfo_devices_values(sysinfo_t *sysinfo) {
	int count = mp_device_count();
	int n;

	if(count > MP_DEVICE_MAX)
		count = MP_DEVICE_MAX;
	mp_tui_table_rows(&sysinfo->screen.deviceTable, count);
	mp_tui_label_set(&sysinfo->screen.deviceCount, "%d / %d", count, MP_DEVICE_MAX);

	for(n = 0; n < count; n++) {
		mp_device_t *device = mp_device_at(n);

		mp_tui_table_set(&sysinfo->screen.deviceTable, n, 0, "%s", device->name);
		mp_tui_table_set(&sysinfo->screen.deviceTable, n, 1, "%s", _sysinfo_device_class(device->driver->type));
		mp_tui_table_set(&sysinfo->screen.deviceTable, n, 2, "%s", device->opened == YES ? "open" : "closed");
	}
}

static void _sysinfo_devices_page(mp_tui_widget_t *page, void *user) {
	sysinfo_t *sysinfo = user;
	const mp_tui_font_t *font = page->tui->theme.font;
	int line = font->lineHeight + 1;
	int width = page->rect.w - 16;

	mp_tui_label(&sysinfo->screen.deviceCount, page, 8, 4, width, line, "");
	mp_tui_label_color(&sysinfo->screen.deviceCount, page->tui->theme.accent);
	mp_tui_table(&sysinfo->screen.deviceTable, page, 8, 4 + line + 4, width,
		_sysinfo_device_columns, 3, &sysinfo->screen.deviceCell[0][0], MP_DEVICE_MAX);
	sysinfo->screen.devicesReady = 1;
	_sysinfo_devices_values(sysinfo);
}

void _sysinfo_devices_open(sysinfo_t *sysinfo) {
	mp_tui_t *tui = &sysinfo->screen.tui;

	sysinfo->screen.devicesIndex = -1;
	if(mp_tui_child(tui, "Devices", _sysinfo_devices_page, sysinfo))
		sysinfo->screen.devicesIndex = tui->appCount - 1;
}

void _sysinfo_devices_refresh(sysinfo_t *sysinfo) {
	if(sysinfo->screen.devicesReady && sysinfo->screen.tui.opened == sysinfo->screen.devicesIndex)
		_sysinfo_devices_values(sysinfo);
}

#endif
