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

#include <mp.h>

#ifdef SUPPORT_COMMON_QUATERNION

/**
@defgroup mpCommonQuaternion Quaternion

@ingroup mpCommon

@brief Orientation fusion of an accelerometer, a gyroscope and a magnetometer

The orientation is a unit quaternion q = (w, x, y, z) that turns the
earth frame (z up, magnetic north in the x-z plane) into the sensor
frame. Lying flat and facing north, the sensor reads q = (1, 0, 0, 0).

Two filters, picked at mp_quaternion_init():
- mp_quaternion_madgwick(): gradient descent, gain beta (rad/s, about
  sqrt(3/4) times the gyroscope error, 0.033 to 0.1), and zeta (rad/s/s)
  to track a drifting gyroscope bias.
- mp_quaternion_mahony(): proportional-integral feedback, Kp around 1,
  Ki small or 0.

Units: the gyroscope is in rad/s. The accelerometer and the magnetometer
are normalised, so any unit fits; all three must share the same axes, so
remap a magnetometer whose axes differ from the accelerometer ones (an
MPU9250 for instance) before calling. A magnetometer reading of zero
runs the filter on 6 axes, without yaw correction; a zero or non finite
accelerometer only integrates the gyroscope.

The first update sets q from the accelerometer and the magnetometer
instead of integrating from the boot time. A gap longer than
@ref MP_QUATERNION_DT_MAX only restarts the time base.

Both filters run in float: on a part without FPU, a Madgwick step costs
a few hundred software float operations, milliseconds on an MSP430.

<strong>@ref SUPPORT_COMMON_QUATERNION must be defined to use this module</strong>
@{
*/

/* not NaN nor infinite, without C99 isfinite() */
static mp_bool_t _mp_quaternion_finite(float v) {
	return(v == v && v - v == 0.0f ? YES : NO);
}

/* scales v[0..n-1] to unit norm; NO when its norm is 0 or not finite */
static mp_bool_t _mp_quaternion_normalise(float *v, int n) {
	float norm = 0.0f;
	int i;

	for(i = 0; i < n; i++)
		norm += v[i] * v[i];
	if(!(norm > 0.0f) || _mp_quaternion_finite(norm) == NO)
		return(NO);
	norm = 1.0f / sqrtf(norm);
	for(i = 0; i < n; i++)
		v[i] *= norm;
	return(YES);
}

static float _mp_quaternion_clamp(float v, float bound) {
	if(v > bound)
		return(bound);
	if(v < -bound)
		return(-bound);
	return(v);
}

/* integrates qDot over dt and keeps q a unit quaternion, back to identity when it degenerates */
static void _mp_quaternion_integrate(mp_quaternion_t *h, const float *qDot, float dt) {
	float q[4];
	int i;

	for(i = 0; i < 4; i++)
		q[i] = h->q[i] + qDot[i] * dt;
	if(_mp_quaternion_normalise(q, 4) == NO) {
		mp_quaternion_reset(h);
		return;
	}
	for(i = 0; i < 4; i++)
		h->q[i] = q[i];
}

/* q from the gravity and the field alone */
static void _mp_quaternion_prime(mp_quaternion_t *h, const float *a, const float *m) {
	float roll, pitch, yaw = 0.0f;
	float cr, sr, cp, sp, cy, sy;

	if(!a) {
		h->q[0] = 1.0f;
		h->q[1] = h->q[2] = h->q[3] = 0.0f;
		return;
	}

	roll = atan2f(a[1], a[2]);
	pitch = atan2f(-a[0], sqrtf(a[1] * a[1] + a[2] * a[2]));
	if(m) {
		float cR = cosf(roll), sR = sinf(roll), cP = cosf(pitch), sP = sinf(pitch);
		float mx = m[0] * cP + m[1] * sR * sP + m[2] * cR * sP;
		float my = m[1] * cR - m[2] * sR;

		yaw = atan2f(-my, mx);
	}

	cr = cosf(roll * 0.5f); sr = sinf(roll * 0.5f);
	cp = cosf(pitch * 0.5f); sp = sinf(pitch * 0.5f);
	cy = cosf(yaw * 0.5f); sy = sinf(yaw * 0.5f);
	h->q[0] = cr * cp * cy + sr * sp * sy;
	h->q[1] = sr * cp * cy - cr * sp * sy;
	h->q[2] = cr * sp * cy + sr * cp * sy;
	h->q[3] = cr * cp * sy - sr * sp * cy;
	if(_mp_quaternion_normalise(h->q, 4) == NO)
		mp_quaternion_reset(h);
}

