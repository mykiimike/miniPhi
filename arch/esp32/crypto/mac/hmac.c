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

#ifdef ESP32C6

#include <string.h>

/**
@defgroup mpArchESP32Hmac HMAC

@ingroup mpArchESP32

Upstream HMAC-SHA-256. The key is an eFuse slot whose purpose is 8.
The engine is the SHA block, so one fuse MAC runs at a time. A block
is 64 bytes. The first byte of a word sits in the low 8 bits.

@{
*/

#define _PCR_SHA  0xCCUL
#define _PCR_DS   0xE0UL
#define _PCR_HMAC 0xE4UL

#define _START    0x40UL
#define _PURPOSE  0x44UL
#define _KEY      0x48UL
#define _PARA     0x4CUL
#define _ONE      0x50UL
#define _ING      0x54UL
#define _END      0x58UL
#define _DONE     0x5CUL
#define _INV_DS   0x64UL
#define _ERROR    0x68UL
#define _BUSY     0x6CUL
#define _MSG      0x80UL
#define _OUT      0xC0UL
#define _PAD      0xF0UL
#define _ONLY     0xF4UL

#define _UP 8u

static struct {
	uint8_t block[64];
	uint8_t held[64];
	uint8_t used;
	uint8_t has_held;
	uint8_t started;
	uint8_t gen;
	uint64_t bytes;
} _st;

static void _clock(void) {
	c6_write(C6_PCR + _PCR_HMAC, 1UL | 2UL);
	c6_write(C6_PCR + _PCR_HMAC, 1UL);
	c6_write(C6_PCR + _PCR_SHA, 1UL | 2UL);
	c6_write(C6_PCR + _PCR_SHA, 1UL);
	c6_clr(C6_PCR + _PCR_DS, 2UL);
}

static int _idle(void) {
	unsigned long n;

	for(n = 0; n < 1000000UL; n++) {
		if((c6_read(C6_HMAC + _BUSY) & 1UL) == 0)
			return(1);
	}
	return(0);
}

static void _wipe(void) {
	memset(&_st, 0, sizeof(_st));
}

static void _put(const uint8_t *block) {
	unsigned int i;

	for(i = 0; i < 16; i++) {
		uint32_t word;

		memcpy(&word, block + i * 4, 4);
		c6_write(C6_HMAC + _MSG + i * 4UL, word);
	}
}

/** write one block and strobe SET_MESSAGE_ONE. mark is 0, or ING, END, PAD, ONLY. */
static int _block(const uint8_t *block, unsigned long mark) {
	if(!_idle())
		return(0);
	_put(block);
	c6_write(C6_HMAC + _ONE, 1UL);
	if(!_idle())
		return(0);
	if(!mark)
		return(1);
	c6_write(C6_HMAC + mark, 1UL);
	return(_idle());
}

static int _arm(unsigned int slot, unsigned int purpose) {
	_clock();
	c6_write(C6_HMAC + _START, 1UL);
	c6_write(C6_HMAC + _PURPOSE, purpose);
	c6_write(C6_HMAC + _KEY, slot);
	c6_write(C6_HMAC + _PARA, 1UL);
	if((c6_read(C6_HMAC + _ERROR) & 1UL) != 0)
		return(0);
	return(1);
}

/**
 * @brief Start an upstream MAC on a slot
 *
 * @param[out] gen Generation the later calls must echo
 * @param[in] slot KEY index whose purpose is HMAC upstream
 * @return NO when the peripheral rejects the slot
 */
mp_bool_t mp_arch_hmac_fuse_init(uint8_t *gen, unsigned int slot) {
	uint8_t next;

	if(!gen)
		return(NO);
	next = (uint8_t)(_st.gen + 1);
	if(next == 0)
		next = 1;
	if(!_arm(slot, _UP)) {
		_clock();
		_wipe();
		return(NO);
	}
	_wipe();
	_st.started = 1;
	_st.gen = next;
	*gen = next;
	return(YES);
}

/**
 * @brief Feed message bytes
 *
 * A full block is held until the next one arrives, so the last block
 * can carry the padding at the end.
 */
