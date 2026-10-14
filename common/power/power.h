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

#ifndef _HAVE_MP_POWER_H
	#define _HAVE_MP_POWER_H

	/**
	 * @defgroup mpCommonPower
	 * @{
	 */

	#ifndef MP_POWER_WINDOW
		#define MP_POWER_WINDOW 100
	#endif

	#ifndef MP_POWER_UP
		#define MP_POWER_UP 70
	#endif

	#ifndef MP_POWER_DOWN
		#define MP_POWER_DOWN 20
	#endif

	#ifndef MP_POWER_DOWN_DELAY
		#define MP_POWER_DOWN_DELAY 500
	#endif

	#ifndef MP_POWER_STOP_MIN
		#define MP_POWER_STOP_MIN 30
	#endif

	/* ordered: a higher level never consumes less */
	typedef enum {
		MP_POWER_SLEEP,
		MP_POWER_LOW,
		MP_POWER_MEDIUM,
		MP_POWER_HIGH,
		MP_POWER_LEVELS,
	} mp_power_level_t;

	/* mp_power_fix() with it gives the level back to the governor */
	#define MP_POWER_AUTO MP_POWER_LEVELS

	typedef struct mp_power_hold_s mp_power_hold_t;
	typedef struct mp_power_s mp_power_t;

	/** A floor put by a user: the level does not go under it while it is held */
	struct mp_power_hold_s {
		mp_power_level_t floor;

		/** tick after which mp_power_hold_for() lets go, when expires is YES */
		unsigned long until;
		mp_bool_t expires;

		mp_bool_t held;
		char *who;
		mp_list_item_t item;
	};

	struct mp_power_s {
		mp_kernel_t *kernel;

		/** level the clock runs at */
		mp_power_level_t level;

		/** level the load asks for */
		mp_power_level_t governed;

		/** MP_POWER_AUTO, or the level mp_power_fix() pinned */
		mp_power_level_t fixed;

		/** highest floor of the live holds */
		mp_power_level_t floor;

		/** busy share of the last window, in % */
		unsigned char load;

		/** load the next level down would see, in % */
		unsigned char projected;

		/** µs at the window start, busy µs inside it, µs of the last wake up */
		unsigned long windowAt;
		unsigned long busy;
		unsigned long wokeAt;

		/** ms the load stayed under MP_POWER_DOWN */
		unsigned long calm;

		/** wake ups in the last window, per second */
		unsigned long wakeups;
		unsigned long passes;

		/** sleeps the arch took deep (Stop, LPM3) since boot */
		unsigned long deep;

		/** level changes since boot */
		unsigned long switches;

		/** ms spent per level since boot */
		unsigned long levelMs[MP_POWER_LEVELS];
		unsigned long levelUs;
		unsigned long accountAt;

		mp_list_t holds;
	};

	/** @} */

	void mp_power_init(mp_kernel_t *kernel);
	void mp_power_schedule(mp_kernel_t *kernel);

	void mp_power_hold(mp_kernel_t *kernel, mp_power_hold_t *hold, mp_power_level_t floor, char *who);
	void mp_power_hold_for(mp_kernel_t *kernel, mp_power_hold_t *hold, mp_power_level_t floor, unsigned long ms, char *who);
	void mp_power_release(mp_kernel_t *kernel, mp_power_hold_t *hold);

	void mp_power_fix(mp_kernel_t *kernel, mp_power_level_t level);
	mp_power_level_t mp_power_level(mp_kernel_t *kernel);
	mp_bool_t mp_power_deep(mp_kernel_t *kernel);
	const char *mp_power_name(mp_power_level_t level);

#endif
