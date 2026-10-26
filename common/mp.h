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

/**
@defgroup mpCommon miniPhi Base of system

@brief Implementation of features to build your firmware
*/


/**
@defgroup mpDriver miniPhi Drivers

@brief Implementation of different chips on miniPhi
*/

/**
@defgroup mpArch Architecture dependant implementation

@brief Differents processor architecture
*/


#ifndef _MP_H
	#define _MP_H

	#ifndef MP_MY_CONFIG
		#include <config.h>
	#endif

	#define NULL (void *)0

	#define TRUE 1
	#define FALSE 0

	#define true 1
	#define false 0

	#define YES TRUE
	#define NO FALSE

	#define ON TRUE
	#define OFF FALSE

	typedef signed char mp_ret_t;
	typedef signed char mp_bool_t;

	typedef struct mp_kernel_s mp_kernel_t;
	typedef void (*mp_kernel_onBoot_t)(void *user);

	typedef struct mp_options_s mp_options_t;

	struct mp_options_s {
		char *key;
		char *value;
	};

	#include <list.h>
	#include <task/task.h>
	#include <power/power.h>

	#include <math.h>
	#include <string.h>
	#include <stdio.h>
	#include <stdlib.h>
	#include <stdarg.h>

	/* the selected arch, linked as build/include/arch/ */
	#include <arch/internal.h>

	#define PI (355.0 / 113.0)

	/* Every arch implements these. The headers above may declare them too. */
	unsigned long mp_clock_ticks();
	unsigned long mp_clock_tick_hz();
	unsigned long mp_clock_get_speed();
	unsigned long mp_clock_us(void);
	unsigned long mp_clock_level_hz(mp_power_level_t level);
	mp_ret_t mp_clock_level(mp_kernel_t *kernel, mp_power_level_t level);
	void mp_clock_schedule(mp_kernel_t *kernel, unsigned long limit);
	const char *mp_clock_deep(void);
	/** what refused the last mp_clock_level(), or NULL */
	const char *mp_clock_blocker(void);
	void mp_machine_state_set(mp_kernel_t *kernel);
	void mp_machine_state_unset(mp_kernel_t *kernel);
	/**
	 * The unique id of this chip, lowercase hex, or NULL when the chip has none.
	 * The text stays valid. A part with no serial of its own uses it as its serial.
	 */
	const char *mp_machine_serial(void);

	#define MP_KERNEL_KPANIC 0
	#define MP_KERNEL_BOOT   1
	#define MP_KERNEL_RES01  2
	#define MP_KERNEL_RES02  3
	#define MP_KERNEL_RES03  4
	#define MP_KERNEL_RES04  5
	#define MP_KERNEL_RES05  6
	#define MP_KERNEL_RES06  7
	#define MP_KERNEL_RES07  8
	#define MP_KERNEL_RES08  9
	#define MP_KERNEL_RES09  10
	#define MP_KERNEL_RES10  11

	/* build/include only holds the headers of the modules and drivers selected */
	#include <utils.h>
	#ifdef SUPPORT_COMMON_MEM
		#include <mem/mem.h>
	#endif
	#ifdef SUPPORT_COMMON_KFLASH
		#include <kflash/kflash.h>
	#endif
	#ifdef SUPPORT_COMMON_CIRCULAR
		#include <circular/circular.h>
	#endif
	#include <state.h>
	#ifdef SUPPORT_COMMON_HCI
		#include <hci/hci.h>
	#endif
	#ifdef SUPPORT_COMMON_SERIAL
		#include <serial/serial.h>
	#endif
	#ifdef SUPPORT_COMMON_PINOUT
		#include <pinout/pinout.h>
	#endif
	#include <printk.h>
	#ifdef SUPPORT_COMMON_QUATERNION
		#include <quaternion/quaternion.h>
	#endif
	#ifdef SUPPORT_COMMON_REGMASTER
		#include <regmaster/regMaster.h>
	#endif
	#ifdef SUPPORT_COMMON_KALMAN
		#include <kalman/kalman.h>
	#endif
	#ifdef SUPPORT_COMMON_TUI
		#include <tui/tui.h>
	#endif
	#ifdef SUPPORT_COMMON_DEVICE
		#include <device/device.h>
	#endif
	#ifdef SUPPORT_COMMON_RESISTIVE
		#include <resistive/resistive.h>
	#endif
	#ifdef SUPPORT_COMMON_BUTTON
		#include <button/button.h>
	#endif
	#ifdef SUPPORT_COMMON_USB
		#include <usb/usb.h>
	#endif
	#ifdef SUPPORT_COMMON_DMA
		#include <dma/dma.h>
	#endif
	#ifdef SUPPORT_COMMON_CRYPTO_HASH_BLAKE2S
		#include <crypto/blake2s.h>
	#endif
	#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHA1
		#include <crypto/sha1.h>
	#endif
	#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHA256
		#include <crypto/sha256.h>
	#endif
	#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHA384
		#include <crypto/sha384.h>
	#endif
	#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHA512
		#include <crypto/sha512.h>
	#endif
	#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHAKE
		#include <crypto/shake.h>
	#endif
	/* after the algorithms: its context is a union of theirs */
	#ifdef SUPPORT_COMMON_CRYPTO_HASH
		#include <crypto/hash.h>
	#endif
	#ifdef SUPPORT_COMMON_CRYPTO_MAC_HMAC
		#include <crypto/hmac.h>
	#endif
	#ifdef SUPPORT_COMMON_CRYPTO_MAC_POLY1305
		#include <crypto/poly1305.h>
	#endif
	/* after the MACs: its context is a union of theirs */
	#ifdef SUPPORT_COMMON_CRYPTO_MAC
		#include <crypto/mac.h>
	#endif
	#ifdef SUPPORT_COMMON_CRYPTO_RNG_HMAC_DRBG
		#include <crypto/hmac_drbg.h>
	#endif
	/* after the generators: the task reseeds the one this build carries */
	#ifdef SUPPORT_COMMON_CRYPTO_RNG
		#include <crypto/rng.h>
	#endif
	#ifdef SUPPORT_COMMON_CRYPTO_BN
		#include <crypto/bn.h>
	#endif
	#ifdef SUPPORT_COMMON_CRYPTO_SIGN_ED25519
		#include <crypto/ed25519.h>
	#endif
	#ifdef SUPPORT_COMMON_CRYPTO_SIGN_ECDSA_P256
		#include <crypto/ecdsa_p256.h>
	#endif
	#ifdef SUPPORT_COMMON_CRYPTO_SIGN_RSA
		#include <crypto/rsa.h>
	#endif
	#ifdef SUPPORT_COMMON_CRYPTO_SIGN_LMS
		#include <crypto/lms.h>
	#endif
	#ifdef SUPPORT_COMMON_CRYPTO_SIGN_SLH
		#include <crypto/slh.h>
	#endif
	#ifdef SUPPORT_COMMON_CRYPTO_SIGN_MLDSA
		#include <crypto/mldsa.h>
	#endif