/**
 * @brief Initialise an orientation filter
 *
 * Gains start from MP_QUATERNION_BETA, MP_QUATERNION_ZETA,
 * MP_QUATERNION_KP and MP_QUATERNION_KI.
 *
 * @param[in] kernel Kernel
 * @param[out] h Filter
 * @param[in] fct mp_quaternion_madgwick or mp_quaternion_mahony
 * @return FALSE without filter or without clock
 */
mp_ret_t mp_quaternion_init(mp_kernel_t *kernel, mp_quaternion_t *h, mp_quaternion_fct_t fct) {
	(void)kernel;

	if(!h)
		return(FALSE);
	memset(h, 0, sizeof(*h));
	if(!fct)
		return(FALSE);
	h->function = fct;
	h->beta = MP_QUATERNION_BETA;
	h->zeta = MP_QUATERNION_ZETA;
	h->Kp = MP_QUATERNION_KP;
	h->Ki = MP_QUATERNION_KI;
	h->hz = mp_clock_tick_hz();
	mp_quaternion_reset(h);
	return(TRUE);
}

void mp_quaternion_fini(mp_quaternion_t *h) {
	if(h)
		memset(h, 0, sizeof(*h));
}

/**
 * @brief Forget the orientation; the next update primes it again
 */
void mp_quaternion_reset(mp_quaternion_t *h) {
	h->q[0] = 1.0f;
	h->q[1] = h->q[2] = h->q[3] = 0.0f;
	h->bias[0] = h->bias[1] = h->bias[2] = 0.0f;
	h->primed = NO;
}

/**
 * @param[in] beta Convergence gain, rad/s, >= 0
 * @param[in] zeta Bias drift gain, rad/s/s, >= 0
 * @return FALSE when a gain is negative or not finite
 */
mp_ret_t mp_quaternion_madgwick_gains(mp_quaternion_t *h, float beta, float zeta) {
	if(!h || !(beta >= 0.0f) || !(zeta >= 0.0f) ||
			_mp_quaternion_finite(beta) == NO || _mp_quaternion_finite(zeta) == NO)
		return(FALSE);
	h->beta = beta;
	h->zeta = zeta;
	return(TRUE);
}

/**
 * @param[in] Kp Proportional gain, >= 0
 * @param[in] Ki Integral gain, >= 0; 0 clears the integral
 * @return FALSE when a gain is negative or not finite
 */
mp_ret_t mp_quaternion_mahony_gains(mp_quaternion_t *h, float Kp, float Ki) {
	if(!h || !(Kp >= 0.0f) || !(Ki >= 0.0f) ||
			_mp_quaternion_finite(Kp) == NO || _mp_quaternion_finite(Ki) == NO)
		return(FALSE);
	h->Kp = Kp;
	h->Ki = Ki;
	return(TRUE);
}

/**
 * @brief Run the filter on a sample taken dt seconds after the previous one
 *
 * For a caller that knows its sample period; mp_quaternion_update() reads
 * the clock instead.
 *
 * @return FALSE when the gyroscope or dt is not finite, or dt out of (0, MP_QUATERNION_DT_MAX]
 */
mp_ret_t mp_quaternion_update_dt(mp_quaternion_t *h, float dt, float ax, float ay, float az, float gx, float gy, float gz, float mx, float my, float mz) {
	float a[3], g[3], m[3];
	const float *pa = a, *pm = m;

	if(!h || !h->function)
		return(FALSE);

	g[0] = gx; g[1] = gy; g[2] = gz;
	if(_mp_quaternion_finite(gx) == NO || _mp_quaternion_finite(gy) == NO || _mp_quaternion_finite(gz) == NO)
		return(FALSE);

	a[0] = ax; a[1] = ay; a[2] = az;
	if(_mp_quaternion_normalise(a, 3) == NO)
		pa = NULL;
	m[0] = mx; m[1] = my; m[2] = mz;
	if(!pa || _mp_quaternion_normalise(m, 3) == NO)
		pm = NULL;

	if(h->primed == NO) {
		_mp_quaternion_prime(h, pa, pm);
		h->primed = YES;
		return(TRUE);
	}

	if(!(dt > 0.0f) || !(dt <= MP_QUATERNION_DT_MAX))
		return(FALSE);

	h->function(h, pa, g, pm, dt);
	return(TRUE);
}

