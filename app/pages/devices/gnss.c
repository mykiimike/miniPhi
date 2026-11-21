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
 * GNSS view of Devices. Subscribing is what makes the driver read the
 * receiver. Leaving the view releases that subscription.
 */

#include "../index.h"

#ifdef SYSINFO_SCREEN
#ifdef SUPPORT_COMMON_DEVICE

static const mp_tui_column_t _sysinfo_gnss_columns[] = {
	{ "Sat", 52, MP_TUI_ALIGN_RIGHT },
	{ "El", 52, MP_TUI_ALIGN_RIGHT },
	{ "Az", 60, MP_TUI_ALIGN_RIGHT },
	{ "dB", 48, MP_TUI_ALIGN_RIGHT },
	{ "Fix", 0, MP_TUI_ALIGN_CENTER },
};

/* five decimals: the receiver is not accurate to the centimetre */
static void _sysinfo_gnss_deg(char *dst, int cap, long e7) {
	unsigned long v;
	int neg = 0;
	char side;

	if(e7 < 0) {
		neg = 1;
		e7 = -e7;
	}
	v = (unsigned long)e7 / 100ul;
	side = neg ? 'S' : 'N';
	snprintf(dst, (size_t)cap, "%lu.%05lu\260 %c", v / 100000ul, v % 100000ul, side);
}

static void _sysinfo_gnss_east(char *dst, int cap, long e7) {
	unsigned long v;
	int neg = 0;

	if(e7 < 0) {
		neg = 1;
		e7 = -e7;
	}
	v = (unsigned long)e7 / 100ul;
	snprintf(dst, (size_t)cap, "%lu.%05lu\260 %c", v / 100000ul, v % 100000ul, neg ? 'W' : 'E');
}

static int _sysinfo_gnss_rank(const mp_device_gnss_sat_t *a, const mp_device_gnss_sat_t *b) {
	int asnr, bsnr;

	if((a->used == YES) != (b->used == YES))
		return(a->used == YES ? -1 : 1);
	asnr = a->snr == 0xff ? -1 : a->snr;
	bsnr = b->snr == 0xff ? -1 : b->snr;
	if(asnr != bsnr)
		return(bsnr - asnr);
	return((int)a->id - (int)b->id);
}

static void _sysinfo_gnss_lines(mp_tui_widget_t *page, sysinfo_t *sysinfo) {
	const mp_tui_font_t *font = page->tui->theme.font;
	mp_tui_theme_t *theme = &page->tui->theme;
	int line = font->lineHeight;
	int gap = 4;
	int x = 16;
	int width = page->rect.w - 32;
	int col = (width - 12) / 2;
	int y = 10;
	int card;

	card = 10 + line + gap + line + gap + line + gap + line + 10;
	mp_tui_box(&sysinfo->screen.gnssCard, page, 8, 8, page->rect.w - 16, card, theme->surface);
	sysinfo->screen.gnssCard.border = 1;
	y = 10;
	mp_tui_label(&sysinfo->screen.accelName, &sysinfo->screen.gnssCard.widget, x - 8, y, width, line, "--");
	mp_tui_label_align(&sysinfo->screen.accelName, MP_TUI_ALIGN_CENTER);
	y += line + gap;
	mp_tui_label(&sysinfo->screen.accelX, &sysinfo->screen.gnssCard.widget, x - 8, y, width, line, "--");
	mp_tui_label_align(&sysinfo->screen.accelX, MP_TUI_ALIGN_CENTER);
	mp_tui_label_color(&sysinfo->screen.accelX, theme->muted);
	y += line + gap;
	mp_tui_label(&sysinfo->screen.accelY, &sysinfo->screen.gnssCard.widget, x - 8, y, col, line, "--");
	mp_tui_label_align(&sysinfo->screen.accelY, MP_TUI_ALIGN_CENTER);
	mp_tui_label(&sysinfo->screen.accelZ, &sysinfo->screen.gnssCard.widget, x - 8 + col + 12, y, col, line, "--");
	mp_tui_label_align(&sysinfo->screen.accelZ, MP_TUI_ALIGN_CENTER);
	y += line + gap;
	mp_tui_label(&sysinfo->screen.accelTemperature, &sysinfo->screen.gnssCard.widget, x - 8, y, width, line, "--");
	mp_tui_label_align(&sysinfo->screen.accelTemperature, MP_TUI_ALIGN_CENTER);
	mp_tui_label_color(&sysinfo->screen.accelTemperature, theme->muted);

	mp_tui_table(&sysinfo->screen.gnssTable, page, 8, 8 + card + 10, page->rect.w - 16,
		_sysinfo_gnss_columns, 5, &sysinfo->screen.gnssCell[0][0], MP_DEVICE_GNSS_SAT_MAX);
}

