/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2026  Michael VERGOZ                                      *
 * Copyright (C) 2026  VERMAN                                              *
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

#ifdef SUPPORT_COMMON_CRYPTO_RNG

/**
@defgroup mpCommonRng RNG

@ingroup mpCommon

@brief The running generator

One task, started with the kernel. Every MP_RNG_PERIOD milliseconds it
mixes a fresh noise sample into HMAC-DRBG. The noise is the low byte of
the die temperature and of the internal reference, on an arch that has
those channels. SHA-256 is the hash when the build carries it.

mp_rng_bytes() draws from that generator. It stays FALSE until the first
sample has seeded it, so a caller never receives bytes mixed from the
clock alone.

@{
*/

static mp_task_t *_mp_rng_task;
static mp_hmac_drbg_t _mp_rng_drbg;
static mp_bool_t _mp_rng_ready;
static uint32_t _mp_rng_stir;

static unsigned long _mp_rng_ticks(unsigned long ms) {
	unsigned long hz = mp_clock_tick_hz();
	unsigned long ticks;

	if(hz == 0 || ms == 0)
		return(1);
	ticks = (ms / 1000) * hz + ((ms % 1000) * hz) / 1000;
	return(ticks ? ticks : 1);
}

static const char *_mp_rng_hash(void) {
	const mp_hash_algo_t *algo;

	if(mp_hash_find("SHA256"))
		return("SHA256");
	algo = mp_hash_at(0);
	return(algo ? algo->name : NULL);
}

/* low bytes of the die sensors; 0 when the converter is down or busy */
static unsigned int _mp_rng_noise(uint8_t *out, unsigned int cap) {
	unsigned int n = 0;

#ifdef MP_ADC_CHANNEL_TEMP
	int i, sample;

	for(i = 0; i < 8 && n < cap; i++) {
		sample = mp_adc_convert(i & 1 ? MP_ADC_CHANNEL_VREF : MP_ADC_CHANNEL_TEMP);
		if(sample < 0)
			continue;
		out[n++] = (uint8_t)sample;
	}
#else
	(void)out;
	(void)cap;
#endif
	return(n);
}

static void _mp_rng_reseed(void) {
	uint8_t noise[32];
	uint8_t nonce[4];
	unsigned long ticks = mp_clock_ticks();
	unsigned int n;
	const char *hash;

	n = _mp_rng_noise(noise, sizeof(noise) - 8);
	if(n == 0)
		return;

	noise[n++] = (uint8_t)ticks;
	noise[n++] = (uint8_t)(ticks >> 8);
	noise[n++] = (uint8_t)(ticks >> 16);
	noise[n++] = (uint8_t)(ticks >> 24);
	noise[n++] = (uint8_t)_mp_rng_stir;
	noise[n++] = (uint8_t)(_mp_rng_stir >> 8);
	noise[n++] = (uint8_t)(_mp_rng_stir >> 16);
	noise[n++] = (uint8_t)(_mp_rng_stir >> 24);
	_mp_rng_stir++;

	if(_mp_rng_ready == YES) {
		mp_hmac_drbg_reseed(&_mp_rng_drbg, noise, n, NULL, 0);
		memset(noise, 0, sizeof(noise));
		return;
	}

	hash = _mp_rng_hash();
	nonce[0] = (uint8_t)ticks;
	nonce[1] = (uint8_t)(ticks >> 8);
	nonce[2] = (uint8_t)(ticks >> 16);
	nonce[3] = (uint8_t)(ticks >> 24);
	if(hash && mp_hmac_drbg_init(&_mp_rng_drbg, hash, noise, n, nonce, 4, NULL, 0) == TRUE)
		_mp_rng_ready = YES;
	memset(noise, 0, sizeof(noise));
	memset(nonce, 0, sizeof(nonce));
}

static MP_TASK(_mp_rng_wake) {
	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}
	_mp_rng_reseed();
}

/**
 * @brief Start the generator task
 *
 * Called from the kernel after the task pool exists. Safe to call twice.
 *
 * @param[in] kernel Kernel
 */
void mp_rng_start(mp_kernel_t *kernel) {
	if(_mp_rng_task || !kernel)
		return;
	_mp_rng_ready = NO;
	_mp_rng_stir = 0;
	memset(&_mp_rng_drbg, 0, sizeof(_mp_rng_drbg));
	_mp_rng_task = mp_task_create(&kernel->tasks, "RNG", _mp_rng_wake, NULL, _mp_rng_ticks(MP_RNG_PERIOD));
	if(!_mp_rng_task)
		mp_printk("RNG: no task slot");
}

/**
 * @brief Stop the generator
 *
 * Wipes the DRBG. The task dies on the next tick of the pool.
 */
void mp_rng_stop(void) {
	if(_mp_rng_task) {
		mp_task_destroy(_mp_rng_task);
		_mp_rng_task = NULL;
	}
	mp_hmac_drbg_free(&_mp_rng_drbg);
	_mp_rng_ready = NO;
}

/**
 * @brief Draw random bytes
 *
 * @param[out] out Buffer
 * @param[in] size 1 to MP_HMAC_DRBG_MAX
 * @return FALSE until the task has seeded, or when size is empty
 */
mp_ret_t mp_rng_bytes(void *out, unsigned int size) {
	if(_mp_rng_ready != YES || !out || !size)
		return(FALSE);
	return(mp_hmac_drbg_generate(&_mp_rng_drbg, out, size, NULL, 0));
}

/**
 * @brief Name of the generator
 */
const char *mp_rng_generator(void) {
	return("HMAC-DRBG");
}

/**
 * @brief Label of the hash under the generator
 *
 * @return "SHA-256", or NULL when the build carries no hash
 */
const char *mp_rng_hash(void) {
	const mp_hash_algo_t *algo = mp_hash_find(_mp_rng_hash());

	return(algo ? algo->label : NULL);
}

/**@}*/

#endif