/**
 * @brief Run the filter on a sample, timed by the kernel clock
 *
 * @param[in] ax, ay, az Accelerometer, any unit
 * @param[in] gx, gy, gz Gyroscope, rad/s
 * @param[in] mx, my, mz Magnetometer, any unit; all zero for 6 axes
 * @return FALSE when the sample is refused, or after a gap that only restarted the time base
 */
mp_ret_t mp_quaternion_update(mp_quaternion_t *h, float ax, float ay, float az, float gx, float gy, float gz, float mx, float my, float mz) {
	unsigned long now = mp_clock_ticks();
	float dt;

	if(!h || h->hz == 0)
		return(FALSE);

	dt = (float)(now - h->lastUpdate) / (float)h->hz;
	h->lastUpdate = now;
	return(mp_quaternion_update_dt(h, dt, ax, ay, az, gx, gy, gz, mx, my, mz));
}

/**
 * @param[out] q w, x, y, z
 */
void mp_quaternion_get(mp_quaternion_t *h, float q[4]) {
	int i;

	for(i = 0; i < 4; i++)
		q[i] = h->q[i];
}

/**
 * @brief Euler angles, radians, aerospace order (yaw, then pitch, then roll)
 *
 * @param[out] roll Around x, -pi to pi; NULL when not wanted
 * @param[out] pitch Around y, -pi/2 to pi/2
 * @param[out] yaw Around z, -pi to pi, from magnetic north
 */
void mp_quaternion_euler(mp_quaternion_t *h, float *roll, float *pitch, float *yaw) {
	float w = h->q[0], x = h->q[1], y = h->q[2], z = h->q[3];

	if(roll)
		*roll = atan2f(2.0f * (w * x + y * z), 1.0f - 2.0f * (x * x + y * y));
	if(pitch)
		*pitch = asinf(_mp_quaternion_clamp(2.0f * (w * y - z * x), 1.0f));
	if(yaw)
		*yaw = atan2f(2.0f * (w * z + x * y), 1.0f - 2.0f * (y * y + z * z));
}

/*
 * Sebastian Madgwick, "An efficient orientation filter for inertial and
 * inertial/magnetic sensor arrays", 2010.
 */