static void _sysinfo_gnss_blank(sysinfo_t *sysinfo) {
	mp_tui_label_set(&sysinfo->screen.accelName, "%s", "--");
	mp_tui_label_color(&sysinfo->screen.accelName, sysinfo->screen.tui.theme.muted);
	mp_tui_label_set(&sysinfo->screen.accelX, "%s", "--");
	mp_tui_label_set(&sysinfo->screen.accelY, "%s", "--");
	mp_tui_label_set(&sysinfo->screen.accelZ, "%s", "--");
	mp_tui_label_set(&sysinfo->screen.accelTemperature, "%s", "--");
	mp_tui_table_rows(&sysinfo->screen.gnssTable, 1);
	mp_tui_table_set(&sysinfo->screen.gnssTable, 0, 0, "%s", "--");
	mp_tui_table_set(&sysinfo->screen.gnssTable, 0, 1, "%s", "");
	mp_tui_table_set(&sysinfo->screen.gnssTable, 0, 2, "%s", "");
	mp_tui_table_set(&sysinfo->screen.gnssTable, 0, 3, "%s", "");
	mp_tui_table_set(&sysinfo->screen.gnssTable, 0, 4, "%s", "");
}

void _sysinfo_dev_gnss_release(sysinfo_t *sysinfo) {
	if(!sysinfo)
		return;
	if(sysinfo->screen.gnssSub.device)
		mp_device_subscribe_end(&sysinfo->screen.gnssSub);
}

mp_bool_t _sysinfo_dev_gnss_shown(sysinfo_t *sysinfo) {
	if(!sysinfo || sysinfo->screen.devView < 0)
		return(NO);
	if(sysinfo->screen.tui.opened != sysinfo->screen.devView)
		return(NO);
	return(sysinfo->screen.devClass == MP_DEVICE_GNSS ? YES : NO);
}

void _sysinfo_dev_gnss_build(mp_tui_widget_t *page, sysinfo_t *sysinfo) {
	mp_device_registry_t *reg = NULL;

	_sysinfo_dev_current(sysinfo, &reg, NULL);
	if(sysinfo->screen.gnssSub.device && sysinfo->screen.gnssSub.device != reg)
		mp_device_subscribe_end(&sysinfo->screen.gnssSub);
	if(reg)
		mp_device_gnss(reg, &sysinfo->screen.gnssSub);
	_sysinfo_gnss_lines(page, sysinfo);
	_sysinfo_dev_gnss_refresh(sysinfo);
}

static void _sysinfo_gnss_table(sysinfo_t *sysinfo, const mp_device_gnss_t *fix) {
	unsigned char order[MP_DEVICE_GNSS_SAT_MAX];
	unsigned char n, i, j;
	const mp_device_gnss_sat_t *sat;

	n = fix->viewed;
	if(n > MP_DEVICE_GNSS_SAT_MAX)
		n = MP_DEVICE_GNSS_SAT_MAX;
	if(!n) {
		mp_tui_table_rows(&sysinfo->screen.gnssTable, 1);
		mp_tui_table_set(&sysinfo->screen.gnssTable, 0, 0, "%s", "--");
		mp_tui_table_set(&sysinfo->screen.gnssTable, 0, 1, "%s", "");
		mp_tui_table_set(&sysinfo->screen.gnssTable, 0, 2, "%s", "");
		mp_tui_table_set(&sysinfo->screen.gnssTable, 0, 3, "%s", "");
		mp_tui_table_set(&sysinfo->screen.gnssTable, 0, 4, "%s", "");
		return;
	}
	for(i = 0; i < n; i++)
		order[i] = i;
	for(i = 1; i < n; i++) {
		unsigned char key = order[i];

		for(j = i; j > 0 && _sysinfo_gnss_rank(&fix->sat[key], &fix->sat[order[j - 1]]) < 0; j--)
			order[j] = order[j - 1];
		order[j] = key;
	}
	mp_tui_table_rows(&sysinfo->screen.gnssTable, n);
	for(i = 0; i < n; i++) {
		sat = &fix->sat[order[i]];
		mp_tui_table_set(&sysinfo->screen.gnssTable, i, 0, "%u", sat->id);
		if(sat->elevation == 0xff)
			mp_tui_table_set(&sysinfo->screen.gnssTable, i, 1, "%s", "");
		else
			mp_tui_table_set(&sysinfo->screen.gnssTable, i, 1, "%u\260", sat->elevation);
		if(sat->azimuth == 0xffff)
			mp_tui_table_set(&sysinfo->screen.gnssTable, i, 2, "%s", "");
		else
			mp_tui_table_set(&sysinfo->screen.gnssTable, i, 2, "%u\260", sat->azimuth);
		if(sat->snr == 0xff)
			mp_tui_table_set(&sysinfo->screen.gnssTable, i, 3, "%s", "");
		else
			mp_tui_table_set(&sysinfo->screen.gnssTable, i, 3, "%u", sat->snr);
		mp_tui_table_set(&sysinfo->screen.gnssTable, i, 4, "%s", sat->used == YES ? "yes" : "");
	}
}

