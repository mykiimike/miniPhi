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

		/* Madgwick gain, rad/s: sqrt(3/4) times the gyroscope error */
		#ifndef MP_QUATERNION_BETA
			#define MP_QUATERNION_BETA 0.1f
		#endif

		/* Madgwick gyroscope bias drift gain, rad/s/s; 0 leaves the bias alone */
		#ifndef MP_QUATERNION_ZETA
			#define MP_QUATERNION_ZETA 0.0f
		#endif

		/* Mahony proportional and integral gains */
		#ifndef MP_QUATERNION_KP
			#define MP_QUATERNION_KP 1.0f
		#endif
		#ifndef MP_QUATERNION_KI
			#define MP_QUATERNION_KI 0.0f
		#endif

		/* bound of the Mahony integral and of the Madgwick bias, rad/s */
		#ifndef MP_QUATERNION_BIAS_MAX
			#define MP_QUATERNION_BIAS_MAX 0.2f
		#endif

		/* a longer gap between two updates restarts the time base without integrating, seconds */
		#ifndef MP_QUATERNION_DT_MAX
			#define MP_QUATERNION_DT_MAX 0.5f
		#endif

		typedef struct mp_quaternion_s mp_quaternion_t;

		/*
		 * one filter step: a normalised accelerometer or NULL, gyroscope in rad/s,
		 * normalised magnetometer or NULL, dt in seconds
		 */
		typedef void (*mp_quaternion_fct_t)(mp_quaternion_t *h, const float *a, float *g, const float *m, float dt);

		struct mp_quaternion_s {
			mp_quaternion_fct_t function;

			/* orientation, w x y z, unit norm */
			float q[4];

			/* Mahony integral, or Madgwick bias estimate, rad/s */
			float bias[3];

			float Kp;
			float Ki;
			float beta;
			float zeta;

			/* clock ticks of the last update and per second */
			unsigned long lastUpdate;
			unsigned long hz;

			/* NO until the first update set q from the sensors */
			mp_bool_t primed;
		};

		mp_ret_t mp_quaternion_init(mp_kernel_t *kernel, mp_quaternion_t *h, mp_quaternion_fct_t fct);
		void mp_quaternion_fini(mp_quaternion_t *h);
		void mp_quaternion_reset(mp_quaternion_t *h);
		mp_ret_t mp_quaternion_madgwick_gains(mp_quaternion_t *h, float beta, float zeta);
		mp_ret_t mp_quaternion_mahony_gains(mp_quaternion_t *h, float Kp, float Ki);
		mp_ret_t mp_quaternion_update(mp_quaternion_t *h, float ax, float ay, float az, float gx, float gy, float gz, float mx, float my, float mz);
		mp_ret_t mp_quaternion_update_dt(mp_quaternion_t *h, float dt, float ax, float ay, float az, float gx, float gy, float gz, float mx, float my, float mz);
		void mp_quaternion_get(mp_quaternion_t *h, float q[4]);
		void mp_quaternion_euler(mp_quaternion_t *h, float *roll, float *pitch, float *yaw);

		/* filters to give mp_quaternion_init() */
		void mp_quaternion_madgwick(mp_quaternion_t *h, const float *a, float *g, const float *m, float dt);
		void mp_quaternion_mahony(mp_quaternion_t *h, const float *a, float *g, const float *m, float dt);

	#endif

#endif
