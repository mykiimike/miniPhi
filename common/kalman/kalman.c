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

@brief One-dimensional Kalman filter

@version 1.1.0

@author @htmlonly &copy; @endhtmlonly 2015
Michael Vergoz <mv@verman.fr>

@date 10 Mar 2015

Kalman filtering, also known as linear quadratic estimation (LQE),
is an algorithm that uses a series of measurements observed over time,
containing noise (random variations) and other inaccuracies, and produces
estimates of unknown variables that tend to be more precise than those
based on a single measurement alone.

The model is a random walk: the value is taken as constant between two
measurements, give or take Q. It suits a value that moves slowly next
to its noise. A small Q over R smooths more and lags more behind a
change, a big one follows the measurements closely.

The first measurement is taken as the estimation, with the measurement
noise as its error.

Single precision only: neither the MSP430 nor the Cortex-M3 has an FPU.

@{
*/

/**
 * @brief Initiate Kalman filtering context
 *
 * @param[in] kernel Kernel handler
 * @param[in] kalman Kalman context
 * @param[in] Q Process noise variance, 0 or more
 * @param[in] R Measurement noise variance, more than 0
 * @return FALSE when Q or R is out of range
 */
mp_ret_t mp_kalman_init(mp_kernel_t *kernel, mp_kalman_t *kalman, float Q, float R) {
	(void)kernel;

	memset(kalman, 0, sizeof(*kalman));

	/* written so NaN fails too */
	if(!(Q >= 0.0f) || !(R > 0.0f))
		return(FALSE);

	kalman->Q = Q;
	kalman->R = R;
	kalman->primed = NO;

	return(TRUE);
}

/**
 * @brief Terminate Kalman filtering context
 *
 * Nothing is held: kept for the symmetry of the module API.
 *
 * @param[in] kalman Kalman context
 */
void mp_kalman_fini(mp_kalman_t *kalman) {
	(void)kalman;
}

/**
 * @brief Forget the estimation, the next measurement starts over
 *
 * @param[in] kalman Kalman context
 */
void mp_kalman_reset(mp_kalman_t *kalman) {
	kalman->primed = NO;
}

/**
 * @brief Update Kalman filtering context
 *
 * @param[in] kalman Kalman context
 * @param[in] x Measurement
 * @return New estimation
 */
float mp_kalman_update(mp_kalman_t *kalman, float x) {
	float K;

	if(kalman->primed == NO) {
		kalman->x = x;
		kalman->P = kalman->R;
		kalman->primed = YES;
		return(kalman->x);
	}

	/* predict: the value may have moved by Q */
	kalman->P += kalman->Q;

	/* correct: R > 0 keeps the division away from 0 */
	K = kalman->P / (kalman->P + kalman->R);
	kalman->x += K * (x - kalman->x);
	kalman->P *= 1.0f - K;

	return(kalman->x);
}

/**@}*/

#endif
