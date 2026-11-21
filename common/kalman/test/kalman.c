/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2026  Michael VERGOZ                                      *
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

/* Host test of the 1D random-walk Kalman update. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>

#define SUPPORT_COMMON_KALMAN
#include <mp.h>
#include "../kalman.h"
#include "../kalman.c"

static int failed;

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "kalman: %s\n", msg);
}

static int near(float got, float exp) {
	float d = got - exp;

	if(d < 0)
		d = -d;
	return(d < 0.00001f);
}

int main(void) {
	mp_kalman_t k;
	mp_kernel_t kernel;
	float nan;

	memset(&kernel, 0, sizeof(kernel));
	expect(mp_kalman_init(&kernel, &k, -1.0f, 1.0f) == FALSE, "Q < 0");
	expect(mp_kalman_init(&kernel, &k, 1.0f, 0.0f) == FALSE, "R = 0");
	expect(mp_kalman_init(&kernel, &k, 1.0f, -1.0f) == FALSE, "R < 0");
	nan = 0.0f;
	nan = nan / nan;
	expect(mp_kalman_init(&kernel, &k, nan, 1.0f) == FALSE, "Q nan");
	expect(mp_kalman_init(&kernel, &k, 1.0f, nan) == FALSE, "R nan");

	expect(mp_kalman_init(&kernel, &k, 1.0f, 1.0f) == TRUE, "init");
	expect(mp_kalman_update(&k, 0.0f) == 0.0f, "first");
	expect(near(k.P, 1.0f), "P = R");
	/* P <- 2, K = 2/3, x <- 4/3, P <- 2/3 */
	expect(near(mp_kalman_update(&k, 2.0f), 4.0f / 3.0f), "second");
	expect(near(k.P, 2.0f / 3.0f), "P after");

	mp_kalman_reset(&k);
	expect(mp_kalman_update(&k, 9.0f) == 9.0f, "reset");
	expect(near(k.P, 1.0f), "P reset");
	mp_kalman_fini(&k);
	return(failed ? 1 : 0);
}
