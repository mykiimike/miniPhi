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


	typedef struct mp_regMaster_op_s mp_regMaster_op_t;
	typedef struct mp_regMaster_s mp_regMaster_t;

	typedef void (*mp_regMaster_cb_t)(mp_regMaster_op_t *operand);
	typedef void (*mp_regMaster_int_t)(mp_regMaster_t *cirr);

	#define MP_REGMASTER_STATE_TX 1
	#define MP_REGMASTER_STATE_RX 2

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

		mp_regMaster_op_t *next;
	};

	struct mp_regMaster_s {
		mp_kernel_t *kernel;

		mp_regMaster_op_t *first;
		mp_regMaster_op_t *last;

		mp_regMaster_int_t enableRX;
		mp_regMaster_int_t disableRX;

		mp_regMaster_int_t enableTX;
		mp_regMaster_int_t disableTX;

		union {
			mp_i2c_t *i2c;
		};

		void *user;

		mp_task_t *asr;
	};


#endif
