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

#include <mp.h>

#ifdef SUPPORT_COMMON_KALMAN

/**
@defgroup mpCommonKalman Simple Kalman filter

@ingroup mpCommon

@brief Simple Kalman filter computation

@version 1.0.0

@author @htmlonly &copy; @endhtmlonly 2015
Michael Vergoz <mv@verman.fr>

@date 10 Mar 2015

Kalman filtering, also known as linear quadratic estimation (LQE),
is an algorithm that uses a series of measurements observed over time,
containing noise (random variations) and other inaccuracies, and produces
estimates of unknown variables that tend to be more precise than those
based on a single measurement alone.

This version of kalman filtering is fast and easy to use.

@{
*/

/**
 * @brief Initiate Kalman filtering context
 *
 * This initiates a Kalman filtering context.
 * Q and R are noise related and these values are know.
 *
 * @param[in] kernel Kernel handler
 * @param[in] kalman Kalman context
 * @param[in] Q The process noise covariance matrix
 * @param[in] R The measurement noise covariance matrix
 */
void mp_kalman_init(mp_kernel_t *kernel, mp_kalman_t *kalman, float Q, float R) {
	memset(kalman, 0, sizeof(*kalman));
	kalman->Q = Q;
	kalman->R = R;
}

/**
 * @brief Terminate Kalman filtering context
 *
 * This terminates a Kalman filtering context.
 *
 * @param[in] kalman Kalman context
 */
void mp_kalman_fini(mp_kalman_t *kalman) {
	// none
}

/**
 * @brief Update Kalman filtering context
 *
 * This updates a Kalman filtering using the value @ref x
 *
 * @param[in] kalman Kalman context
 * @param[in] x Measurement
 * @return New Kalman number
 */
float mp_kalman_update(mp_kalman_t *kalman, float x) {
	float P_temp;
	float x_temp_est;

    x_temp_est = kalman->x_est_last;
    P_temp = kalman->P_last + kalman->Q;

    // calculate the Kalman gain
    kalman->K = P_temp * (1.0/(P_temp + kalman->R));

    // correct
    kalman->x_est = x_temp_est + kalman->K * (x - x_temp_est);
    kalman->P = (1- kalman->K) * P_temp;

    return(kalman->x_est);
}

/**@}*/

#endif
