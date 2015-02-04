/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2015  Michael VERGOZ                                      *
 * Copyright (C) 2015  VERMAN                                              *
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

#ifndef _HAVE_MP_COMMON_REGMASTER_H
	#define _HAVE_MP_COMMON_REGMASTER_H


	#define MP_REGMASTER_STATE_TX 1
	#define MP_REGMASTER_STATE_RX 2

	/**
	 * @defgroup mpCommonRegMaster
	 * @{
	 */
	typedef struct mp_regMaster_op_s mp_regMaster_op_t;
	typedef struct mp_regMaster_s mp_regMaster_t;

	typedef void (*mp_regMaster_cb_t)(mp_regMaster_op_t *operand, mp_bool_t terminate);
	typedef void (*mp_regMaster_int_t)(mp_regMaster_t *cirr);


	struct mp_regMaster_op_s {
		char state;

		unsigned char *reg;
		int regSize;
		int regPos;

		unsigned char *wait;
		int waitSize;
		int waitPos;

		mp_regMaster_cb_t callback;
		void *user;

		mp_list_item_t item;
	};

	struct mp_regMaster_s {
		mp_kernel_t *kernel;

		mp_list_t executing;
		mp_list_t pending;

		mp_regMaster_int_t enableRX;
		mp_regMaster_int_t disableRX;

		mp_regMaster_int_t enableTX;
		mp_regMaster_int_t disableTX;

		/** On bus error */
		//mp_regMaster_int_t error;

		mp_i2c_t *i2c;


		void *user;

		mp_task_t *asr;



	};

	/** @} */

	mp_ret_t mp_regMaster_init_i2c(
		mp_kernel_t *kernel, mp_regMaster_t *cirr,
		mp_i2c_t *i2c,
		void *user,
		char *who
	);
	void mp_regMaster_fini(mp_regMaster_t *cirr);
	mp_ret_t mp_regMaster_read(
		mp_regMaster_t *cirr,
		unsigned char *reg, int regSize,
		unsigned char *wait, int waitSize,
		mp_regMaster_cb_t callback, void *user
	);
	mp_ret_t mp_regMaster_write(
		mp_regMaster_t *cirr,
		unsigned char *reg, int regSize,
		mp_regMaster_cb_t callback, void *user
	);

#endif