void mp_quaternion_madgwick(mp_quaternion_t *h, const float *a, float *g, const float *m, float dt) {
	float q1 = h->q[0], q2 = h->q[1], q3 = h->q[2], q4 = h->q[3];
	float s[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	float qDot[4];
	int i;

	if(a && m) {
		float ax = a[0], ay = a[1], az = a[2], mx = m[0], my = m[1], mz = m[2];
		float hx, hy, _2bx, _2bz, _4bx, _4bz;
		float _2q1mx = 2.0f * q1 * mx, _2q1my = 2.0f * q1 * my, _2q1mz = 2.0f * q1 * mz, _2q2mx = 2.0f * q2 * mx;
		float _2q1 = 2.0f * q1, _2q2 = 2.0f * q2, _2q3 = 2.0f * q3, _2q4 = 2.0f * q4;
		float _2q1q3 = 2.0f * q1 * q3, _2q3q4 = 2.0f * q3 * q4;
		float q1q1 = q1 * q1, q1q2 = q1 * q2, q1q3 = q1 * q3, q1q4 = q1 * q4;
		float q2q2 = q2 * q2, q2q3 = q2 * q3, q2q4 = q2 * q4;
		float q3q3 = q3 * q3, q3q4 = q3 * q4, q4q4 = q4 * q4;

		/* earth field direction seen through the current q */
		hx = mx * q1q1 - _2q1my * q4 + _2q1mz * q3 + mx * q2q2 + _2q2 * my * q3 + _2q2 * mz * q4 - mx * q3q3 - mx * q4q4;
		hy = _2q1mx * q4 + my * q1q1 - _2q1mz * q2 + _2q2mx * q3 - my * q2q2 + my * q3q3 + _2q3 * mz * q4 - my * q4q4;
		_2bx = sqrtf(hx * hx + hy * hy);
		_2bz = -_2q1mx * q3 + _2q1my * q2 + mz * q1q1 + _2q2mx * q4 - mz * q2q2 + _2q3 * my * q4 - mz * q3q3 + mz * q4q4;
		_4bx = 2.0f * _2bx;
		_4bz = 2.0f * _2bz;

		s[0] = -_2q3 * (2.0f * q2q4 - _2q1q3 - ax) + _2q2 * (2.0f * q1q2 + _2q3q4 - ay) - _2bz * q3 * (_2bx * (0.5f - q3q3 - q4q4) + _2bz * (q2q4 - q1q3) - mx) + (-_2bx * q4 + _2bz * q2) * (_2bx * (q2q3 - q1q4) + _2bz * (q1q2 + q3q4) - my) + _2bx * q3 * (_2bx * (q1q3 + q2q4) + _2bz * (0.5f - q2q2 - q3q3) - mz);
		s[1] = _2q4 * (2.0f * q2q4 - _2q1q3 - ax) + _2q1 * (2.0f * q1q2 + _2q3q4 - ay) - 4.0f * q2 * (1.0f - 2.0f * q2q2 - 2.0f * q3q3 - az) + _2bz * q4 * (_2bx * (0.5f - q3q3 - q4q4) + _2bz * (q2q4 - q1q3) - mx) + (_2bx * q3 + _2bz * q1) * (_2bx * (q2q3 - q1q4) + _2bz * (q1q2 + q3q4) - my) + (_2bx * q4 - _4bz * q2) * (_2bx * (q1q3 + q2q4) + _2bz * (0.5f - q2q2 - q3q3) - mz);
		s[2] = -_2q1 * (2.0f * q2q4 - _2q1q3 - ax) + _2q4 * (2.0f * q1q2 + _2q3q4 - ay) - 4.0f * q3 * (1.0f - 2.0f * q2q2 - 2.0f * q3q3 - az) + (-_4bx * q3 - _2bz * q1) * (_2bx * (0.5f - q3q3 - q4q4) + _2bz * (q2q4 - q1q3) - mx) + (_2bx * q2 + _2bz * q4) * (_2bx * (q2q3 - q1q4) + _2bz * (q1q2 + q3q4) - my) + (_2bx * q1 - _4bz * q3) * (_2bx * (q1q3 + q2q4) + _2bz * (0.5f - q2q2 - q3q3) - mz);
		s[3] = _2q2 * (2.0f * q2q4 - _2q1q3 - ax) + _2q3 * (2.0f * q1q2 + _2q3q4 - ay) + (-_4bx * q4 + _2bz * q2) * (_2bx * (0.5f - q3q3 - q4q4) + _2bz * (q2q4 - q1q3) - mx) + (-_2bx * q1 + _2bz * q3) * (_2bx * (q2q3 - q1q4) + _2bz * (q1q2 + q3q4) - my) + _2bx * q2 * (_2bx * (q1q3 + q2q4) + _2bz * (0.5f - q2q2 - q3q3) - mz);
	}
	else if(a) {
		float ax = a[0], ay = a[1], az = a[2];
		float _2q1 = 2.0f * q1, _2q2 = 2.0f * q2, _2q3 = 2.0f * q3, _2q4 = 2.0f * q4;
		float _4q1 = 4.0f * q1, _4q2 = 4.0f * q2, _4q3 = 4.0f * q3;
		float _8q2 = 8.0f * q2, _8q3 = 8.0f * q3;
		float q1q1 = q1 * q1, q2q2 = q2 * q2, q3q3 = q3 * q3, q4q4 = q4 * q4;

		s[0] = _4q1 * q3q3 + _2q3 * ax + _4q1 * q2q2 - _2q2 * ay;
		s[1] = _4q2 * q4q4 - _2q4 * ax + 4.0f * q1q1 * q2 - _2q1 * ay - _4q2 + _8q2 * q2q2 + _8q2 * q3q3 + _4q2 * az;
		s[2] = 4.0f * q1q1 * q3 + _2q1 * ax + _4q3 * q4q4 - _2q4 * ay - _4q3 + _8q3 * q2q2 + _8q3 * q3q3 + _4q3 * az;
		s[3] = 4.0f * q2q2 * q4 - _2q2 * ax + 4.0f * q3q3 * q4 - _2q3 * ay;
	}

	/* already on the target the step is 0: no direction to correct */
	if(a && _mp_quaternion_normalise(s, 4) == YES) {
		if(h->zeta > 0.0f) {
			/* gyroscope error seen by the step, Madgwick equations 47 and 48 */
			float e[3];

			e[0] = 2.0f * (q1 * s[1] - q2 * s[0] - q3 * s[3] + q4 * s[2]);
			e[1] = 2.0f * (q1 * s[2] + q2 * s[3] - q3 * s[0] - q4 * s[1]);
			e[2] = 2.0f * (q1 * s[3] - q2 * s[2] + q3 * s[1] - q4 * s[0]);
			for(i = 0; i < 3; i++)
				h->bias[i] = _mp_quaternion_clamp(h->bias[i] + e[i] * dt * h->zeta, MP_QUATERNION_BIAS_MAX);
		}
	}
	else
		s[0] = s[1] = s[2] = s[3] = 0.0f;

	for(i = 0; i < 3; i++)
		g[i] -= h->bias[i];

	qDot[0] = 0.5f * (-q2 * g[0] - q3 * g[1] - q4 * g[2]) - h->beta * s[0];
	qDot[1] = 0.5f * (q1 * g[0] + q3 * g[2] - q4 * g[1]) - h->beta * s[1];
	qDot[2] = 0.5f * (q1 * g[1] - q2 * g[2] + q4 * g[0]) - h->beta * s[2];
	qDot[3] = 0.5f * (q1 * g[2] + q2 * g[1] - q3 * g[0]) - h->beta * s[3];
	_mp_quaternion_integrate(h, qDot, dt);
}

/*
 * Robert Mahony, "Nonlinear complementary filters on the special
 * orthogonal group", 2008: the error between the measured and the
 * estimated directions drives a PI correction of the gyroscope.
 */
void mp_quaternion_mahony(mp_quaternion_t *h, const float *a, float *g, const float *m, float dt) {
	float q1 = h->q[0], q2 = h->q[1], q3 = h->q[2], q4 = h->q[3];
	float e[3] = { 0.0f, 0.0f, 0.0f };
	float qDot[4];
	int i;

	if(a) {
		float q1q1 = q1 * q1, q1q2 = q1 * q2, q1q3 = q1 * q3, q1q4 = q1 * q4;
		float q2q2 = q2 * q2, q2q3 = q2 * q3, q2q4 = q2 * q4;
		float q3q3 = q3 * q3, q3q4 = q3 * q4, q4q4 = q4 * q4;

		/* estimated gravity, crossed with the measured one */
		float vx = 2.0f * (q2q4 - q1q3);
		float vy = 2.0f * (q1q2 + q3q4);
		float vz = q1q1 - q2q2 - q3q3 + q4q4;

		e[0] = a[1] * vz - a[2] * vy;
		e[1] = a[2] * vx - a[0] * vz;
		e[2] = a[0] * vy - a[1] * vx;

		if(m) {
			float mx = m[0], my = m[1], mz = m[2];
			float hx, hy, bx, bz, wx, wy, wz;

			hx = 2.0f * mx * (0.5f - q3q3 - q4q4) + 2.0f * my * (q2q3 - q1q4) + 2.0f * mz * (q2q4 + q1q3);
			hy = 2.0f * mx * (q2q3 + q1q4) + 2.0f * my * (0.5f - q2q2 - q4q4) + 2.0f * mz * (q3q4 - q1q2);
			bx = sqrtf(hx * hx + hy * hy);
			bz = 2.0f * mx * (q2q4 - q1q3) + 2.0f * my * (q3q4 + q1q2) + 2.0f * mz * (0.5f - q2q2 - q3q3);

			wx = 2.0f * bx * (0.5f - q3q3 - q4q4) + 2.0f * bz * (q2q4 - q1q3);
			wy = 2.0f * bx * (q2q3 - q1q4) + 2.0f * bz * (q1q2 + q3q4);
			wz = 2.0f * bx * (q1q3 + q2q4) + 2.0f * bz * (0.5f - q2q2 - q3q3);

			e[0] += my * wz - mz * wy;
			e[1] += mz * wx - mx * wz;
			e[2] += mx * wy - my * wx;
		}
	}

	for(i = 0; i < 3; i++) {
		if(h->Ki > 0.0f)
			h->bias[i] = _mp_quaternion_clamp(h->bias[i] + h->Ki * e[i] * dt, MP_QUATERNION_BIAS_MAX);
		else
			h->bias[i] = 0.0f;
		g[i] += h->Kp * e[i] + h->bias[i];
	}

	qDot[0] = 0.5f * (-q2 * g[0] - q3 * g[1] - q4 * g[2]);
	qDot[1] = 0.5f * (q1 * g[0] + q3 * g[2] - q4 * g[1]);
	qDot[2] = 0.5f * (q1 * g[1] - q2 * g[2] + q4 * g[0]);
	qDot[3] = 0.5f * (q1 * g[2] + q2 * g[1] - q3 * g[0]);
	_mp_quaternion_integrate(h, qDot, dt);
}

/**@}*/

#endif
