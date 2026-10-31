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

/**
@defgroup mpArchESP32Fuse FUSE

@ingroup mpArchESP32

KEY0 and KEY1 purposes sit in EFUSE_RD_REPEAT_DATA1. KEY2 to KEY5 sit
in EFUSE_RD_REPEAT_DATA2. The role values are the ones the C6 burns:
8 HMAC upstream, 7 RSA_DS, 5 HMAC downstream for JTAG and RSA_DS,
4 XTS-AES. KEY5 cannot hold the XTS key.

@{
*/

#define _DATA1 0x34UL
#define _DATA2 0x38UL

#define _XTS 4u
#define _DS_JTAG 5u
#define _DS 7u
#define _HMAC 8u

static unsigned int _purpose(unsigned int slot) {
	unsigned long word;

	if(slot < 2) {
		word = c6_read(C6_EFUSE + _DATA1);
		return((unsigned int)(word >> (24 + slot * 4)) & 0xfu);
	}
	word = c6_read(C6_EFUSE + _DATA2);
	return((unsigned int)(word >> ((slot - 2) * 4)) & 0xfu);
}

/**
 * @brief Read the burned purpose of a slot
 *
 * @param[in] slot KEY index, 0 to 5
 * @param[in] purpose MP_CRYPTO_FUSE_HMAC, MP_CRYPTO_FUSE_DS or MP_CRYPTO_FUSE_XTS
 * @return YES when the burned value is the role purpose names
 */
mp_bool_t mp_arch_fuse_check(unsigned int slot, unsigned int purpose) {
	unsigned int burned;

	if(slot > 5)
		return(NO);
	burned = _purpose(slot);
	if(purpose == MP_CRYPTO_FUSE_HMAC && burned == _HMAC)
		return(YES);
	if(purpose == MP_CRYPTO_FUSE_DS && (burned == _DS || burned == _DS_JTAG))
		return(YES);
	if(purpose == MP_CRYPTO_FUSE_XTS && burned == _XTS && slot < 5)
		return(YES);
	/* Ed25519, ECDSA P-256, LMS, SLH-DSA and ML-DSA have roles. This chip burns none of them. */
	return(NO);
}

/** the burned purpose, or 0 when slot is out of range. Downstream HMAC writes this value. */
unsigned int mp_arch_fuse_purpose(unsigned int slot) {
	if(slot > 5)
		return(0);
	return(_purpose(slot));
}

/**@}*/

#else
	#error FUSE accelerator is implemented for the ESP32-C6
#endif
