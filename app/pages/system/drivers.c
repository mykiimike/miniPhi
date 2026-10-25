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
 * Sub-page of System: one row per driver that registered, then a board
 * driver whose open() ran and that has no registration of its own.
 * A driver that has not registered stays off this page.
 */

#include "../index.h"

#ifdef SYSINFO_SCREEN

static const mp_tui_column_t _sysinfo_device_columns[] = {
	{ "Driver", 108, MP_TUI_ALIGN_LEFT },
	{ "Description", 0, MP_TUI_ALIGN_LEFT },
	{ "Version", 56, MP_TUI_ALIGN_LEFT },
};

static void _sysinfo_driver_row(sysinfo_t *sysinfo, int row, const char *name, const char *description, const char *version) {
	mp_tui_table_set(&sysinfo->screen.deviceTable, row, 0, "%s", name ? name : "");
	mp_tui_table_set(&sysinfo->screen.deviceTable, row, 1, "%s", description ? description : "");
	mp_tui_table_set(&sysinfo->screen.deviceTable, row, 2, "%s", version ? version : "");
}

static mp_bool_t _sysinfo_driver_self(sysinfo_t *sysinfo, void *self) {
	int count;
	int n;

	if(!self)
		return(NO);
	count = mp_device_driver_count(&sysinfo->kernel);
	for(n = 0; n < count; n++) {
		mp_device_driver_t *driver = mp_device_driver_at(&sysinfo->kernel, n);

		if(driver && driver->self == self)
			return(YES);
	}
	return(NO);
}

static void _sysinfo_devices_values(sysinfo_t *sysinfo) {
	const mp_device_ops_t *seen[MP_DEVICE_MAX];
	int seenCount = 0;
	int drivers = mp_device_driver_count(&sysinfo->kernel);
	int row = 0;
	int i;

	mp_tui_table_rows(&sysinfo->screen.deviceTable, MP_DEVICE_MAX);

	for(i = 0; i < drivers && row < MP_DEVICE_MAX; i++) {
		mp_device_driver_t *driver = mp_device_driver_at(&sysinfo->kernel, i);

		if(!driver)
			continue;
		_sysinfo_driver_row(sysinfo, row, driver->name, driver->description, driver->version);
		row++;
	}
	for(i = 0; i < mp_device_count() && row < MP_DEVICE_MAX; i++) {
		mp_device_t *device = mp_device_at(i);
		int s;

		if(!device || device->opened == NO || !device->driver)
			continue;
		if(_sysinfo_driver_self(sysinfo, device->self) == YES)
			continue;
		for(s = 0; s < seenCount; s++)
			if(seen[s] == device->driver)
				break;
		if(s < seenCount)
			continue;
		seen[seenCount++] = device->driver;
		_sysinfo_driver_row(sysinfo, row, device->driver->name, device->driver->description, device->driver->version);
		row++;
	}
	mp_tui_table_rows(&sysinfo->screen.deviceTable, row);
	mp_tui_label_set(&sysinfo->screen.deviceCount, "%d loaded", row);
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
	if(mp_tui_child(tui, "Drivers", _sysinfo_devices_page, sysinfo))
		sysinfo->screen.devicesIndex = tui->appCount - 1;
}

void _sysinfo_devices_refresh(sysinfo_t *sysinfo) {
	if(sysinfo->screen.devicesReady && sysinfo->screen.tui.opened == sysinfo->screen.devicesIndex) {
		_sysinfo_devices_values(sysinfo);
	}
}

#endif
