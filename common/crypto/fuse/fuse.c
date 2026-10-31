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

#ifdef SUPPORT_COMMON_CRYPTO_FUSE

#ifndef MP_ARCH_FUSE
	#error FUSE needs an accelerator declared on the series or the MCU
#endif

/**
@defgroup mpCommonFuse FUSE

@ingroup mpCommon

@brief Named eFuse key slots

The series or the MCU declares the block, and that declaration is what
puts this module in the build. A caller names a slot and a role.
mp_crypto_fuse_check() asks the arch whether the burned purpose is that role.
Nothing here burns a bit or picks a free slot.

@{
*/

/**
 * @brief Accept a slot for one role
 *
 * @param[in] fuse Slot and the role it claims
 * @param[in] purpose MP_CRYPTO_FUSE_HMAC, MP_CRYPTO_FUSE_DS or MP_CRYPTO_FUSE_XTS
 * @return NO when the claim and the burned purpose disagree
 */
mp_bool_t mp_crypto_fuse_check(const mp_crypto_fuse_t *fuse, unsigned int purpose) {
	static mp_bool_t said;

	if(!fuse || fuse->purpose != purpose || mp_arch_fuse_check(fuse->slot, purpose) != YES) {
		if(said == NO) {
			mp_printk("FUSE: the slot does not have this purpose");
			said = YES;
		}
		return(NO);
	}
	return(YES);
}

/**@}*/

#endif
