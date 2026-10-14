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
#ifdef SUPPORT_COMMON_CRYPTO_RNG

#ifndef _HAVE_MP_COMMON_RNG_H
	#define _HAVE_MP_COMMON_RNG_H

	/**
	 * @defgroup mpCommonRng
	 * @{
	 */

	/** @} */

	/** one task, reseeding every MP_RNG_PERIOD ms; called from the kernel */
	void mp_rng_start(mp_kernel_t *kernel);
	void mp_rng_stop(void);
	/**
	 * Draws size bytes, at most MP_HMAC_DRBG_MAX.
	 * FALSE until the task has seeded from a noise sample, or when size is empty.
	 */
	mp_ret_t mp_rng_bytes(void *out, unsigned int size);
	/** the generator the task reseeds, "HMAC-DRBG" */
	const char *mp_rng_generator(void);
	/** the label of the hash under it, "SHA-256"; NULL when the build carries none */
	const char *mp_rng_hash(void);

#endif
#endif
