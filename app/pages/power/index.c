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
 * Home page. The switch that gives the level to the governor or pins it at
 * HIGH, the switch that authorizes hibernate, the level and its clock, a
 * table of the figures (load, wake ups, switches, deep sleeps, supply
 * sensors), who holds a level, and a table of the time spent at each level.
 */

#include "../index.h"

#ifdef SYSINFO_SCREEN

#include <string.h>

/* the rows of the figures table before the sensors */
#define _SYSINFO_POWER_FIGURES 6

static const mp_tui_column_t _sysinfo_power_columns[] = {
	{ "Figure", 0, MP_TUI_ALIGN_LEFT },
	{ "Value", 130, MP_TUI_ALIGN_RIGHT },
};

static const mp_tui_column_t _sysinfo_level_columns[] = {
	{ "Level", 0, MP_TUI_ALIGN_LEFT },
	{ "MHz", 44, MP_TUI_ALIGN_RIGHT },
	{ "Time", 72, MP_TUI_ALIGN_RIGHT },
	{ "Share", 52, MP_TUI_ALIGN_RIGHT },
};

static unsigned long _sysinfo_mhz10(unsigned long hz) {
	return((hz + 50000UL) / 100000UL);
}

static int _sysinfo_power_sensor(mp_sensor_t *sensor) {
	return(sensor->type == MP_SENSOR_VOLTAGE || sensor->type == MP_SENSOR_CURRENT);
}

static int _sysinfo_power_sensors(mp_kernel_t *kernel) {
	int count = 0;
#ifdef SUPPORT_COMMON_SENSOR
	mp_sensor_t *sensor = NULL;

	while((sensor = mp_sensor_next(kernel, sensor)) != NULL)
		if(_sysinfo_power_sensor(sensor))
			count++;
#endif
	if(count > 8 - _SYSINFO_POWER_FIGURES)
		count = 8 - _SYSINFO_POWER_FIGURES;
	return(count);
}

static void _sysinfo_power_holds(mp_power_t *power, char *text, size_t size) {
	mp_list_item_t *item;
	mp_power_hold_t *hold;
	size_t used;

	snprintf(text, size, "Holds  ");
	used = strlen(text);
	MP_INTERRUPT_SAFE_BEGIN
	for(item = power->holds.first; item != NULL && used + 1 < size; item = item->next) {
		hold = item->user;
		snprintf(text + used, size - used, "%s%s %s", item == power->holds.first ? "" : ", ",
			hold->who ? hold->who : "?", mp_power_name(hold->floor));
		used = strlen(text);
	}
	MP_INTERRUPT_SAFE_END
	if(!power->holds.first)
		snprintf(text + used, size - used, "none");
}

/* 12345 s as 3h25m45s, shorter while it is short */
static void _sysinfo_power_time(char *text, size_t size, unsigned long ms) {
	unsigned long s = ms / 1000UL;

	if(s < 60)
		snprintf(text, size, "%lu.%lu s", s, (ms / 100UL) % 10);
	else if(s < 3600)
		snprintf(text, size, "%lum%02lus", s / 60, s % 60);
	else
		snprintf(text, size, "%luh%02lum", s / 3600, (s / 60) % 60);
}

static void _sysinfo_power_values(sysinfo_t *sysinfo) {
	mp_kernel_t *kernel = &sysinfo->kernel;
	mp_power_t *power = &kernel->power;
	mp_tui_table_t *table = &sysinfo->screen.powerTable;
	mp_tui_table_t *levels = &sysinfo->screen.powerLevels;
	unsigned long total = 0;
	unsigned long hz;
	char text[MP_TUI_TEXT_MAX];
	int level, row;

	mp_tui_switch_set(&sysinfo->screen.powerSwitch, power->fixed == MP_POWER_AUTO);
	mp_tui_switch_set(&sysinfo->screen.powerHibernate, sysinfo->hibernate == YES);

	hz = mp_clock_level_hz(power->level);
	mp_tui_label_set(&sysinfo->screen.powerLevel, "%s, %lu.%lu MHz", mp_power_name(power->level),
		_sysinfo_mhz10(hz) / 10, _sysinfo_mhz10(hz) % 10);
	if(power->fixed == MP_POWER_AUTO)
		mp_tui_label_set(&sysinfo->screen.powerGovernor, "The governor wants %s, the holds %s",
			mp_power_name(power->governed), mp_power_name(power->floor));
	else
		mp_tui_label_set(&sysinfo->screen.powerGovernor, "Fixed at %s, the holds %s",
			mp_power_name(power->fixed), mp_power_name(power->floor));

	mp_tui_table_set(table, 0, 0, "%s", "Load");
	mp_tui_table_set(table, 0, 1, "%u %%", (unsigned int)power->load);
	mp_tui_table_set(table, 1, 0, "%s", "One level down");
	mp_tui_table_set(table, 1, 1, "%u %%", (unsigned int)power->projected);
	mp_tui_table_set(table, 2, 0, "%s", "Wake ups");
	mp_tui_table_set(table, 2, 1, "%lu /s", power->wakeups);
	mp_tui_table_set(table, 3, 0, "%s", "Level changes");
	mp_tui_table_set(table, 3, 1, "%lu", power->switches);
	mp_tui_table_set(table, 4, 0, "%s", "Deep sleeps");
	mp_tui_table_set(table, 4, 1, "%lu", power->deep);
	mp_tui_table_set(table, 5, 0, "%s", "Deepest");
	mp_tui_table_set(table, 5, 1, "%s", mp_clock_deep());

	row = _SYSINFO_POWER_FIGURES;
#ifdef SUPPORT_COMMON_SENSOR
	{
		mp_sensor_t *sensor = NULL;
		float v[3];

		while(row < _SYSINFO_POWER_FIGURES + sysinfo->screen.powerSensorRows && (sensor = mp_sensor_next(kernel, sensor)) != NULL) {
			if(!_sysinfo_power_sensor(sensor))
				continue;
			mp_tui_table_set(table, row, 0, "%s", sensor->name);
			if(mp_sensor_read(sensor, v, NULL) != TRUE)
				mp_tui_table_set(table, row, 1, "%s", "--");
			else if(sensor->type == MP_SENSOR_VOLTAGE)
				mp_tui_table_set(table, row, 1, "%.3f V", (double)v[0]);
			else
				mp_tui_table_set(table, row, 1, "%.1f mA", (double)v[0] * 1000.0);
			row++;
		}
	}
#endif
	for(; row < _SYSINFO_POWER_FIGURES + sysinfo->screen.powerSensorRows; row++) {
		mp_tui_table_set(table, row, 0, "%s", "");
		mp_tui_table_set(table, row, 1, "%s", "");
	}

	_sysinfo_power_holds(power, text, sizeof(text));
	mp_tui_label_set(&sysinfo->screen.powerHolds, "%s", text);

	for(level = 0; level < MP_POWER_LEVELS; level++)
		total += power->levelMs[level];
	for(level = MP_POWER_LEVELS - 1; level >= 0; level--) {
		row = MP_POWER_LEVELS - 1 - level;
		hz = mp_clock_level_hz((mp_power_level_t)level);
		mp_tui_table_set(levels, row, 0, "%s%s", mp_power_name((mp_power_level_t)level), level == (int)power->level ? " <" : "");
		mp_tui_table_set(levels, row, 1, "%lu", (hz + 500000UL) / 1000000UL);
		_sysinfo_power_time(text, sizeof(text), power->levelMs[level]);
		mp_tui_table_set(levels, row, 2, "%s", text);
		mp_tui_table_set(levels, row, 3, "%lu %%",
			total ? (unsigned long)((unsigned long long)power->levelMs[level] * 100ULL / total) : 0UL);
	}
}

