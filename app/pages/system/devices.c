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
 * Sub-page of System: each board device, then the model and serial it registered.
 * The software version is the line under the table. While this page is open each
 * camera is started, so the chip can be asked.
 */

#include "../index.h"

#ifdef SYSINFO_SCREEN

static const mp_tui_column_t _sysinfo_device_columns[] = {
	{ "Device", 76, MP_TUI_ALIGN_LEFT },
	{ "Model", 0, MP_TUI_ALIGN_LEFT },
	{ "Serial", 128, MP_TUI_ALIGN_LEFT },
};

static int _sysinfo_device_line(char *text, int n, const char *line) {
	int wrote;

	if(!line || !line[0] || n >= MP_TUI_TEXT_MAX - 1)
		return(n);
	wrote = snprintf(text + n, (size_t)(MP_TUI_TEXT_MAX - n), "%s%s", n ? "\n" : "", line);
	if(wrote < 0 || n + wrote >= MP_TUI_TEXT_MAX)
		return(MP_TUI_TEXT_MAX - 1);
	return(n + wrote);
}

static void _sysinfo_devices_identity(sysinfo_t *sysinfo) {
	char text[MP_TUI_TEXT_MAX];
	int count = mp_device_count();
	int n = 0;
	int i;

	text[0] = 0;
	if(count > MP_DEVICE_MAX)
		count = MP_DEVICE_MAX;
	for(i = 0; i < count; i++) {
		mp_device_t *device = mp_device_at(i);

		if(!device->version)
			continue;
		n = _sysinfo_device_line(text, n, device->name);
		n = _sysinfo_device_line(text, n, device->version);
	}
	mp_tui_label_set(&sysinfo->screen.deviceInfo, "%s", text);
}

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
		mp_tui_table_set(&sysinfo->screen.deviceTable, n, 1, "%s", device->model ? device->model : "");
		mp_tui_table_set(&sysinfo->screen.deviceTable, n, 2, "%s", device->serial ? device->serial : "");
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
	{
		mp_tui_widget_t *table = &sysinfo->screen.deviceTable.widget;
		int y = table->rect.y + table->rect.h + 8;

		mp_tui_label(&sysinfo->screen.deviceInfo, page, 8, y, width, line * 3, "");
		mp_tui_label_color(&sysinfo->screen.deviceInfo, page->tui->theme.muted);
		_sysinfo_devices_identity(sysinfo);
	}
}

void _sysinfo_devices_open(sysinfo_t *sysinfo) {
	mp_tui_t *tui = &sysinfo->screen.tui;

	sysinfo->screen.devicesIndex = -1;
	if(mp_tui_child(tui, "Devices", _sysinfo_devices_page, sysinfo))
		sysinfo->screen.devicesIndex = tui->appCount - 1;
}

void _sysinfo_devices_refresh(sysinfo_t *sysinfo) {
	if(sysinfo->screen.devicesReady && sysinfo->screen.tui.opened == sysinfo->screen.devicesIndex) {
		_sysinfo_devices_values(sysinfo);
		_sysinfo_devices_identity(sysinfo);
	}
}

#endif
