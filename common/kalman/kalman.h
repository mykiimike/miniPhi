/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2015  Michael VERGOZ                                      *
 * Copyright (C) 2015  VERMAN                                              *
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
#ifdef SUPPORT_COMMON_KALMAN

#ifndef _HAVE_MP_COMMON_KALMAN_H
	#define _HAVE_MP_COMMON_KALMAN_H

	/**
	 * @defgroup mpCommonKalman
	 * @{
	 */

	typedef struct mp_kalman_s mp_kalman_t;

	struct mp_kalman_s {
		/** current estimation */
		float x;

		/** variance of the estimation error */
		float P;

		/** process noise variance: how much the value may move between two measurements */
		float Q;

		/** measurement noise variance */
		float R;

		/** NO until the first measurement */
		mp_bool_t primed;
	};

	/** @} */

	mp_ret_t mp_kalman_init(mp_kernel_t *kernel, mp_kalman_t *kalman, float Q, float R);
	void mp_kalman_fini(mp_kalman_t *kalman);
	void mp_kalman_reset(mp_kalman_t *kalman);
	float mp_kalman_update(mp_kalman_t *kalman, float x);


#endif
#endif
