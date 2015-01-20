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

#ifndef _HAVE_CONFIG_H
	#define _HAVE_CONFIG_H

	#define _DEBUG

	#define SUPPORT_DRV_LED
	#define SUPPORT_DRV_BUTTON
	//#define SUPPORT_DRV_LSM9DS0
	//#define SUPPORT_DRV_TMP006
	//#define SUPPORT_DRV_LCD_NOKIA3310
	//#define SUPPORT_DRV_MPL3115A2

	#define SUPPORT_COMMON_MEM /* enable tiny-malloc */
	#define SUPPORT_COMMON_SERIAL /* serial interface */
	#define SUPPORT_COMMON_PINOUT /* enable pinout feature, need mem support */
	//#define SUPPORT_COMMON_QUATERNION /* enable quaternion feature */
	#define SUPPORT_COMMON_SENSOR /* enable sensor feature */

	/* clock manager */
	#ifndef MP_CLOCK_LE_FREQ
		#define MP_CLOCK_LE_FREQ MHZ1_t
	#endif

	#ifndef MP_CLOCK_HE_FREQ
		#define MP_CLOCK_HE_FREQ MHZ25_t
	#endif

	/* sensor configuration */

	/* mem configuration */
	#ifndef MP_MEM_SIZE
		#define MP_MEM_SIZE  1024 /* total memory allowed for heap */
	#endif

	#ifndef MP_MEM_CHUNK
		#define MP_MEM_CHUNK 50    /* fixed size of a chunck */
	#endif

	#ifndef MP_COMMON_MEM_USE_MALLOC
		#define MP_COMMON_MEM_USE_MALLOC
	#endif

	/* task configuration */
	#ifndef MP_TASK_MAX
		#define MP_TASK_MAX 10 /* number of maximum task per instance */
	#endif

	/* state configuration */
	#ifndef MP_STATE_MAX
		#define MP_STATE_MAX 5 /* maximum number of machine states */
	#endif

#endif
