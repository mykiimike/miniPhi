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

#ifdef SUPPORT_COMMON_COMMANDER

#ifndef _HAVE_MP_COMMANDER_H
	#include <commander/commander.h>
#endif

#include <limits.h>

/**
 * The registry is a list on the kernel. The transport tokenises, then
 * mp_command_call parses each token against the command's tables and
 * runs the callback in the context the transport still holds.
 */

static int _mp_command_same(const char *a, const char *b) {
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

static int _mp_command_name_ok(const char *name) {
	int n = 0;

	if(!name || !name[0])
		return(0);
	while(name[n]) {
		char c = name[n];
		int ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';

		if(!ok || n >= 48)
			return(0);
		n++;
	}
	return(1);
}

static mp_ret_t _mp_command_table(const mp_command_param_t *table, int asOptions) {
	int n = 0;
	int optional = 0;

	if(!table)
		return(TRUE);
	while(table[n].name) {
		if(n >= MP_COMMANDER_ARGS)
			return(FALSE);
		if(!_mp_command_name_ok(table[n].name))
			return(FALSE);
		if(table[n].arg < MP_COMMAND_ARG_NONE || table[n].arg > MP_COMMAND_ARG_SELECT)
			return(FALSE);
		if(!asOptions && table[n].arg == MP_COMMAND_ARG_NONE)
			return(FALSE);
		if(table[n].arg == MP_COMMAND_ARG_SELECT && (!table[n].choice || !table[n].choice[0]))
			return(FALSE);
		if(!asOptions) {
			if(table[n].optional)
				optional = 1;
			else if(optional)
				return(FALSE);
		}
		n++;
	}
	return(TRUE);
}

static int _mp_command_slots(const mp_command_param_t *table) {
	int n = 0;

	if(!table)
		return(0);
	while(table[n].name)
		n++;
	return(n);
}

static mp_command_t *_mp_command_named(mp_kernel_t *kernel, const char *name, mp_command_t *skip) {
	mp_list_item_t *item;

	for(item = kernel->commands.first; item != NULL; item = item->next) {
		mp_command_t *cmd = item->user;

		if(cmd != skip && _mp_command_same(cmd->name, name))
			return(cmd);
	}
	return(NULL);
}

static int _mp_command_listed(mp_kernel_t *kernel, mp_command_t *cmd) {
	mp_list_item_t *item;

	for(item = kernel->commands.first; item != NULL; item = item->next)
		if(item == &cmd->item)
			return(1);
	return(0);
}

static int _mp_command_n(mp_kernel_t *kernel) {
	int n = 0;
	mp_list_item_t *item;

	for(item = kernel->commands.first; item != NULL; item = item->next)
		n++;
	return(n);
}

void mp_commander_init(mp_kernel_t *kernel) {
	if(!kernel)
		return;
	mp_list_init(&kernel->commands);
}

void mp_commander_fini(mp_kernel_t *kernel) {
	mp_list_item_t *item;

	if(!kernel)
		return;
	item = kernel->commands.first;
	while(item) {
		mp_list_item_t *next = item->next;

		mp_list_remove(&kernel->commands, item);
		item = next;
	}
}

mp_ret_t mp_command_register(mp_kernel_t *kernel, mp_command_t *cmd,
	const char *module, const char *name, const char *label, const char *help,
	const mp_command_param_t *params, const mp_command_param_t *options,
	mp_command_fn_t fn, void *user) {
	int listed;

	if(!kernel || !cmd || !fn || !_mp_command_name_ok(name))
		return(FALSE);
	if(_mp_command_table(params, 0) == FALSE || _mp_command_table(options, 1) == FALSE)
		return(FALSE);
	if(_mp_command_named(kernel, name, cmd))
		return(FALSE);
	listed = _mp_command_listed(kernel, cmd);
	if(!listed && _mp_command_n(kernel) >= MP_COMMANDER_MAX)
		return(FALSE);

	cmd->name = name;
	cmd->label = label;
	cmd->help = help;
	cmd->module = module;
	cmd->params = params;
	cmd->options = options;
	cmd->fn = fn;
	cmd->user = user;
	if(!listed) {
		cmd->item.next = NULL;
		cmd->item.prev = NULL;
		mp_list_add_last(&kernel->commands, &cmd->item, cmd);
	}
	return(TRUE);
}

void mp_command_unregister(mp_kernel_t *kernel, mp_command_t *cmd) {
	if(!kernel || !cmd)
		return;
	if(_mp_command_listed(kernel, cmd))
		mp_list_remove(&kernel->commands, &cmd->item);
}

int mp_command_count(mp_kernel_t *kernel) {
	if(!kernel)
		return(0);
	return(_mp_command_n(kernel));
}

mp_command_t *mp_command_at(mp_kernel_t *kernel, int index) {
	int n = 0;
	mp_list_item_t *item;

	if(!kernel || index < 0)
		return(NULL);
	for(item = kernel->commands.first; item != NULL; item = item->next) {
		if(n == index)
			return(item->user);
		n++;
	}
	return(NULL);
}

mp_command_t *mp_command_find(mp_kernel_t *kernel, const char *name) {
	if(!kernel || !name || !name[0])
		return(NULL);
	return(_mp_command_named(kernel, name, NULL));
}

static int _mp_command_bounded(const mp_command_param_t *slot, float number) {
	if(slot->max < slot->min)
		return(1);
	if(number < slot->min || number > slot->max)
		return(0);
	return(1);
}

static mp_ret_t _mp_command_int(const char *text, long *out) {
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

static mp_ret_t _mp_command_float(const char *text, float *out) {
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

static mp_ret_t _mp_command_bool(const char *text, int *on) {
	if(!text)
		return(FALSE);
	if(_mp_command_same(text, "1") || _mp_command_same(text, "on") ||
		_mp_command_same(text, "true") || _mp_command_same(text, "yes")) {
		*on = 1;
		return(TRUE);
	}
	if(_mp_command_same(text, "0") || _mp_command_same(text, "off") ||
		_mp_command_same(text, "false") || _mp_command_same(text, "no")) {
		*on = 0;
		return(TRUE);
	}
	return(FALSE);
}

static mp_ret_t _mp_command_parse(const mp_command_param_t *slot, const char *text, mp_command_value_t *value) {
	int i;
	long integer;
	float number;
	int on;

	if(value->present)
		return(FALSE);
	value->arg = slot->arg;
	if(slot->arg == MP_COMMAND_ARG_NONE) {
		if(text && text[0])
			return(FALSE);
		value->present = YES;
		return(TRUE);
	}
	if(slot->arg == MP_COMMAND_ARG_BOOL) {
		if(!text) {
			value->as.on = 1;
			value->present = YES;
			return(TRUE);
		}
		if(_mp_command_bool(text, &on) == FALSE)
			return(FALSE);
		value->as.on = on;
		value->present = YES;
		return(TRUE);
	}
	if(slot->arg == MP_COMMAND_ARG_INT) {
		if(_mp_command_int(text, &integer) == FALSE)
			return(FALSE);
		if(!_mp_command_bounded(slot, (float)integer))
			return(FALSE);
		value->as.integer = integer;
		value->present = YES;
		return(TRUE);
	}
	if(slot->arg == MP_COMMAND_ARG_FLOAT) {
		if(_mp_command_float(text, &number) == FALSE)
			return(FALSE);
		if(!_mp_command_bounded(slot, number))
			return(FALSE);
		value->as.number = number;
		value->present = YES;
		return(TRUE);
	}
	if(slot->arg == MP_COMMAND_ARG_STRING) {
		if(!text || !text[0])
			return(FALSE);
		value->as.string = text;
		value->present = YES;
		return(TRUE);
	}
	if(slot->arg == MP_COMMAND_ARG_SELECT) {
		if(!text || !slot->choice)
			return(FALSE);
		for(i = 0; slot->choice[i]; i++) {
			if(_mp_command_same(slot->choice[i], text)) {
				value->as.index = i;
				value->present = YES;
				return(TRUE);
			}
		}
		return(FALSE);
	}
	return(FALSE);
}

static int _mp_command_option_token(const char *token) {
	return(token && token[0] == '-' && token[1] == '-' && token[2] && token[2] != '=');
}

static int _mp_command_option_split(const char *token, char *name, int cap, const char **value, int *hasValue) {
	int i = 0;
	const char *body = token + 2;

	*hasValue = 0;
	*value = NULL;
	while(body[i] && body[i] != '=') {
		if(i >= cap - 1)
			return(0);
		name[i] = body[i];
		i++;
	}
	if(i == 0)
		return(0);
	name[i] = 0;
	if(body[i] == '=') {
		*hasValue = 1;
		*value = body + i + 1;
	}
	return(1);
}

static int _mp_command_option_index(const mp_command_param_t *options, const char *name) {
	int n;

	if(!options)
		return(-1);
	for(n = 0; options[n].name; n++)
		if(_mp_command_same(options[n].name, name))
			return(n);
	return(-1);
}

static void _mp_command_say(char *out, int cap, const char *text) {
	int n = 0;

	if(!out || cap <= 0)
		return;
	if(!text)
		text = "";
	while(text[n] && n < cap - 1) {
		out[n] = text[n];
		n++;
	}
	out[n] = 0;
}

static void _mp_command_clear(mp_command_value_t *values, int count) {
	int i;

	for(i = 0; i < count; i++) {
		memset(&values[i], 0, sizeof(values[i]));
		values[i].present = NO;
	}
}

mp_ret_t mp_command_call(mp_kernel_t *kernel, mp_command_ctx_t *ctx,
	const char *name, int argc, const char *const *argv,
	char *out, int outCap) {
	mp_command_t *cmd;
	mp_command_value_t params[MP_COMMANDER_ARGS];
	mp_command_value_t options[MP_COMMANDER_ARGS];
	int paramCount;
	int optionCount;
	int positional = 0;
	int i;
	const char *reason = NULL;

	if(!kernel || !name || !name[0] || argc < 0 || (argc > 0 && !argv))
		reason = "bad args";
	else if(!ctx)
		reason = "no context";
	else {
		cmd = mp_command_find(kernel, name);
		if(!cmd || !cmd->fn)
			reason = "unknown";
	}
	if(reason) {
		_mp_command_say(out, outCap, reason);
		return(FALSE);
	}

	paramCount = _mp_command_slots(cmd->params);
	optionCount = _mp_command_slots(cmd->options);
	_mp_command_clear(params, paramCount);
	_mp_command_clear(options, optionCount);

	for(i = 0; i < argc && !reason; i++) {
		if(_mp_command_option_token(argv[i])) {
			char oname[49];
			const char *inlineValue = NULL;
			int hasValue = 0;
			int index;

			if(!_mp_command_option_split(argv[i], oname, (int)sizeof(oname), &inlineValue, &hasValue))
				reason = "bad args";
			else {
				index = _mp_command_option_index(cmd->options, oname);
				if(index < 0)
					reason = "bad args";
				else if(hasValue) {
					if(_mp_command_parse(&cmd->options[index], inlineValue, &options[index]) == FALSE)
						reason = "bad args";
				}
				else if(cmd->options[index].arg == MP_COMMAND_ARG_NONE ||
					cmd->options[index].arg == MP_COMMAND_ARG_BOOL) {
					int takeNext = 0;

					if(cmd->options[index].arg == MP_COMMAND_ARG_BOOL && i + 1 < argc &&
						!_mp_command_option_token(argv[i + 1])) {
						int on;

						if(_mp_command_bool(argv[i + 1], &on) == TRUE)
							takeNext = 1;
					}
					if(takeNext) {
						i++;
						if(_mp_command_parse(&cmd->options[index], argv[i], &options[index]) == FALSE)
							reason = "bad args";
					}
					else if(_mp_command_parse(&cmd->options[index], NULL, &options[index]) == FALSE)
						reason = "bad args";
				}
				else if(i + 1 >= argc || _mp_command_option_token(argv[i + 1]) ||
					_mp_command_parse(&cmd->options[index], argv[i + 1], &options[index]) == FALSE)
					reason = "bad args";
				else
					i++;
			}
		}
		else if(positional >= paramCount ||
			_mp_command_parse(&cmd->params[positional], argv[i], &params[positional]) == FALSE)
			reason = "bad args";
		else
			positional++;
	}

	if(!reason) {
		for(i = 0; i < paramCount; i++) {
			if(!cmd->params[i].optional && !params[i].present)
				reason = "bad args";
		}
	}
	if(reason) {
		_mp_command_say(out, outCap, reason);
		return(FALSE);
	}
	if(cmd->fn(kernel, ctx,
		paramCount ? params : NULL, paramCount,
		optionCount ? options : NULL, optionCount,
		cmd->user) == FALSE) {
		_mp_command_say(out, outCap, "refused");
		return(FALSE);
	}
	_mp_command_say(out, outCap, "");
	return(TRUE);
}

static int _mp_command_copy(char *dst, int cap, const char *src, int n) {
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

static int _mp_command_out(char *dst, int cap, const char *text) {
	int n = 0;

	if(!text)
		text = "";
	while(text[n])
		n++;
	return(_mp_command_copy(dst, cap, text, n));
}

static int _mp_command_int_text(char *dst, int cap, long number) {
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
	return(_mp_command_copy(dst, cap, buf, n));
}

static int _mp_command_whole(char *buf, int cap, int n, float number) {
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

static int _mp_command_float_text(char *dst, int cap, float number) {
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
		n = _mp_command_whole(buf, (int)sizeof(buf), n, number);
		return(_mp_command_copy(dst, cap, buf, n));
	}
	scaled = (unsigned long long)(number * 1000000.0f + 0.5f);
	whole = scaled / 1000000ULL;
	frac = scaled % 1000000ULL;
	if(neg && scaled != 0)
		buf[n++] = '-';
	n = _mp_command_whole(buf, (int)sizeof(buf), n, (float)whole);
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
	return(_mp_command_copy(dst, cap, buf, n));
}

int mp_command_value_text(const mp_command_param_t *param, const mp_command_value_t *value, char *dst, int cap) {
	int i;

	if(dst && cap > 0)
		dst[0] = 0;
	if(!param || !value || !value->present)
		return(0);
	if(param->arg == MP_COMMAND_ARG_NONE)
		return(0);
	if(param->arg == MP_COMMAND_ARG_BOOL)
		return(_mp_command_out(dst, cap, value->as.on ? "1" : "0"));
	if(param->arg == MP_COMMAND_ARG_INT)
		return(_mp_command_int_text(dst, cap, value->as.integer));
	if(param->arg == MP_COMMAND_ARG_FLOAT)
		return(_mp_command_float_text(dst, cap, value->as.number));
	if(param->arg == MP_COMMAND_ARG_STRING)
		return(_mp_command_out(dst, cap, value->as.string));
	if(param->arg == MP_COMMAND_ARG_SELECT) {
		if(!param->choice || value->as.index < 0)
			return(0);
		for(i = 0; param->choice[i]; i++)
			if(i == value->as.index)
				return(_mp_command_out(dst, cap, param->choice[i]));
		return(0);
	}
	return(0);
}

#endif
