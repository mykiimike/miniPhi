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

#ifdef SUPPORT_COMMON_DEVICE

#include <limits.h>

/**
 * Commands a registered device announces. The driver points device->commands
 * at a static table after register. An app lists them and sends one by name.
 */

static int _mp_device_command_same(const char *a, const char *b) {
	if(!a || !b)
		return(0);
	while(*a && *b) {
		unsigned char ca = (unsigned char)*a++;
		unsigned char cb = (unsigned char)*b++;

		if(ca >= 'A' && ca <= 'Z')
			ca = (unsigned char)(ca - 'A' + 'a');
		if(cb >= 'A' && cb <= 'Z')
			cb = (unsigned char)(cb - 'A' + 'a');
		if(ca != cb)
			return(0);
	}
	return(*a == *b);
}

static const mp_device_command_t *_mp_device_commands(mp_device_registry_t *device) {
	if(!device || device->removed == YES || !device->driver)
		return(NULL);
	return(device->commands);
}

int mp_device_command_count(mp_device_registry_t *device) {
	const mp_device_command_t *command;
	int n = 0;

	command = _mp_device_commands(device);
	if(!command)
		return(0);
	while(command[n].name)
		n++;
	return(n);
}

const mp_device_command_t *mp_device_command_at(mp_device_registry_t *device, int index) {
	const mp_device_command_t *command;
	int n;

	if(index < 0)
		return(NULL);
	command = _mp_device_commands(device);
	if(!command)
		return(NULL);
	for(n = 0; command[n].name; n++)
		if(n == index)
			return(&command[n]);
	return(NULL);
}

const mp_device_command_t *mp_device_command_find(mp_device_registry_t *device, const char *name) {
	const mp_device_command_t *command;
	int n;

	if(!name || !name[0])
		return(NULL);
	command = _mp_device_commands(device);
	if(!command)
		return(NULL);
	for(n = 0; command[n].name; n++)
		if(_mp_device_command_same(command[n].name, name))
			return(&command[n]);
	return(NULL);
}

static int _mp_device_command_bounded(const mp_device_command_t *command, float number) {
	if(command->max < command->min)
		return(1);
	if(number < command->min || number > command->max)
		return(0);
	return(1);
}

static mp_ret_t _mp_device_command_int(const char *text, long *out) {
	int neg = 0;
	int digits = 0;
	unsigned long long mag = 0;

	if(!text || !text[0])
		return(FALSE);
	if(text[0] == '+' || text[0] == '-') {
		neg = text[0] == '-';
		text++;
	}
	while(text[0] >= '0' && text[0] <= '9') {
		unsigned int d = (unsigned int)(text[0] - '0');

		if(mag > (ULLONG_MAX - d) / 10ULL)
			return(FALSE);
		mag = mag * 10ULL + d;
		digits++;
		text++;
	}
	if(!digits || text[0])
		return(FALSE);
	if(neg) {
		if(mag > (unsigned long long)LONG_MAX + 1ULL)
			return(FALSE);
		if(mag == (unsigned long long)LONG_MAX + 1ULL)
			*out = LONG_MIN;
		else
			*out = -(long)mag;
	}
	else {
		if(mag > (unsigned long long)LONG_MAX)
			return(FALSE);
		*out = (long)mag;
	}
	return(TRUE);
}

static mp_ret_t _mp_device_command_float(const char *text, float *out) {
	int neg = 0;
	int digits = 0;
	int dot = 0;
	float value = 0.0f;
	float place = 0.1f;

	if(!text || !text[0])
		return(FALSE);
	if(text[0] == '+' || text[0] == '-') {
		neg = text[0] == '-';
		text++;
	}
	while(text[0]) {
		if(text[0] == '.') {
			if(dot)
				return(FALSE);
			dot = 1;
			text++;
			continue;
		}
		if(text[0] < '0' || text[0] > '9')
			return(FALSE);
		if(!dot)
			value = value * 10.0f + (float)(text[0] - '0');
		else {
			value += place * (float)(text[0] - '0');
			place *= 0.1f;
		}
		if(value > 1.0e20f)
			return(FALSE);
		digits++;
		text++;
	}
	if(!digits)
		return(FALSE);
	*out = neg ? -value : value;
	return(TRUE);
}

