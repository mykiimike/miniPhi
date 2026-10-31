/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2026  Michael VERGOZ                                      *
 * Copyright (C) 2026  VERMAN                                              *
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

#ifdef SUPPORT_COMMON_CRYPTO_RNG_HSM

#ifndef MP_ARCH_HSM
	#error HSM needs an accelerator declared on the series or the MCU
#endif

/**
@defgroup mpCommonHsm HSM

@ingroup mpCommon

@brief Hardware entropy

The series or the MCU declares the block, and that declaration is what
puts this module in the build. One read fills the caller's buffer from
the accelerator. A DRBG seeds from here. mp_rng_bytes() is the draw.

@{
*/

/**
 * @brief Read entropy from the accelerator
 *
 * @param[out] out Buffer of size bytes
 * @param[in] size Number of bytes
 * @return FALSE when size is empty or the accelerator refuses; out is then unchanged
 */
mp_ret_t mp_hsm_entropy(void *out, unsigned int size) {
	if(!out || !size)
		return(FALSE);
	if(mp_arch_hsm_entropy(out, size) != YES) {
		static mp_bool_t said;

		if(said == NO) {
			mp_printk("HSM: accelerator refused the read");
			said = YES;
		}
		return(FALSE);
	}
	return(TRUE);
}

/**@}*/

#endif
