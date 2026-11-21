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

#ifdef SUPPORT_COMMON_CRYPTO_LOCK

/**
@defgroup mpCommonCryptoLock Crypto lock

@ingroup mpCommon

@brief One asymmetric operation at a time

The scratch of bignum and of the signature and KEM modules is shared.
This admits one caller. It is recursive. An interrupt must not take it:
the lock spins, and it raises the task ceiling.

@{
*/

#include "lock.h"

#if MP_CPUS > 1
static mp_cpu_mutex_t _mp_crypto_mutex = MP_CPU_MUTEX_INIT;

mp_crypto_lock_t mp_crypto_lock(void) {
	return(mp_task_mutex_lock(&_mp_crypto_mutex));
}

void mp_crypto_unlock(mp_crypto_lock_t key) {
	mp_task_mutex_unlock(&_mp_crypto_mutex, key);
}
#else
mp_crypto_lock_t mp_crypto_lock(void) {
	return(mp_task_lock(MP_TASK_CEILING_KERNEL));
}

void mp_crypto_unlock(mp_crypto_lock_t key) {
	mp_task_unlock(key);
}
#endif

/**@}*/

#endif
