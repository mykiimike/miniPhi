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
 * Charger view of Devices. Subscribing is what makes the driver read the
 * status lines. Leaving the view releases that subscription.
 */

#include "../index.h"

#ifdef SYSINFO_SCREEN
#ifdef SUPPORT_COMMON_DEVICE

static void _sysinfo_charge_lines(mp_tui_widget_t *page, sysinfo_t *sysinfo) {
	const mp_tui_font_t *font = page->tui->theme.font;
	int line = font->lineHeight + 1;
	int width = page->rect.w - 48;
	int x = 24;
	int y = 48;

	mp_tui_label(&sysinfo->screen.accelName, page, x, y, width, line, "--");
	mp_tui_label_align(&sysinfo->screen.accelName, MP_TUI_ALIGN_CENTER);
	mp_tui_label_color(&sysinfo->screen.accelName, page->tui->theme.accent);
}

void _sysinfo_dev_charge_release(sysinfo_t *sysinfo) {
	if(!sysinfo)
		return;
	if(sysinfo->screen.chargeSub.device)
		mp_device_subscribe_end(&sysinfo->screen.chargeSub);
}

mp_bool_t _sysinfo_dev_charge_shown(sysinfo_t *sysinfo) {
	if(!sysinfo || sysinfo->screen.devView < 0)
		return(NO);
	if(sysinfo->screen.tui.opened != sysinfo->screen.devView)
		return(NO);
	return(sysinfo->screen.devClass == MP_DEVICE_CHARGER ? YES : NO);
}

void _sysinfo_dev_charge_build(mp_tui_widget_t *page, sysinfo_t *sysinfo) {
	mp_device_registry_t *reg = NULL;

	_sysinfo_dev_current(sysinfo, &reg, NULL);
	if(sysinfo->screen.chargeSub.device && sysinfo->screen.chargeSub.device != reg)
		mp_device_subscribe_end(&sysinfo->screen.chargeSub);
	if(reg)
		mp_device_charge(reg, &sysinfo->screen.chargeSub);
	_sysinfo_charge_lines(page, sysinfo);
	_sysinfo_dev_charge_refresh(sysinfo);
}

void _sysinfo_dev_charge_refresh(sysinfo_t *sysinfo) {
	mp_device_charge_state_t state;
	const char *text = "--";

	if(sysinfo->screen.chargeSub.device && mp_device_charge_read(sysinfo->screen.chargeSub.device->instance, &state) == TRUE)
		text = mp_device_charge_text(state);
	mp_tui_label_set(&sysinfo->screen.accelName, "%s", text);
}

#endif
#endif