/* after the algorithms: the index calls them by name */
#ifdef SUPPORT_COMMON_CRYPTO_SIGN
	#include <crypto/sign.h>
#endif

#ifdef SUPPORT_COMMON_CRYPTO_CIPHER_AES
	#include <crypto/aes.h>
#endif
#ifdef SUPPORT_COMMON_CRYPTO_CIPHER_CHACHA20
	#include <crypto/chacha20.h>
#endif
/* after the algorithms: the index calls them by name */
#ifdef SUPPORT_COMMON_CRYPTO_CIPHER
	#include <crypto/cipher.h>
#endif
/* after the modes: the index calls them by name; a mode wraps a cipher */
#ifdef SUPPORT_COMMON_CRYPTO_MODE_CHACHA20_POLY1305
	#include <crypto/chacha20_poly1305.h>
#endif
#ifdef SUPPORT_COMMON_CRYPTO_MODE_CCM
	#include <crypto/ccm.h>
#endif
#ifdef SUPPORT_COMMON_CRYPTO_MODE
	#include <crypto/mode.h>
#endif

	#ifdef SUPPORT_DRV_LED
		#include <led/led.h>
	#endif
	#ifdef SUPPORT_DRV_LCD_ILI932X
		#include <ili932x/ili932x.h>
	#endif
	#ifdef SUPPORT_DRV_LCD_NOKIA3310
		#include <nokia3310/nokia3310.h>
	#endif
	#ifdef SUPPORT_DRV_NRF8001
		#include <nrf8001/nRF8001.h>
	#endif
	#ifdef SUPPORT_DRV_LSM9DS0
		#include <lsm9ds0/LSM9DS0.h>
	#endif
	#ifdef SUPPORT_DRV_TMP006
		#include <tmp006/TMP006.h>
	#endif
	#ifdef SUPPORT_DRV_MPL3115A2
		#include <mpl3115a2/MPL3115A2.h>
	#endif
	#ifdef SUPPORT_DRV_ADS1115
		#include <ads1115/ADS1115.h>
	#endif
	#ifdef SUPPORT_DRV_INA219
		#include <ina219/INA219.h>
	#endif
	#ifdef SUPPORT_DRV_ADS124X
		#include <ads124x/ADS124x.h>
	#endif
	#ifdef SUPPORT_DRV_SMB380
		#include <smb380/smb380.h>
	#endif
	#ifdef SUPPORT_DRV_LEPTON
		#include <lepton/lepton.h>
	#endif
	#ifdef SUPPORT_DRV_DS1307
		#include <ds1307/ds1307.h>
	#endif

	#define MP_KERNEL_VERSION "2.0.0"

	struct mp_kernel_s {
		/** Vendor name  */
		char *mcuVendor;

		/** Microchip name  */
		char *mcuName;

		/** Kernel version  */
		char *version;

		/** Kernel states */
		mp_state_handler_t states;

		/** Kernel tasks */
		mp_task_handler_t tasks;

		/** Level, governor and holds */
		mp_power_t power;

#ifdef SUPPORT_COMMON_DEVICE
		/** every mp_device_input_t, see mp_device_input_next() */
		mp_list_t inputs;
#endif

#ifdef SUPPORT_COMMON_USB
		/** PHY ports, see mp_usb_at() */
		mp_list_t usbs;
#endif

#ifdef SUPPORT_COMMON_DEVICE
		/** loaded drivers, see mp_device_driver_at() */
		mp_list_t drivers;
		/** registered devices, see mp_device_registry_at() */
		mp_list_t devices;
#endif

#ifdef SUPPORT_COMMON_KFLASH
		/** values kept in flash, see mp_kflash_listen() */
		mp_kflash_t kflash;
#endif

		/** Internal tick timer */
		mp_timer_t tickTimer;

		/** Kernel on boot state */
		mp_kernel_onBoot_t onBoot;

		/** User pointer for onBoot */
		void *onBootUser;

		/** Arch state, mp_arch_t comes from arch/internal.h */
		mp_arch_t arch;
	};

	char *mp_options_get(mp_options_t *options, char *key);
	mp_ret_t mp_options_cmp(char *a, char *b);

	void mp_kernel_init(mp_kernel_t *kernel, mp_kernel_onBoot_t onBoot, void *user);
	void mp_kernel_fini(mp_kernel_t *kernel);
	void mp_kernel_state(mp_kernel_t *kernel, char number);
	void mp_kernel_loop(mp_kernel_t *kernel);
	void mp_kernel_panic(mp_kernel_t *kernel, int error);

	/* error codes for kpanic */
	#define KPANIC_MEM_SIZE 1
	#define KPANIC_MEM_OOM  2
	#define KPANIC_MEM_CORRUPT 3
#endif