static mp_ret_t _mp_device_command_bool(const char *text, int *on) {
	if(!text)
		return(FALSE);
	if(_mp_device_command_same(text, "1") || _mp_device_command_same(text, "on") ||
		_mp_device_command_same(text, "true") || _mp_device_command_same(text, "yes")) {
		*on = 1;
		return(TRUE);
	}
	if(_mp_device_command_same(text, "0") || _mp_device_command_same(text, "off") ||
		_mp_device_command_same(text, "false") || _mp_device_command_same(text, "no")) {
		*on = 0;
		return(TRUE);
	}
	return(FALSE);
}

static mp_ret_t _mp_device_command_parse(const mp_device_command_t *command, const char *text, mp_device_value_t *value) {
	int i;
	long integer;
	float number;
	int on;

	value->arg = command->arg;
	if(command->arg == MP_DEVICE_ARG_NONE) {
		if(text && text[0])
			return(FALSE);
		return(TRUE);
	}
	if(command->arg == MP_DEVICE_ARG_BOOL) {
		if(_mp_device_command_bool(text, &on) == FALSE)
			return(FALSE);
		value->as.on = on;
		return(TRUE);
	}
	if(command->arg == MP_DEVICE_ARG_INT) {
		if(_mp_device_command_int(text, &integer) == FALSE)
			return(FALSE);
		if(!_mp_device_command_bounded(command, (float)integer))
			return(FALSE);
		value->as.integer = integer;
		return(TRUE);
	}
	if(command->arg == MP_DEVICE_ARG_FLOAT) {
		if(_mp_device_command_float(text, &number) == FALSE)
			return(FALSE);
		if(!_mp_device_command_bounded(command, number))
			return(FALSE);
		value->as.number = number;
		return(TRUE);
	}
	if(command->arg == MP_DEVICE_ARG_SELECT) {
		if(!text || !command->choice)
			return(FALSE);
		for(i = 0; command->choice[i]; i++) {
			if(_mp_device_command_same(command->choice[i], text)) {
				value->as.index = i;
				return(TRUE);
			}
		}
		return(FALSE);
	}
	return(FALSE);
}

mp_ret_t mp_device_command(mp_device_registry_t *device, const char *name, const char *text) {
	const mp_device_command_t *command;
	mp_device_value_t value;

	if(!device || device->removed == YES || !device->driver)
		return(FALSE);
	command = mp_device_command_find(device, name);
	if(!command || !command->set)
		return(FALSE);
	memset(&value, 0, sizeof(value));
	if(_mp_device_command_parse(command, text, &value) == FALSE)
		return(FALSE);
	return(command->set(device, &value));
}

mp_ret_t mp_device_command_get(mp_device_registry_t *device, const char *name, mp_device_value_t *value) {
	const mp_device_command_t *command;

	if(!device || device->removed == YES || !device->driver || !value)
		return(FALSE);
	command = mp_device_command_find(device, name);
	if(!command || !command->get)
		return(FALSE);
	return(command->get(device, value));
}

static int _mp_device_command_copy(char *dst, int cap, const char *src, int n) {
	int i;
	int keep;

	if(n < 0)
		n = 0;
	if(!dst || cap <= 0)
		return(n);
	keep = n < cap - 1 ? n : cap - 1;
	for(i = 0; i < keep; i++)
		dst[i] = src[i];
	dst[keep] = 0;
	return(n);
}

static int _mp_device_command_out(char *dst, int cap, const char *text) {
	int n = 0;

	if(!text)
		text = "";
	while(text[n])
		n++;
	return(_mp_device_command_copy(dst, cap, text, n));
}

