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

#ifndef _HAVE_MP_COMMON_PRINTK
	#define _HAVE_MP_COMMON_PRINTK

	/**
	 * @defgroup mpCommonPrintk
	 * @{
	 */

	#ifdef _DEBUG
		#define _DP(a, args...) mp_printk_call(mp_printk_user, "* %s(): "a, __func__, ##args)
	#else
		#define _DP(a, ...)
	#endif

	/**
	 * @brief miniPhi printk()
	 *
	 * This function is the general call to output messages.
	 *
	 * @param[in] a Format string
	 * @param[in] args Format argument
	 */
	#if MP_CPUS > 1
		/* one line at a time across cores: the sink keeps its own state */
		#define mp_printk(a, args...) do { \
			int _mp_printk_key = mp_printk_enter(); \
			mp_printk_call(mp_printk_user, a, ##args); \
			mp_printk_leave(_mp_printk_key); \
		} while(0)
		/* the key to hand back, negative when the line is not held */
		int mp_printk_enter(void);
		void mp_printk_leave(int key);
	#else
		#define mp_printk(a, args...) mp_printk_call(mp_printk_user, a, ##args)
	#endif

	typedef void (*mp_printk_call_t)(void *user, char *fmt, ...);

	/**@}*/

	extern mp_printk_call_t mp_printk_call;
	extern void *mp_printk_user;

	mp_ret_t mp_printk_set(mp_printk_call_t call, void *user);
	mp_ret_t mp_printk_unset();

#endif