static void _sysinfo_on_governor(mp_tui_switch_t *sw) {
	sysinfo_t *sysinfo = sw->user;

	mp_power_fix(&sysinfo->kernel, sw->on ? MP_POWER_AUTO : MP_POWER_HIGH);
	_sysinfo_power_values(sysinfo);
}

static void _sysinfo_on_hibernate(mp_tui_switch_t *sw) {
	sysinfo_t *sysinfo = sw->user;

	_sysinfo_hibernate_enable(sysinfo, sw->on ? YES : NO);
	_sysinfo_power_values(sysinfo);
}

static void _sysinfo_power_page(mp_tui_widget_t *page, void *user) {
	sysinfo_t *sysinfo = user;
	mp_tui_t *tui = page->tui;
	const mp_tui_font_t *font = tui->theme.font;
	mp_tui_table_t *table = &sysinfo->screen.powerTable;
	mp_tui_table_t *levels = &sysinfo->screen.powerLevels;
	int line = font->lineHeight + 1;
	int w = page->rect.w - 16;
	int y = 4;

	mp_tui_switch(&sysinfo->screen.powerSwitch, page, 8, y, w, line + 10, "Governor", sysinfo->kernel.power.fixed == MP_POWER_AUTO);
	sysinfo->screen.powerSwitch.onChange = _sysinfo_on_governor;
	sysinfo->screen.powerSwitch.user = sysinfo;
	y += line + 14;

	mp_tui_switch(&sysinfo->screen.powerHibernate, page, 8, y, w, line + 10, "Hibernate", sysinfo->hibernate == YES);
	sysinfo->screen.powerHibernate.onChange = _sysinfo_on_hibernate;
	sysinfo->screen.powerHibernate.user = sysinfo;
	y += line + 14;

	mp_tui_label_wrap(&sysinfo->screen.powerLevel, page, 8, &y, w, line, font, "");
	mp_tui_label_color(&sysinfo->screen.powerLevel, tui->theme.accent);
	mp_tui_label_wrap(&sysinfo->screen.powerGovernor, page, 8, &y, w, line, font, "");
	mp_tui_label_color(&sysinfo->screen.powerGovernor, tui->theme.muted);
	y += 6;

	sysinfo->screen.powerSensorRows = _sysinfo_power_sensors(&sysinfo->kernel);
	mp_tui_table(table, page, 8, y, w, _sysinfo_power_columns, 2, &sysinfo->screen.powerCell[0][0], 8);
	mp_tui_table_rows(table, _SYSINFO_POWER_FIGURES + sysinfo->screen.powerSensorRows);
	y += table->widget.rect.h + 6;

	mp_tui_label_wrap(&sysinfo->screen.powerHolds, page, 8, &y, w, line, font, "");
	y += 6;

	mp_tui_table(levels, page, 8, y, w, _sysinfo_level_columns, 4, &sysinfo->screen.powerLevelCell[0][0], MP_POWER_LEVELS);
	mp_tui_table_rows(levels, MP_POWER_LEVELS);

	sysinfo->screen.powerReady = 1;
	_sysinfo_power_values(sysinfo);
}

void _sysinfo_power_open(sysinfo_t *sysinfo) {
	mp_tui_t *tui = &sysinfo->screen.tui;

	sysinfo->screen.powerIndex = -1;
	if(mp_tui_app(tui, "Power", _sysinfo_power_page, sysinfo))
		sysinfo->screen.powerIndex = tui->appCount - 1;
}

void _sysinfo_power_refresh(sysinfo_t *sysinfo) {
	if(sysinfo->screen.powerReady && sysinfo->screen.tui.opened == sysinfo->screen.powerIndex)
		_sysinfo_power_values(sysinfo);
}

#endif