void _sysinfo_dev_gnss_refresh(sysinfo_t *sysinfo) {
	mp_device_gnss_t fix;
	mp_device_registry_t *reg = NULL;
	mp_device_t *legacy = NULL;
	mp_tui_theme_t *theme = &sysinfo->screen.tui.theme;
	char lat[24], lon[24], line[64];
	const char *system;
	const char *status;
	unsigned long mm, speed;
	int neg = 0;

	_sysinfo_dev_current(sysinfo, &reg, &legacy);
	if(!reg && !legacy) {
		_sysinfo_gnss_blank(sysinfo);
		return;
	}
	if(!sysinfo->screen.gnssSub.device || mp_device_gnss_read(sysinfo->screen.gnssSub.device->instance, &fix) != TRUE) {
		_sysinfo_gnss_blank(sysinfo);
		return;
	}
	status = "No fix";
	if(fix.quality == 1)
		status = "Fix";
	else if(fix.quality == 2)
		status = "Differential";
	else if(fix.quality == 6)
		status = "Estimated";
	else if(fix.quality)
		status = "Fix";
	mp_tui_label_set(&sysinfo->screen.accelName, "%s", status);
	mp_tui_label_color(&sysinfo->screen.accelName, fix.fixed == YES ? theme->accent : theme->muted);
	system = mp_device_gnss_system_text(fix.system);
	if(fix.hdop) {
		if(system[0])
			snprintf(line, sizeof(line), "%s \267 %u in view \267 %u used \267 %u.%02u",
				system, fix.viewed, fix.usedCount, fix.hdop / 100, fix.hdop % 100);
		else
			snprintf(line, sizeof(line), "%u in view \267 %u used \267 %u.%02u",
				fix.viewed, fix.usedCount, fix.hdop / 100, fix.hdop % 100);
	}
	else if(system[0])
		snprintf(line, sizeof(line), "%s \267 %u in view \267 %u used", system, fix.viewed, fix.usedCount);
	else
		snprintf(line, sizeof(line), "%u in view \267 %u used", fix.viewed, fix.usedCount);
	mp_tui_label_set(&sysinfo->screen.accelX, "%s", line);
	if(fix.fixed == YES) {
		_sysinfo_gnss_deg(lat, (int)sizeof(lat), fix.latitude);
		_sysinfo_gnss_east(lon, (int)sizeof(lon), fix.longitude);
		mp_tui_label_set(&sysinfo->screen.accelY, "%s", lat);
		mp_tui_label_set(&sysinfo->screen.accelZ, "%s", lon);
		if(fix.altitude < 0) {
			neg = 1;
			mm = (unsigned long)(-fix.altitude);
		}
		else
			mm = (unsigned long)fix.altitude;
		speed = fix.speed;
		if(fix.hour > 23 || fix.minute > 59 || fix.second > 59)
			snprintf(line, sizeof(line), "%s%lu.%lu m \267 %lu.%lu m/s \267 %03u\260",
				neg ? "-" : "", mm / 1000ul, (mm % 1000ul) / 100ul,
				speed / 1000ul, (speed % 1000ul) / 100ul, fix.heading / 100u);
		else
			snprintf(line, sizeof(line), "%s%lu.%lu m \267 %lu.%lu m/s \267 %03u\260 \267 %02u:%02u:%02u",
				neg ? "-" : "", mm / 1000ul, (mm % 1000ul) / 100ul,
				speed / 1000ul, (speed % 1000ul) / 100ul, fix.heading / 100u,
				fix.hour, fix.minute, fix.second);
	}
	else {
		mp_tui_label_set(&sysinfo->screen.accelY, "%s", "--");
		mp_tui_label_set(&sysinfo->screen.accelZ, "%s", "--");
		if(fix.hour > 23 || fix.minute > 59 || fix.second > 59)
			snprintf(line, sizeof(line), "%s", "--");
		else if(fix.year)
			snprintf(line, sizeof(line), "%02u:%02u:%02u \267 %04u-%02u-%02u",
				fix.hour, fix.minute, fix.second, fix.year, fix.month, fix.day);
		else
			snprintf(line, sizeof(line), "%02u:%02u:%02u", fix.hour, fix.minute, fix.second);
	}
	mp_tui_label_set(&sysinfo->screen.accelTemperature, "%s", line);
	_sysinfo_gnss_table(sysinfo, &fix);
}

#endif
#endif
