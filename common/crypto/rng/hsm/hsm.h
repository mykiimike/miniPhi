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
#ifdef SUPPORT_COMMON_CRYPTO_RNG_HSM

#ifndef _HAVE_MP_COMMON_HSM_H
	#define _HAVE_MP_COMMON_HSM_H

	/**
	 * @defgroup mpCommonHsm
	 * @{
	 */

	/** @} */

	/**
	 * Fills out with size bytes from the accelerator.
	 * FALSE when size is empty or the accelerator refuses; out is then unchanged.
	 * This seeds a DRBG. Application random bytes come from mp_rng_bytes().
	 */
	mp_ret_t mp_hsm_entropy(void *out, unsigned int size);

	/** the series accelerator; NO when the block refuses the read */
	mp_bool_t mp_arch_hsm_entropy(void *out, unsigned int size);

#endif
#endif
