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
 * Sub-page of System: the PHY ports, the role each one is locked to,
 * and the role it runs now. A free port is not device and not host yet.
 */

#include "../index.h"

#if defined(SYSINFO_SCREEN) && defined(SUPPORT_COMMON_USB)

static const mp_tui_column_t _sysinfo_usb_columns[] = {
	{ "Port", 0, MP_TUI_ALIGN_LEFT },
	{ "Lock", 52, MP_TUI_ALIGN_LEFT },
	{ "Role", 52, MP_TUI_ALIGN_LEFT },
	{ "Link", 88, MP_TUI_ALIGN_LEFT },
};

static void _sysinfo_usb_values(sysinfo_t *sysinfo) {
	int count = mp_usb_count();
	int n;

	if(count > MP_USB_PORT_MAX)
		count = MP_USB_PORT_MAX;
	mp_tui_table_rows(&sysinfo->screen.usbTable, count);
	mp_tui_label_set(&sysinfo->screen.usbCount, "%d", count);

	for(n = 0; n < count; n++) {
		mp_usb_port_t *port = mp_usb_at(n);
		const char *link = port->fault == YES ? "fault" : mp_usb_link_name(port->link);

		mp_tui_table_set(&sysinfo->screen.usbTable, n, 0, "%s", port->name);
		mp_tui_table_set(&sysinfo->screen.usbTable, n, 1, "%s", mp_usb_lock_name(port->fixed));
		mp_tui_table_set(&sysinfo->screen.usbTable, n, 2, "%s", mp_usb_role_name(port->role));
		if(port->speed != MP_USB_SPEED_NONE && port->fault == NO)
			mp_tui_table_set(&sysinfo->screen.usbTable, n, 3, "%s, %s", link, mp_usb_speed_name(port->speed));
		else
			mp_tui_table_set(&sysinfo->screen.usbTable, n, 3, "%s", link);
	}
}

static void _sysinfo_usb_page(mp_tui_widget_t *page, void *user) {
	sysinfo_t *sysinfo = user;
	const mp_tui_font_t *font = page->tui->theme.font;
	int line = font->lineHeight + 1;
	int width = page->rect.w - 16;

	mp_tui_label(&sysinfo->screen.usbCount, page, 8, 4, width, line, "");
	mp_tui_label_color(&sysinfo->screen.usbCount, page->tui->theme.accent);
	mp_tui_table(&sysinfo->screen.usbTable, page, 8, 4 + line + 4, width,
		_sysinfo_usb_columns, 4, &sysinfo->screen.usbCell[0][0], MP_USB_PORT_MAX);
	sysinfo->screen.usbReady = 1;
	_sysinfo_usb_values(sysinfo);
}

void _sysinfo_usb_open(sysinfo_t *sysinfo) {
	mp_tui_t *tui = &sysinfo->screen.tui;

	sysinfo->screen.usbIndex = -1;
	if(mp_tui_child(tui, "USB", _sysinfo_usb_page, sysinfo))
		sysinfo->screen.usbIndex = tui->appCount - 1;
}

void _sysinfo_usb_refresh(sysinfo_t *sysinfo) {
	if(sysinfo->screen.usbReady && sysinfo->screen.tui.opened == sysinfo->screen.usbIndex)
		_sysinfo_usb_values(sysinfo);
}

#endif
