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

#include <mp.h>

/**
@defgroup mpCommonPower Power levels and governor

@ingroup mpCommon

@brief Pick the clock level from the demand the kernel measures

The kernel runs at one of four levels: HIGH, MEDIUM, LOW and SLEEP. The
arch maps each to a clock tree with mp_clock_level(). SLEEP is the slow
tree and the only level that may sleep deep (Stop, LPM3).

Every MP_POWER_WINDOW ms the busy share of the loop is smoothed into an
EWMA. The frequency that share needs, so a load of MP_POWER_TARGET percent
stays on the clock that just ran, picks the slowest level that still
reaches it. The level moves up at once and down, straight to that target,
only after it has stayed below the current level for MP_POWER_DOWN_DELAY
ms. A move up clears that wait.

A hold is a frequency floor or a veto of deep sleep. mp_power_hold() and
mp_power_nodeep() can be called from an interrupt; the hold memory belongs
to the caller and starts zeroed. mp_power_deep() is YES only at SLEEP, with
no floor above it and no veto.

@{
*/

static const char *__names[MP_POWER_LEVELS] = { "Sleep", "Low", "Medium", "High" };

void mp_power_init(mp_kernel_t *kernel) {
	mp_power_t *power = &kernel->power;
	unsigned long now;

	memset(power, 0, sizeof(*power));
	power->kernel = kernel;

	/* mp_machine_init() boots the HIGH tree */
	power->level = MP_POWER_HIGH;
	power->governed = MP_POWER_HIGH;
#ifdef MP_POWER_PINNED
	power->fixed = MP_POWER_HIGH;
#else
	power->fixed = MP_POWER_AUTO;
#endif
	power->floor = MP_POWER_SLEEP;

	now = mp_clock_us();
	power->windowAt = now;
	power->wokeAt = now;
	power->accountAt = now;
}

static void _mp_power_attach(mp_power_t *power, mp_power_hold_t *hold, mp_power_level_t floor, char *who) {
	hold->floor = floor;
	hold->who = who;
	if(hold->held == NO) {
		hold->item.next = NULL;
		hold->item.prev = NULL;
		mp_list_add_last(&power->holds, &hold->item, hold);
		hold->held = YES;
	}
	if(floor > power->floor)
		power->floor = floor;
}

/**
 * @brief Keep the level at floor or above until released
 *
 * Holding again moves the floor. Interrupt safe.
 *
 * @param[in] kernel Kernel handler
 * @param[in] hold Caller memory, zeroed before the first use
 * @param[in] floor Lowest level allowed
 * @param[in] who Shown on the Power page
 */
void mp_power_hold(mp_kernel_t *kernel, mp_power_hold_t *hold, mp_power_level_t floor, char *who) {
	MP_INTERRUPT_SAFE_BEGIN
	hold->expires = NO;
	hold->nodeep = NO;
	_mp_power_attach(&kernel->power, hold, floor, who);
	MP_INTERRUPT_SAFE_END
}

/**
 * @brief Hold for ms, the hold lets go by itself
 *
 * Holding again restarts the delay. Interrupt safe.
 *
 * @param[in] kernel Kernel handler
 * @param[in] hold Caller memory, zeroed before the first use
 * @param[in] floor Lowest level allowed
 * @param[in] ms How long
 * @param[in] who Shown on the Power page
 */
void mp_power_hold_for(mp_kernel_t *kernel, mp_power_hold_t *hold, mp_power_level_t floor, unsigned long ms, char *who) {
	mp_power_t *power = &kernel->power;
	unsigned long until = mp_clock_ticks() + ms * mp_clock_tick_hz() / 1000UL;

	MP_INTERRUPT_SAFE_BEGIN
	hold->expires = YES;
	hold->nodeep = NO;
	hold->until = until;
	_mp_power_attach(power, hold, floor, who);
	MP_INTERRUPT_SAFE_END
}

/**
 * @brief Let go of a hold
 *
 * Releasing a hold that is not held does nothing. Interrupt safe.
 *
 * @param[in] kernel Kernel handler
 * @param[in] hold The hold
 */
void mp_power_release(mp_kernel_t *kernel, mp_power_hold_t *hold) {
	MP_INTERRUPT_SAFE_BEGIN
	if(hold->held == YES) {
		mp_list_remove(&kernel->power.holds, &hold->item);
		hold->held = NO;
	}
	MP_INTERRUPT_SAFE_END
}

/**
 * @brief Pin the level, or give it back to the governor
 *
 * Holds still apply above a pinned level.
 *
 * @param[in] kernel Kernel handler
 * @param[in] level A level, or MP_POWER_AUTO
 */
void mp_power_fix(mp_kernel_t *kernel, mp_power_level_t level) {
	mp_power_t *power = &kernel->power;

	power->fixed = level;
	if(level != MP_POWER_AUTO)
		power->governed = level;
	power->calm = 0;
}

/**
 * @brief Veto deep sleep until released
 *
 * The frequency stays free. Holding again keeps the veto. Interrupt safe.
 *
 * @param[in] kernel Kernel handler
 * @param[in] hold Caller memory, zeroed before the first use
 * @param[in] who Shown on the Power page
 */
void mp_power_nodeep(mp_kernel_t *kernel, mp_power_hold_t *hold, char *who) {
	MP_INTERRUPT_SAFE_BEGIN
	hold->expires = NO;
	hold->nodeep = YES;
	_mp_power_attach(&kernel->power, hold, MP_POWER_SLEEP, who);
	MP_INTERRUPT_SAFE_END
}

