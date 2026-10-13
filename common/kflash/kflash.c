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

#ifdef SUPPORT_COMMON_KFLASH

#if !defined(MP_FLASH_PAGE_SIZE) || !defined(MP_FLASH_ORIGIN)
	#error "KFLASH needs an arch with MP_FLASH_PAGE_SIZE, MP_FLASH_ORIGIN and mp_flash_program()"
#endif

/**
@defgroup mpCommonKFlash Flash key/value store

@ingroup mpCommon

The store is the last two pages of the internal flash. Each is a bank that
starts with a magic and a sequence number; the valid bank with the highest
sequence is the active one.

Records follow the bank header, four-byte aligned: the FNV-1a hash of the
key (4 bytes), the value size (2), a Fletcher-16 of both and of the value (2),
then the value. A record is programmed with its check last, so one cut by a
reset fails it and is skipped. The last valid record of a hash is its value.

When the active bank is full, the latest record of every hash is copied to the
other bank, whose header is programmed last. Until then the old bank stays
the one read at boot.

@{
*/

#define KFLASH_MAGIC 0x314C464BUL
#define KFLASH_HEAD 8
#define KFLASH_ERASED 0xFFFFFFFFUL
#define KFLASH_PAGE ((unsigned int)MP_FLASH_PAGE_SIZE)
#define KFLASH_ALIGN(n) (((unsigned int)(n) + 3U) & ~3U)

typedef struct {
	unsigned long hash;
	unsigned int size;
	unsigned int check;
} _kflash_head_t;

static unsigned long _kflash_hash(const char *key) {
	unsigned long hash = 2166136261UL;

	while(*key) {
		hash ^= (unsigned char)*key++;
		hash = (hash * 16777619UL) & 0xFFFFFFFFUL;
	}
	/* all ones reads as erased flash */
	if(hash == KFLASH_ERASED)
		hash--;
	return(hash);
}

static unsigned long _kflash_le32(const unsigned char *raw) {
	return((unsigned long)raw[0] | (unsigned long)raw[1] << 8 | (unsigned long)raw[2] << 16 | (unsigned long)raw[3] << 24);
}

static void _kflash_put32(unsigned char *raw, unsigned long value) {
	raw[0] = (unsigned char)value;
	raw[1] = (unsigned char)(value >> 8);
	raw[2] = (unsigned char)(value >> 16);
	raw[3] = (unsigned char)(value >> 24);
}

/* never 0xFFFF, so a check that was not programmed never matches */
static unsigned int _kflash_sum(unsigned long hash, unsigned int size, const unsigned char *data) {
	unsigned char head[6];
	unsigned int a = 0, b = 0, i;

	_kflash_put32(head, hash);
	head[4] = (unsigned char)size;
	head[5] = (unsigned char)(size >> 8);
	for(i = 0; i < sizeof(head); i++) {
		a = (a + head[i]) % 255;
		b = (b + a) % 255;
	}
	for(i = 0; i < size; i++) {
		a = (a + data[i]) % 255;
		b = (b + a) % 255;
	}
	return(b << 8 | a);
}

/* the record length, 0 at the end of the log, -1 when the rest can not be trusted */
static int _kflash_record(mp_kflash_t *kf, int bank, unsigned int at, _kflash_head_t *head, unsigned char *data) {
	unsigned char raw[KFLASH_HEAD];

	if(at + KFLASH_HEAD > KFLASH_PAGE)
		return(0);
	mp_flash_read(&kf->bank[bank], at, KFLASH_HEAD, raw);
	head->hash = _kflash_le32(raw);
	head->size = raw[4] | (unsigned int)raw[5] << 8;
	head->check = raw[6] | (unsigned int)raw[7] << 8;
	if(head->hash == KFLASH_ERASED && head->size == 0xFFFF && head->check == 0xFFFF)
		return(0);
	if(head->size > MP_KFLASH_VALUE_MAX || at + KFLASH_HEAD + KFLASH_ALIGN(head->size) > KFLASH_PAGE)
		return(-1);
	if(head->size)
		mp_flash_read(&kf->bank[bank], at + KFLASH_HEAD, head->size, data);
	return(KFLASH_HEAD + KFLASH_ALIGN(head->size));
}

static int _kflash_valid(const _kflash_head_t *head, const unsigned char *data) {
	return(head->check == _kflash_sum(head->hash, head->size, data));
}

/* offset of the last valid record of hash from offset from, or 0; value is MP_KFLASH_VALUE_MAX bytes */
static unsigned int _kflash_find(mp_kflash_t *kf, int bank, unsigned long hash, unsigned int from, _kflash_head_t *found, unsigned char *value) {
	unsigned char data[MP_KFLASH_VALUE_MAX];
	_kflash_head_t head;
	unsigned int at = from, last = 0;
	int length;

	while((length = _kflash_record(kf, bank, at, &head, data)) > 0) {
		if(head.hash == hash && _kflash_valid(&head, data)) {
			last = at;
			if(found)
				*found = head;
			if(value)
				memcpy(value, data, head.size);
		}
		at += (unsigned int)length;
	}
	return(last);
}

