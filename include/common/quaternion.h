/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2014  Michael VERGOZ                                      *
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

#ifndef _HAVE_MP_QUATERNION_H
	#define _HAVE_MP_QUATERNION_H

	#ifdef SUPPORT_COMMON_QUATERNION
		typedef struct mp_quaternion_s mp_quaternion_t;
		typedef void (*mp_quaternion_fct_t)(mp_quaternion_t *, float, float, float, float, float, float, float, float, float);

		struct mp_quaternion_s {
			/* function used to compute */
			mp_quaternion_fct_t function;

			/* vector to hold quaternion */
			float q[4];

			/* vector to hold integral error for Mahony method */
			float eInt[3];

			/* mahony */
			float Kp;
			float Ki;
			float beta;
			float zeta;

			/* integration interval for both filter schemes */
			float deltat;

			/* use for last update */
			unsigned long lastUpdate;

			/* frequency */
			unsigned long frequency;
		};

		void mp_quaternion_init(mp_quaternion_t *h, mp_quaternion_fct_t fct);
		void mp_quaternion_fini(mp_quaternion_t *h);
		void mp_quaternion_update(mp_quaternion_t *h, float ax, float ay, float az, float gx, float gy, float gz, float mx, float my, float mz);
		void mp_quaternion_madgwick(mp_quaternion_t *h, float ax, float ay, float az, float gx, float gy, float gz, float mx, float my, float mz);
		void mp_quaternion_mahony(mp_quaternion_t *h, float ax, float ay, float az, float gx, float gy, float gz, float mx, float my, float mz);

	#endif

#endif
