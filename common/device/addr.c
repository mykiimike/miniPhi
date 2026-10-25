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

/**
 * An address is typeN.model.serial. The three fields share one filter, so a
 * stored address and a lookup compare equal. A dot never survives inside a
 * field: it becomes a dash, and splitting on '.' is enough.
 */

static void _mp_device_address_clear(char *dst, int cap) {
	if(dst && cap > 0)
		dst[0] = 0;
}

static int _mp_device_address_put(char *dst, int cap, int at, char c) {
	if(!dst || at < 0 || at + 1 >= cap)
		return(-1);
	dst[at] = c;
	return(at + 1);
}

/** n bytes of src, not a C string. Writes the NUL. -1 leaves dst cleared by the caller. */
static int _mp_device_address_field_n(char *dst, int cap, const char *src, int n) {
	int at = 0;
	int i;

	if(!dst || cap < 2 || !src || n <= 0)
		return(-1);

	for(i = 0; i < n; i++) {
		unsigned char c = (unsigned char)src[i];

		if(c >= 0x80 || c < 0x20 || c == 0x7f)
			return(-1);
		if(c >= 'A' && c <= 'Z')
			c = (unsigned char)(c - 'A' + 'a');
		else if(c == ' ' || c == '.')
			c = '-';
		at = _mp_device_address_put(dst, cap, at, (char)c);
		if(at < 0)
			return(-1);
	}
	dst[at] = 0;
	return(at);
}

mp_ret_t mp_device_address_field(char *dst, int cap, const char *src) {
	int n = 0;

	if(!src) {
		_mp_device_address_clear(dst, cap);
		return(FALSE);
	}
	while(src[n])
		n++;
	if(_mp_device_address_field_n(dst, cap, src, n) < 0) {
		_mp_device_address_clear(dst, cap);
		return(FALSE);
	}
	return(TRUE);
}

static int _mp_device_address_num(char *dst, int cap, int at, int num) {
	char tmp[16];
	int n = 0;
	int value;

	if(num < 0)
		return(-1);
	value = num;
	do {
		if(n >= (int)sizeof(tmp))
			return(-1);
		tmp[n++] = (char)('0' + (value % 10));
		value /= 10;
	} while(value);

	while(n > 0) {
		n--;
		at = _mp_device_address_put(dst, cap, at, tmp[n]);
		if(at < 0)
			return(-1);
	}
	return(at);
}

/** The stem is letters only. A digit belongs to the index that follows it. */
static int _mp_device_address_stem(char *dst, int cap, int at, const char *type) {
	int start = at;
	int n = 0;
	int i;

	if(!type)
		return(-1);
	while(type[n])
		n++;
	at = _mp_device_address_field_n(dst + start, cap - start, type, n);
	if(at < 0)
		return(-1);
	at += start;
	for(i = start; i < at; i++)
		if(dst[i] < 'a' || dst[i] > 'z')
			return(-1);
	return(at);
}

mp_ret_t mp_device_address(char *dst, int cap, const char *type, int num, const char *model, const char *serial) {
	int at = 0;
	int n;

	if(!dst || cap < 2 || num < 0) {
		_mp_device_address_clear(dst, cap);
		return(FALSE);
	}

	at = _mp_device_address_stem(dst, cap, at, type);
	if(at < 0)
		goto fail;
	at = _mp_device_address_num(dst, cap, at, num);
	if(at < 0)
		goto fail;
	at = _mp_device_address_put(dst, cap, at, '.');
	if(at < 0)
		goto fail;

	if(!model)
		goto fail;
	n = 0;
	while(model[n])
		n++;
	n = _mp_device_address_field_n(dst + at, cap - at, model, n);
	if(n < 0)
		goto fail;
	at += n;

	at = _mp_device_address_put(dst, cap, at, '.');
	if(at < 0)
		goto fail;

	if(!serial)
		goto fail;
	n = 0;
	while(serial[n])
		n++;
	n = _mp_device_address_field_n(dst + at, cap - at, serial, n);
	if(n < 0)
		goto fail;
	at += n;
	dst[at] = 0;
	return(TRUE);

fail:
	_mp_device_address_clear(dst, cap);
	return(FALSE);
}

static mp_ret_t _mp_device_address_take_num(const char *text, int *num) {
	long value = 0;
	int digits = 0;

	if(!text || !text[0])
		return(FALSE);
	if(text[0] == '0' && text[1] != 0)
		return(FALSE);

	while(text[digits]) {
		unsigned char c = (unsigned char)text[digits];

		if(c < '0' || c > '9')
			return(FALSE);
		/* int is 16-bit on MSP430; an index still fits well below that */
		if(value > (32767L - (long)(c - '0')) / 10L)
			return(FALSE);
		value = value * 10 + (long)(c - '0');
		digits++;
	}
	*num = (int)value;
	return(TRUE);
}

static mp_ret_t _mp_device_address_parts(const char *address, const char *start[3], int len[3]) {
	int field = 0;
	const char *p;

	if(!address || !address[0])
		return(FALSE);

	start[0] = address;
	for(p = address; *p; p++) {
		if(*p != '.')
			continue;
		len[field] = (int)(p - start[field]);
		if(len[field] <= 0)
			return(FALSE);
		field++;
		if(field > 2)
			return(FALSE);
		start[field] = p + 1;
	}
	if(field != 2)
		return(FALSE);
	len[2] = (int)(p - start[2]);
	if(len[2] <= 0)
		return(FALSE);
	return(TRUE);
}

mp_ret_t mp_device_address_split(const char *address, char *type, int typeCap, int *num, char *model, int modelCap, char *serial, int serialCap) {
	const char *start[3];
	int len[3];
	char first[MP_DEVICE_ADDRESS];
	int stem;
	int at;

	if(!type || typeCap < 2 || !num || !model || modelCap < 2 || !serial || serialCap < 2)
		return(FALSE);
	if(_mp_device_address_parts(address, start, len) == FALSE)
		goto fail;
	if(_mp_device_address_field_n(first, (int)sizeof(first), start[0], len[0]) < 0)
		goto fail;

	stem = 0;
	while(first[stem] >= 'a' && first[stem] <= 'z')
		stem++;
	if(stem == 0 || stem + 1 > typeCap)
		goto fail;
	if(_mp_device_address_take_num(first + stem, num) == FALSE)
		goto fail;
	for(at = 0; at < stem; at++)
		type[at] = first[at];
	type[stem] = 0;

	if(_mp_device_address_field_n(model, modelCap, start[1], len[1]) < 0)
		goto fail;
	if(_mp_device_address_field_n(serial, serialCap, start[2], len[2]) < 0)
		goto fail;
	return(TRUE);

fail:
	_mp_device_address_clear(type, typeCap);
	_mp_device_address_clear(model, modelCap);
	_mp_device_address_clear(serial, serialCap);
	*num = -1;
	return(FALSE);
}

mp_ret_t mp_device_address_canon(char *dst, int cap, const char *address) {
	char type[32];
	char model[MP_DEVICE_ADDRESS];
	char serial[MP_DEVICE_ADDRESS];
	int num;

	if(mp_device_address_split(address, type, (int)sizeof(type), &num, model, (int)sizeof(model), serial, (int)sizeof(serial)) == FALSE) {
		_mp_device_address_clear(dst, cap);
		return(FALSE);
	}
	return(mp_device_address(dst, cap, type, num, model, serial));
}

#endif