static int _mp_device_command_int_text(char *dst, int cap, long number) {
	char buf[24];
	char rev[20];
	int n = 0;
	int r = 0;
	unsigned long long mag;

	if(number < 0) {
		buf[n++] = '-';
		mag = (unsigned long long)(-(number + 1)) + 1ULL;
	}
	else
		mag = (unsigned long long)number;
	if(mag == 0)
		buf[n++] = '0';
	else {
		while(mag && r < (int)sizeof(rev)) {
			rev[r++] = (char)('0' + (int)(mag % 10ULL));
			mag /= 10ULL;
		}
		while(r > 0)
			buf[n++] = rev[--r];
	}
	return(_mp_device_command_copy(dst, cap, buf, n));
}

static int _mp_device_command_whole(char *buf, int cap, int n, float number) {
	int places = 0;
	float w = number;

	if(w < 1.0f) {
		if(n < cap)
			buf[n] = '0';
		return(n + 1);
	}
	while(w >= 10.0f && places < 16) {
		w /= 10.0f;
		places++;
	}
	while(places >= 0 && n < cap) {
		int d = (int)w;

		if(d < 0)
			d = 0;
		if(d > 9)
			d = 9;
		buf[n++] = (char)('0' + d);
		w = (w - (float)d) * 10.0f;
		places--;
	}
	return(n);
}

static int _mp_device_command_float_text(char *dst, int cap, float number) {
	char buf[48];
	int n = 0;
	int neg = 0;
	int i;
	int digits = 6;
	unsigned long long scaled;
	unsigned long long whole;
	unsigned long long frac;

	if(number < 0.0f) {
		neg = 1;
		number = -number;
	}
	if(number >= 1000000.0f) {
		if(neg)
			buf[n++] = '-';
		n = _mp_device_command_whole(buf, (int)sizeof(buf), n, number);
		return(_mp_device_command_copy(dst, cap, buf, n));
	}
	scaled = (unsigned long long)(number * 1000000.0f + 0.5f);
	whole = scaled / 1000000ULL;
	frac = scaled % 1000000ULL;
	if(neg && scaled != 0)
		buf[n++] = '-';
	n = _mp_device_command_whole(buf, (int)sizeof(buf), n, (float)whole);
	{
		char fracDigits[6];

		for(i = 5; i >= 0; i--) {
			fracDigits[i] = (char)('0' + (int)(frac % 10ULL));
			frac /= 10ULL;
		}
		while(digits > 0 && fracDigits[digits - 1] == '0')
			digits--;
		if(digits > 0 && n < (int)sizeof(buf)) {
			buf[n++] = '.';
			for(i = 0; i < digits && n < (int)sizeof(buf); i++)
				buf[n++] = fracDigits[i];
		}
	}
	return(_mp_device_command_copy(dst, cap, buf, n));
}

int mp_device_command_text(const mp_device_command_t *command, const mp_device_value_t *value, char *dst, int cap) {
	int i;

	if(dst && cap > 0)
		dst[0] = 0;
	if(!command || !value)
		return(0);
	if(command->arg == MP_DEVICE_ARG_NONE)
		return(0);
	if(command->arg == MP_DEVICE_ARG_BOOL)
		return(_mp_device_command_out(dst, cap, value->as.on ? "1" : "0"));
	if(command->arg == MP_DEVICE_ARG_INT)
		return(_mp_device_command_int_text(dst, cap, value->as.integer));
	if(command->arg == MP_DEVICE_ARG_FLOAT)
		return(_mp_device_command_float_text(dst, cap, value->as.number));
	if(command->arg == MP_DEVICE_ARG_SELECT) {
		if(!command->choice || value->as.index < 0)
			return(0);
		for(i = 0; command->choice[i]; i++)
			if(i == value->as.index)
				return(_mp_device_command_out(dst, cap, command->choice[i]));
		return(0);
	}
	return(0);
}

#endif