/**
 * @brief Level the clock runs at
 *
 * @param[in] kernel Kernel handler
 * @return The level
 */
mp_power_level_t mp_power_level(mp_kernel_t *kernel) {
	return(kernel->power.level);
}

/**
 * @brief Whether the arch may sleep deep
 *
 * The arch calls it with interrupts masked, so a hold put by an
 * interrupt just before is seen.
 *
 * @param[in] kernel Kernel handler
 * @return YES at SLEEP, with floor SLEEP and no deep-sleep veto
 */
mp_bool_t mp_power_deep(mp_kernel_t *kernel) {
	mp_power_t *power = &kernel->power;
	mp_list_item_t *item;

	if(power->level != MP_POWER_SLEEP || power->floor != MP_POWER_SLEEP)
		return(NO);
	for(item = power->holds.first; item; item = item->next) {
		mp_power_hold_t *hold = item->user;

		if(hold->nodeep == YES)
			return(NO);
	}
	return(YES);
}

const char *mp_power_name(mp_power_level_t level) {
	if((unsigned int)level >= MP_POWER_LEVELS)
		return("Auto");
	return(__names[level]);
}

/* drops the expired holds, returns the ticks until the next one expires */
static unsigned long _mp_power_floor(mp_power_t *power) {
	mp_power_level_t floor = MP_POWER_SLEEP;
	unsigned long next = MP_TASK_FOREVER;
	unsigned long now = mp_clock_ticks();
	mp_list_item_t *item;
	mp_list_item_t *following;
	mp_power_hold_t *hold;
	long left;

	MP_INTERRUPT_SAFE_BEGIN
	item = power->holds.first;
	while(item) {
		following = item->next;
		hold = item->user;
		left = (long)(hold->until - now);
		if(hold->expires == YES && left <= 0) {
			mp_list_remove(&power->holds, &hold->item);
			hold->held = NO;
		}
		else {
			if(hold->nodeep == NO && hold->floor > floor)
				floor = hold->floor;
			if(hold->expires == YES && (unsigned long)left < next)
				next = (unsigned long)left;
		}
		item = following;
	}
	power->floor = floor;
	MP_INTERRUPT_SAFE_END

	return(next);
}

static void _mp_power_account(mp_power_t *power, unsigned long now) {
	power->levelUs += now - power->accountAt;
	power->accountAt = now;
	if(power->levelUs >= 1000UL) {
		power->levelMs[power->level] += power->levelUs / 1000UL;
		power->levelUs %= 1000UL;
	}
}

/* lowest level that reaches need Hz; equal clocks stay on the lowest */
static mp_power_level_t _mp_power_fit(unsigned long long need) {
	unsigned int index;

	for(index = 0; index < MP_POWER_LEVELS; index++) {
		if((unsigned long long)mp_clock_level_hz((mp_power_level_t)index) >= need)
			return((mp_power_level_t)index);
	}
	return(MP_POWER_HIGH);
}

static void _mp_power_govern(mp_power_t *power, unsigned long now) {
	unsigned long span = now - power->windowAt;
	unsigned long sample;
	unsigned long util;
	unsigned long hz;
	unsigned long long need;
	unsigned int targetPct = MP_POWER_TARGET ? MP_POWER_TARGET : 1;
	mp_power_level_t target;

	sample = power->busy >= span ? 100 : power->busy / ((span + 99) / 100);
	if(sample > 100)
		sample = 100;
	util = ((unsigned long)power->load + sample) / 2;
	if(util > 100)
		util = 100;
	power->load = (unsigned char)util;
	power->wakeups = power->passes * 1000UL / (span / 1000UL ? span / 1000UL : 1);

	hz = mp_clock_level_hz(power->level);
	need = (unsigned long long)hz * util / targetPct;
	power->need = (unsigned long)(need / 1000ULL);
	target = _mp_power_fit(need);
	power->target = target;

	if(power->fixed != MP_POWER_AUTO)
		power->governed = power->fixed;
	else if(target > power->governed) {
		power->governed = target;
		power->calm = 0;
	}
	else if(target < power->governed) {
		power->calm += span / 1000UL;
		if(power->calm >= MP_POWER_DOWN_DELAY) {
			power->governed = target;
			power->calm = 0;
		}
	}
	else
		power->calm = 0;

	power->windowAt = now;
	power->busy = 0;
	power->passes = 0;
}

/**
 * @brief Govern, then sleep until the next task is due
 *
 * The kernel loop calls it after each pass over the tasks.
 *
 * @param[in] kernel Kernel handler
 */
void mp_power_schedule(mp_kernel_t *kernel) {
	mp_power_t *power = &kernel->power;
	mp_power_level_t want;
	unsigned long now = mp_clock_us();
	unsigned long stretch = now - power->wokeAt;
	unsigned long expiry;

	power->busy += stretch;
	power->passes++;
	_mp_power_account(power, now);

	if(now - power->windowAt >= MP_POWER_WINDOW * 1000UL)
		_mp_power_govern(power, now);

	expiry = _mp_power_floor(power);

	want = power->governed > power->floor ? power->governed : power->floor;
	/* a bus that is busy refuses, the next pass tries again */
	if(want != power->level) {
		if(mp_clock_level(kernel, want) == TRUE) {
			power->level = want;
			power->switches++;
		}
		else {
			power->refused++;
			power->blocker = mp_clock_blocker();
		}
	}

	mp_clock_schedule(kernel, expiry);
	power->wokeAt = mp_clock_us();
}

/**@}*/
