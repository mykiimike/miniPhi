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

	#define SUPPORT_DRV_LED
	#define SUPPORT_DRV_BUTTON
	//#define SUPPORT_DRV_TEMPERATURE

	//#define SUPPORT_DRV_LCD_NOKIA3310

	#define SUPPORT_COMMON_MEM /* enable tiny-malloc */
	#define SUPPORT_COMMON_SERIAL
	#define SUPPORT_COMMON_PINOUT /* enable pinout feature, need mem support */

	/* mem configuration */
	#ifndef MP_MEM_SIZE
		#define MP_MEM_SIZE  5120 /* total memory allowed for heap */
	#endif

	#ifndef MP_MEM_CHUNK
		#define MP_MEM_CHUNK 50    /* fixed size of a chunck */
	#endif

	/* task configuration */
	#ifndef MP_TASK_MAX
		#define MP_TASK_MAX 10 /* number of maximum task per instance */
	#endif

	/* state configuration */
	#ifndef MP_STATE_MAX
		#define MP_STATE_MAX 5 /* maximum number of machine states */
	#endif

	/* serial configuration */
	#ifndef MP_SERIAL_RX_BUFFER_SIZE
		#define MP_SERIAL_RX_BUFFER_SIZE 512
	#endif

	#ifndef MP_SERIAL_TX_BUFFER_SIZE
		#define MP_SERIAL_TX_BUFFER_SIZE 240
	#endif

	/* button configuration */


#endif
