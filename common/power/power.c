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

@brief Pick the clock level from the load, like a cpufreq governor

The kernel runs at one of four levels: HIGH, MEDIUM, LOW and SLEEP. The
arch maps each to a clock tree with mp_clock_level(); SLEEP runs the LOW
tree and lets the arch sleep deep (Stop, LPM3) when nothing is due soon.

The kernel loop measures how long it stays awake. Every MP_POWER_WINDOW ms
the busy share of the window is the load:

- 90 % or more goes straight to HIGH,
- MP_POWER_UP % or more goes one level up,
- under MP_POWER_DOWN % for MP_POWER_DOWN_DELAY ms goes one level down,
  when the load the lower clock would see stays under MP_POWER_UP.

A hold is a floor: the level never goes under the highest live hold. A
driver holds while its bus would not survive a lower clock or a deep
sleep, the TUI holds HIGH for a moment after a touch. mp_power_hold() and
mp_power_release() can be called from an interrupt; the hold memory
belongs to the caller and starts zeroed.

@{
*/

static const char *__names[MP_POWER_LEVELS] = { "Sleep", "Low", "Medium", "High" };

/* the next level down must keep this much headroom under MP_POWER_UP */
#define _MARGIN 10

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
 * @brief Authorize the screen to turn fully off
 *
 * Off, the saver stays at the dimmed backlight. That duty holds the reduced
 * supply, so the governor cannot reach Sleep. On, the saver may go dark.
 *
 * @param[in] kernel Kernel handler
 * @param[in] on YES to allow it
 */
void mp_power_hibernate(mp_kernel_t *kernel, mp_bool_t on) {
	kernel->power.hibernate = on ? YES : NO;
}

/**
 * @brief Whether hibernate is authorized
 *
 * @param[in] kernel Kernel handler
 * @return YES when the screen may turn fully off
 */
mp_bool_t mp_power_hibernating(mp_kernel_t *kernel) {
	return(kernel->power.hibernate);
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
 * @return YES at SLEEP with no hold above it
 */
mp_bool_t mp_power_deep(mp_kernel_t *kernel) {
	mp_power_t *power = &kernel->power;

	if(power->level != MP_POWER_SLEEP || power->floor != MP_POWER_SLEEP)
		return(NO);
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
			if(hold->floor > floor)
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

static void _mp_power_govern(mp_power_t *power, unsigned long now) {
	unsigned long span = now - power->windowAt;
	unsigned long load;
	unsigned long below;
	unsigned long here;

	load = power->busy >= span ? 100 : power->busy / ((span + 99) / 100);
	if(load > 100)
		load = 100;
	power->load = (unsigned char)load;
	power->wakeups = power->passes * 1000UL / (span / 1000UL ? span / 1000UL : 1);
	power->projected = (unsigned char)load;

	if(power->fixed != MP_POWER_AUTO)
		power->governed = power->fixed;
	else if(load >= 90) {
		power->governed = MP_POWER_HIGH;
		power->calm = 0;
	}
	else if(load >= MP_POWER_UP) {
		if(power->governed < MP_POWER_HIGH)
			power->governed++;
		power->calm = 0;
	}
	else if(load < MP_POWER_DOWN) {
		power->calm += span / 1000UL;
		if(power->governed > MP_POWER_SLEEP) {
			here = mp_clock_level_hz(power->governed) / 1000UL;
			below = mp_clock_level_hz(power->governed - 1) / 1000UL;
			if(below)
				power->projected = (unsigned char)(load * here / below > 100 ? 100 : load * here / below);
			if(power->calm >= MP_POWER_DOWN_DELAY && power->projected + _MARGIN < MP_POWER_UP) {
				power->governed--;
				power->calm = 0;
			}
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
	unsigned long expiry;

	power->busy += now - power->wokeAt;
	power->passes++;
	_mp_power_account(power, now);

	if(now - power->windowAt >= MP_POWER_WINDOW * 1000UL)
		_mp_power_govern(power, now);

	expiry = _mp_power_floor(power);

	want = power->governed > power->floor ? power->governed : power->floor;
	/* a bus that is busy refuses, the next pass tries again */
	if(want != power->level && mp_clock_level(kernel, want) == TRUE) {
		power->level = want;
		power->switches++;
	}

	mp_clock_schedule(kernel, expiry);
	power->wokeAt = mp_clock_us();
}

/**@}*/