static void _kflash_scan(mp_kflash_t *kf) {
	unsigned char data[MP_KFLASH_VALUE_MAX];
	_kflash_head_t head;
	unsigned int at = KFLASH_HEAD;
	int length;

	while((length = _kflash_record(kf, kf->active, at, &head, data)) > 0)
		at += (unsigned int)length;
	kf->full = length < 0;
	kf->used = at;
}

static int _kflash_bank_valid(mp_kflash_t *kf, int bank, unsigned long *sequence) {
	unsigned char raw[KFLASH_HEAD];

	mp_flash_read(&kf->bank[bank], 0, KFLASH_HEAD, raw);
	if(_kflash_le32(raw) != KFLASH_MAGIC)
		return(0);
	*sequence = _kflash_le32(raw + 4);
	return(*sequence != KFLASH_ERASED);
}

static mp_ret_t _kflash_seal(mp_kflash_t *kf, int bank, unsigned long sequence) {
	unsigned char raw[KFLASH_HEAD];

	_kflash_put32(raw, KFLASH_MAGIC);
	_kflash_put32(raw + 4, sequence);
	return(mp_flash_program(&kf->bank[bank], 0, KFLASH_HEAD, raw));
}

static mp_ret_t _kflash_put(mp_kflash_t *kf, int bank, unsigned int at, unsigned long hash, const unsigned char *value, unsigned int size) {
	unsigned int check = _kflash_sum(hash, size, value);
	unsigned char raw[6];

	_kflash_put32(raw, hash);
	raw[4] = (unsigned char)size;
	raw[5] = (unsigned char)(size >> 8);
	if(mp_flash_program(&kf->bank[bank], at, 6, raw) == FALSE)
		return(FALSE);
	if(size && mp_flash_program(&kf->bank[bank], at + KFLASH_HEAD, (int)size, value) == FALSE)
		return(FALSE);
	raw[0] = (unsigned char)check;
	raw[1] = (unsigned char)(check >> 8);
	return(mp_flash_program(&kf->bank[bank], at + 6, 2, raw));
}

static mp_ret_t _kflash_compact(mp_kflash_t *kf) {
	unsigned char data[MP_KFLASH_VALUE_MAX];
	_kflash_head_t head;
	int from = kf->active, to = !kf->active;
	unsigned int at = KFLASH_HEAD, out = KFLASH_HEAD;
	unsigned long sequence = (kf->sequence + 1) & 0xFFFFFFFFUL;
	int length;

	if(sequence == KFLASH_ERASED)
		sequence = 0;
	if(mp_flash_erase(&kf->bank[to]) == FALSE)
		return(FALSE);
	while((length = _kflash_record(kf, from, at, &head, data)) > 0) {
		if(_kflash_valid(&head, data) && !_kflash_find(kf, from, head.hash, at + (unsigned int)length, NULL, NULL)) {
			if(_kflash_put(kf, to, out, head.hash, data, head.size) == FALSE)
				return(FALSE);
			out += KFLASH_HEAD + KFLASH_ALIGN(head.size);
		}
		at += (unsigned int)length;
	}
	if(_kflash_seal(kf, to, sequence) == FALSE)
		return(FALSE);

	kf->active = to;
	kf->sequence = sequence;
	kf->used = out;
	kf->full = 0;
	return(TRUE);
}

mp_ret_t mp_kflash_init(mp_kernel_t *kernel) {
	mp_kflash_t *kf = &kernel->kflash;
	mp_flash_addr_t base = MP_FLASH_ORIGIN + PHI_BUILD_FLASH - 2 * MP_FLASH_PAGE_SIZE;
	unsigned long first = 0, second = 0;
	int hasFirst, hasSecond;

	memset(kf, 0, sizeof(*kf));
	mp_list_init(&kf->vars);
	kf->active = -1;

	/* the image grew into the store */
	if(mp_flash_image_end() > base)
		return(FALSE);
	if(mp_flash_init(&kf->bank[0], base, KFLASH_PAGE) == FALSE)
		return(FALSE);
	if(mp_flash_init(&kf->bank[1], base + KFLASH_PAGE, KFLASH_PAGE) == FALSE)
		return(FALSE);

	hasFirst = _kflash_bank_valid(kf, 0, &first);
	hasSecond = _kflash_bank_valid(kf, 1, &second);
	if(hasFirst && hasSecond) {
		/* 32-bit sequences that wrap: the second is newer when it is ahead by less than half */
		unsigned long ahead = (second - first) & 0xFFFFFFFFUL;

		kf->active = ahead != 0 && ahead < 0x80000000UL;
		kf->sequence = kf->active ? second : first;
	}
	else if(hasFirst || hasSecond) {
		kf->active = hasSecond;
		kf->sequence = hasSecond ? second : first;
	}
	else {
		if(mp_flash_erase(&kf->bank[0]) == FALSE || _kflash_seal(kf, 0, 1) == FALSE)
			return(FALSE);
		kf->active = 0;
		kf->sequence = 1;
	}

	_kflash_scan(kf);
	return(TRUE);
}

