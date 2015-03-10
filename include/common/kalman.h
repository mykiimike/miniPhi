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
#define SUPPORT_COMMON_KALMAN

#ifdef SUPPORT_COMMON_KALMAN

#ifndef _HAVE_MP_COMMON_KALMAN_H
	#define _HAVE_MP_COMMON_KALMAN_H

	/**
	 * @defgroup mpCommonKalman
	 * @{
	 */

	typedef struct mp_kalman_s mp_kalman_t;

	struct mp_kalman_s {
		/** last estimation */
		float x_est_last;

		/* last covariance estimation error */
		float P_last;

		/** process noise covariance matrix */
		float Q;

		/** the measurement noise covariance matrix */
		float R;

		/** the kalman gain */
		float K;

		/** estimate error covariance matrix */
		float P;

		/** current estimation */
		float x_est;
	};

	/** @} */

	void mp_kalman_init(mp_kernel_t *kernel, mp_kalman_t *kalman, float Q, float R);
	void mp_kalman_fini(mp_kalman_t *kalman);
	float mp_kalman_update(mp_kalman_t *kalman, float x);


#endif
#endif