void mp_arch_hmac_fuse_update(uint8_t gen, const void *data, unsigned int size) {
	const uint8_t *bytes = data;

	if(!_st.started || gen != _st.gen || !data || !size)
		return;
	while(size) {
		unsigned int take = 64 - _st.used;

		if(take > size)
			take = size;
		memcpy(_st.block + _st.used, bytes, take);
		_st.used += take;
		_st.bytes += take;
		bytes += take;
		size -= take;
		if(_st.used < 64)
			continue;
		if(_st.has_held) {
			if(!_block(_st.held, _ING)) {
				_clock();
				_wipe();
				return;
			}
		}
		memcpy(_st.held, _st.block, 64);
		_st.has_held = 1;
		_st.used = 0;
	}
}

static void _length(uint8_t *block, uint64_t bits) {
	int i;

	for(i = 0; i < 8; i++)
		block[56 + i] = (uint8_t)(bits >> (56 - 8 * i));
}

static int _pad_one(uint8_t *block, unsigned int used, uint64_t bits) {
	block[used] = 0x80;
	if(used + 1 < 64)
		memset(block + used + 1, 0, 64 - (used + 1));
	if(used > 55)
		return(0);
	_length(block, bits);
	return(1);
}

/**
 * @brief Read the tag
 *
 * @param[in] gen Generation from init
 * @param[out] tag 32 bytes
 * @return NO when gen is stale or the peripheral stalls
 */
mp_bool_t mp_arch_hmac_fuse_final(uint8_t gen, uint8_t *tag) {
	uint8_t last[64];
	uint64_t bits;
	unsigned int i;
	int one;

	if(!_st.started || gen != _st.gen || !tag)
		return(NO);
	bits = _st.bytes << 3;

	if(!_st.has_held) {
		one = _pad_one(_st.block, _st.used, bits);
		if(one) {
			if(!_block(_st.block, _ONLY))
				goto fail;
		}
		else {
			if(!_block(_st.block, _PAD))
				goto fail;
			memset(last, 0, 64);
			_length(last, bits);
			if(!_block(last, 0))
				goto fail;
		}
	}
	else if(_st.used == 0) {
		if(!_block(_st.held, _END))
			goto fail;
	}
	else {
		one = _pad_one(_st.block, _st.used, bits);
		if(!_block(_st.held, one ? _PAD : _ING))
			goto fail;
		if(one) {
			if(!_block(_st.block, 0))
				goto fail;
		}
		else {
			if(!_block(_st.block, _PAD))
				goto fail;
			memset(last, 0, 64);
			_length(last, bits);
			if(!_block(last, 0))
				goto fail;
		}
	}

	if(!_idle())
		goto fail;
	for(i = 0; i < 8; i++) {
		uint32_t word = (uint32_t)c6_read(C6_HMAC + _OUT + i * 4UL);

		memcpy(tag + i * 4, &word, 4);
	}
	c6_write(C6_HMAC + _DONE, 1UL);
	_wipe();
	return(YES);
fail:
	_clock();
	_wipe();
	return(NO);
}

/** drops a MAC that will not reach final */
void mp_arch_hmac_fuse_drop(uint8_t gen) {
	if(_st.started && gen == _st.gen) {
		_clock();
		_wipe();
	}
}

/**
 * @brief Derive the RSA_DS key from a slot
 *
 * Purpose 7, or 5 when the slot also re-enables JTAG. The result stays
 * in the peripheral for the digital signature block.
 *
 * @param[in] slot KEY index
 * @return NO when the burned purpose is not a DS purpose or the block refuses
 */
mp_bool_t mp_arch_hmac_downstream(unsigned int slot) {
	unsigned int purpose = mp_arch_fuse_purpose(slot);

	if(purpose != 7u && purpose != 5u)
		return(NO);
	if(_st.started) {
		_clock();
		_wipe();
	}
	if(!_arm(slot, purpose) || !_idle()) {
		c6_write(C6_HMAC + _INV_DS, 1UL);
		_clock();
		return(NO);
	}
	return(YES);
}

/**@}*/

#else
	#error HMAC fuse accelerator is implemented for the ESP32-C6
#endif