mp_ret_t mp_kflash_get(mp_kernel_t *kernel, const char *key, void *value, int size) {
	mp_kflash_t *kf = &kernel->kflash;
	unsigned char data[MP_KFLASH_VALUE_MAX];
	_kflash_head_t head;

	if(kf->active < 0 || !key || !value || size < 0 || size > MP_KFLASH_VALUE_MAX)
		return(FALSE);
	if(!_kflash_find(kf, kf->active, _kflash_hash(key), KFLASH_HEAD, &head, data))
		return(FALSE);
	if(head.size != (unsigned int)size)
		return(FALSE);
	memcpy(value, data, (size_t)size);
	return(TRUE);
}

mp_ret_t mp_kflash_listen(mp_kernel_t *kernel, mp_kflash_var_t *var, const char *key, void *value, const void *fallback, int size, mp_kflash_on_t onUpdate, void *user) {
	if(!var || !key || !value || size <= 0 || size > MP_KFLASH_VALUE_MAX)
		return(FALSE);

	memset(var, 0, sizeof(*var));
	var->key = key;
	var->hash = _kflash_hash(key);
	var->value = value;
	var->size = size;
	var->onUpdate = onUpdate;
	var->user = user;

	if(mp_kflash_get(kernel, key, value, size) == FALSE && fallback)
		memcpy(value, fallback, (size_t)size);
	mp_list_add_last(&kernel->kflash.vars, &var->item, var);
	return(TRUE);
}

void mp_kflash_unlisten(mp_kernel_t *kernel, mp_kflash_var_t *var) {
	mp_list_remove(&kernel->kflash.vars, &var->item);
}

mp_ret_t mp_kflash_set(mp_kernel_t *kernel, const char *key, const void *value, int size) {
	mp_kflash_t *kf = &kernel->kflash;
	unsigned char current[MP_KFLASH_VALUE_MAX];
	unsigned char copy[MP_KFLASH_VALUE_MAX];
	mp_list_item_t *item, *next;
	mp_kflash_var_t *var;
	_kflash_head_t head;
	unsigned long hash;
	unsigned int length;

	if(kf->active < 0 || !key || size < 0 || size > MP_KFLASH_VALUE_MAX || (size && !value))
		return(FALSE);

	/* value may be a listener's own buffer, which the loop below overwrites */
	if(size)
		memcpy(copy, value, (size_t)size);
	hash = _kflash_hash(key);
	if(_kflash_find(kf, kf->active, hash, KFLASH_HEAD, &head, current) &&
		head.size == (unsigned int)size && memcmp(current, copy, (size_t)size) == 0)
		return(TRUE);

	length = KFLASH_HEAD + KFLASH_ALIGN(size);
	if(kf->full || kf->used + length > KFLASH_PAGE) {
		if(_kflash_compact(kf) == FALSE)
			return(FALSE);
		if(kf->used + length > KFLASH_PAGE)
			return(FALSE);
	}
	if(_kflash_put(kf, kf->active, kf->used, hash, copy, (unsigned int)size) == FALSE) {
		_kflash_scan(kf);
		return(FALSE);
	}
	kf->used += length;

	for(item = kf->vars.first; item != NULL; item = next) {
		next = item->next;
		var = item->user;
		if(var->hash != hash || var->size != size)
			continue;
		memcpy(var->value, copy, (size_t)size);
		if(var->onUpdate)
			var->onUpdate(var);
	}
	return(TRUE);
}

mp_ret_t mp_kflash_listen_bool(mp_kernel_t *kernel, mp_kflash_var_t *var, const char *key, mp_bool_t *value, mp_bool_t fallback, mp_kflash_on_t onUpdate, void *user) {
	mp_bool_t initial = fallback ? YES : NO;
	mp_ret_t ret;

	ret = mp_kflash_listen(kernel, var, key, value, &initial, sizeof(*value), onUpdate, user);
	if(ret == TRUE)
		*value = *value ? YES : NO;
	return(ret);
}

mp_ret_t mp_kflash_set_bool(mp_kernel_t *kernel, const char *key, mp_bool_t value) {
	mp_bool_t stored = value ? YES : NO;

	return(mp_kflash_set(kernel, key, &stored, sizeof(stored)));
}

void mp_kflash_state(mp_kernel_t *kernel, mp_kflash_state_t *state) {
	mp_kflash_t *kf = &kernel->kflash;
	unsigned char data[MP_KFLASH_VALUE_MAX];
	_kflash_head_t head;
	unsigned int at = KFLASH_HEAD;
	int length;

	memset(state, 0, sizeof(*state));
	state->bank = kf->active;
	if(kf->active < 0)
		return;
	state->base = kf->bank[kf->active].base;
	state->size = KFLASH_PAGE;
	state->used = kf->used;
	while((length = _kflash_record(kf, kf->active, at, &head, data)) > 0) {
		if(_kflash_valid(&head, data) && !_kflash_find(kf, kf->active, head.hash, at + (unsigned int)length, NULL, NULL))
			state->keys++;
		at += (unsigned int)length;
	}
}

/**@}*/

#endif
