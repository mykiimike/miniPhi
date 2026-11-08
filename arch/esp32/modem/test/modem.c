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

/* Host test of the modem refcount. Not linked into the firmware. */

#include <stdio.h>

static int failed;
static int ons;
static int offs;

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

void mp_arch_modem_on(void) {
	ons++;
}

void mp_arch_modem_off(void) {
	offs++;
}

#include "../modem.c"

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "fail: %s\n", msg);
}

int main(void) {
	expect(mp_arch_modem_get(MP_ARCH_MODEM_COUNT) == FALSE, "bad user");
	expect(ons == 0 && mp_arch_modem_users() == 0, "still down");

	expect(mp_arch_modem_get(MP_ARCH_MODEM_PHY) == TRUE, "phy");
	expect(ons == 1 && offs == 0 && mp_arch_modem_users() == 1, "first get powers on");

	expect(mp_arch_modem_get(MP_ARCH_MODEM_PHY) == TRUE, "phy again");
	expect(ons == 1 && mp_arch_modem_users() == 1, "same user is one hold");

	expect(mp_arch_modem_get(MP_ARCH_MODEM_WIFI) == TRUE, "wifi");
	expect(ons == 1 && mp_arch_modem_users() == 2, "second user does not power again");

	mp_arch_modem_put(MP_ARCH_MODEM_PHY);
	expect(offs == 0 && mp_arch_modem_users() == 1, "wifi still holds");

	mp_arch_modem_put(MP_ARCH_MODEM_PHY);
	expect(offs == 0 && mp_arch_modem_users() == 1, "put without a hold");

	mp_arch_modem_put(MP_ARCH_MODEM_WIFI);
	expect(offs == 1 && mp_arch_modem_users() == 0, "last put powers off");

	expect(mp_arch_modem_get(MP_ARCH_MODEM_IEEE154) == TRUE, "again");
	mp_arch_modem_put(MP_ARCH_MODEM_IEEE154);
	expect(ons == 2 && offs == 2, "a later user powers on and off");

	if(failed) {
		fprintf(stderr, "%d failed\n", failed);
		return(1);
	}
	return(0);
}
