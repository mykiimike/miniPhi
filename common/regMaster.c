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

#include <mp.h>

/* i2c master side implementation */
static void _mp_regMaster_i2c_enableRX(mp_regMaster_t *cirr);
static void _mp_regMaster_i2c_disableRX(mp_regMaster_t *cirr);
static void _mp_regMaster_i2c_enableTX(mp_regMaster_t *cirr);
static void _mp_regMaster_i2c_disableTX(mp_regMaster_t *cirr);
static void _mp_regMaster_i2c_interrupt(mp_i2c_t *i2c, mp_i2c_flag_t flag);
MP_TASK(mp_regMaster_asr);

/**
 * @brief Initiate circular register context
 *
 * This initiates a circular register context
 *
 * @param[in] kernel Kernel handler
 * @param[in] cirr Circular context.
 * @param[in] user User pointer embedded
 */
mp_ret_t mp_regMaster_init_i2c(
		mp_kernel_t *kernel, mp_regMaster_t *cirr,
		mp_i2c_t *i2c,
		void *user,
		char *who
	) {
	memset(cirr, 0, sizeof(*cirr));

	cirr->kernel = kernel;

	mp_list_init(&cirr->pending);
	mp_list_init(&cirr->executing);

	cirr->enableRX = _mp_regMaster_i2c_enableRX;
	cirr->disableRX = _mp_regMaster_i2c_disableRX;

	cirr->enableTX = _mp_regMaster_i2c_enableTX;
	cirr->disableTX = _mp_regMaster_i2c_disableTX;

	cirr->user = user;

	cirr->i2c = i2c;
	cirr->i2c->user = cirr;
	mp_i2c_setInterruption(i2c, _mp_regMaster_i2c_interrupt);

	cirr->enableRX(cirr);
	cirr->enableTX(cirr);

	/* create task and place it in sleep mode */
	cirr->asr = mp_task_create(&kernel->tasks, who, mp_regMaster_asr, cirr, 100);
	if(!cirr->asr)
		return(FALSE);
	cirr->asr->signal = MP_TASK_SIG_SLEEP;

	return(TRUE);
}

/**
 * @brief Terminate circular register context
 *
 * This terminate a circular register context
 *
 * @param[in] cirr Circular context.
 */
void mp_regMaster_fini(mp_regMaster_t *cirr) {
	/* regMaster is destructed using ASR task then
	 * we just send stop signal and disable interrupts */
	mp_task_destroy(cirr->asr);
	cirr->disableRX(cirr);
	cirr->disableTX(cirr);
}



/**
 * @brief Start circular register read operation
 *
 * This initiates a read operation using circular register.
 *
 * When callback() is executed you must take care of the allocated
 * pointer (if used). The terminate boolean argument is set to TRUE
 * to notify whether regMaster has been shutdown.
 * In this case you must stop actions and free buffers if needed.
 *
 * @param[in] cirr Circular context.
 * @param[in] reg Registers to write
 * @param[in] regSize Size of the registers to write
 * @param[out] wait Buffer to fill
 * @param[in] waitSize Number of bytes to read. wait allocation must be aligned with waitSize.
 * @param[in] callback Callback executed on the end of operation
 * @param[in] user User pointer embedded and passed as argument
 */
mp_ret_t mp_regMaster_read(
		mp_regMaster_t *cirr,
		unsigned char *reg, int regSize,
		unsigned char *wait, int waitSize,
		mp_regMaster_cb_t callback, void *user
	) {
	mp_regMaster_op_t *operand;

	/* allocate new operand */
	//operand = mp_mem_alloc(cirr->kernel, sizeof(*operand));
	operand = malloc(sizeof(*operand));

	operand->state = MP_REGMASTER_STATE_TX;

	operand->reg = reg;
	operand->regSize = regSize;
	operand->regPos = 0;

	operand->wait = wait;
	operand->waitSize = waitSize;
	operand->waitPos = 0;

	operand->callback = callback;
	operand->user = user;

	/* add operand at last pending */
	mp_list_add_last(&cirr->pending, &operand->item, operand);

	/* tell to the scheduler task pending */
	if(cirr->asr->signal == MP_TASK_SIG_SLEEP)
		cirr->asr->signal = MP_TASK_SIG_PENDING;

	return(TRUE);
}

/**
 * @brief Start circular register write operation
 *
 * This initiates a write operation using circular register
 *
 * @param[in] cirr Circular context.
 * @param[in] reg Registers to write
 * @param[in] regSize Size of the registers to write
 * @param[in] callback Callback executed on the end of operation
 * @param[in] user User pointer embedded and passed as argument
 */
mp_ret_t mp_regMaster_write(
		mp_regMaster_t *cirr,
		unsigned char *reg, int regSize,
		mp_regMaster_cb_t callback, void *user
	) {
	mp_regMaster_op_t *operand;

	/* allocate new operand */
	operand = mp_mem_alloc(cirr->kernel, sizeof(*operand));

	operand->state = MP_REGMASTER_STATE_TX;

	operand->reg = reg;
	operand->regSize = regSize;
	operand->regPos = 0;

	operand->callback = callback;
	operand->user = user;

	/* add operand at last pending */
	mp_list_add_last(&cirr->pending, &operand->item, operand);

	/* tell to the scheduler task pending */
	if(cirr->asr->signal == MP_TASK_SIG_SLEEP)
		cirr->asr->signal = MP_TASK_SIG_PENDING;
	return(TRUE);
}


static void _mp_regMaster_i2c_enableRX(mp_regMaster_t *cirr) {
	mp_i2c_enable_rx(cirr->i2c);
}

static void _mp_regMaster_i2c_disableRX(mp_regMaster_t *cirr) {
	mp_i2c_disable_rx(cirr->i2c);
}

static void _mp_regMaster_i2c_enableTX(mp_regMaster_t *cirr) {
	mp_i2c_enable_tx(cirr->i2c);
}

static void _mp_regMaster_i2c_disableTX(mp_regMaster_t *cirr) {
	mp_i2c_disable_tx(cirr->i2c);
}

static void _mp_regMaster_i2c_interrupt(mp_i2c_t *i2c, mp_i2c_flag_t flag) {
	mp_regMaster_t *cirr = i2c->user;
	mp_regMaster_op_t *operand;
	int rest;

	/* get first operand */
	operand = cirr->pending.first ? cirr->pending.first->user : NULL;
	if(!operand)
		return;

	/* send registers */
	if(operand->state == MP_REGMASTER_STATE_TX && flag == MP_I2C_FL_TX) {

		/* check for end of register */
		if(operand->regPos == operand->regSize) {

			/* need to read data */
			if(operand->waitSize > 0) {
				operand->state = MP_REGMASTER_STATE_RX;
				cirr->asr->signal = MP_TASK_SIG_PENDING;
				cirr->disableTX(cirr);
			}
			/* no need to read */
			else {
				mp_i2c_txStop(i2c);

				/* switch buffer into ASR space */
				mp_list_switch_last(&cirr->executing, &cirr->pending, &operand->item);
				cirr->asr->signal = MP_TASK_SIG_PENDING;

				cirr->disableTX(cirr);
			}
		}
		else
			mp_i2c_tx(i2c, operand->reg[operand->regPos++]);

	}

	/* read data, CTR and start has already been sent */
	else if(operand->state == MP_REGMASTER_STATE_RX && flag == MP_I2C_FL_RX) {

		rest = operand->waitSize-operand->waitPos-1;

		if(rest == 0) {
			if(operand->waitSize > 1)
				mp_i2c_txStop(i2c);

			operand->wait[operand->waitPos++] = mp_i2c_rx(i2c);

			/* switch buffer into ASR space */
			mp_list_switch_last(&cirr->executing, &cirr->pending, &operand->item);
			cirr->asr->signal = MP_TASK_SIG_PENDING;

			cirr->disableRX(cirr);
			cirr->disableTX(cirr);
		}
		else {
			operand->wait[operand->waitPos++] = mp_i2c_rx(i2c);
		}
	}

	return;
}

MP_TASK(mp_regMaster_asr) {
	mp_regMaster_t *cirr = task->user;
	mp_regMaster_op_t *cur;
	mp_regMaster_op_t *next;


	/* receive regMaster shutdown */
	if(task->signal == MP_TASK_SIG_STOP) {
		cirr->disableTX(cirr);
		cirr->disableRX(cirr);

		/* free pending list */
		if(cirr->pending.first) {
			cur = cirr->pending.first->user;
			while(cur) {
				next = cur->item.next != NULL ? cur->item.next->user : NULL;

				if(cur->callback)
					cur->callback(cur, TRUE);

				mp_list_remove(&cirr->pending, &cur->item);
				mp_mem_free(cirr->kernel, cur);
				cur = next;
			}
		}

		/* free executing list */
		if(cirr->pending.first) {
			cur = cirr->pending.first->user;
			while(cur) {
				next = cur->item.next != NULL ? cur->item.next->user : NULL;

				if(cur->callback)
					cur->callback(cur, TRUE);

				mp_list_remove(&cirr->pending, &cur->item);
				mp_mem_free(cirr->kernel, cur);
				cur = next;
			}
		}

		/* close i2c interface */
		mp_i2c_close(cirr->i2c);

		/* acknowledging */
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	/* FIFO pop */
	if(cirr->executing.first) {
		cur = cirr->executing.first->user;

		/* execute callback in asr mode */
		if(cur->callback)
			cur->callback(cur, FALSE);

		/* remove completely the buffer */
		mp_list_remove(&cirr->executing, &cur->item);
		mp_mem_free(cirr->kernel, cur);
	}

	task->signal = MP_TASK_SIG_SLEEP;

	/* pending request activate interruption */

	if(cirr->pending.first) {
		cur = cirr->pending.first->user;

		if(cur->state == MP_REGMASTER_STATE_TX) {
			mp_i2c_mode(cirr->i2c, 1);
			mp_i2c_txStart(cirr->i2c);
			cirr->enableTX(cirr);
		}
		else if(cur->state == MP_REGMASTER_STATE_RX) {
			if(cur->waitSize == 1) {
				mp_i2c_waitStop(cirr->i2c);
				mp_i2c_mode(cirr->i2c, 0);
				mp_i2c_txStart(cirr->i2c);
				mp_i2c_waitStart(cirr->i2c);
				mp_i2c_txStop(cirr->i2c);

				cirr->enableRX(cirr);
			}
			else {
				/* receiver mode */
				mp_i2c_mode(cirr->i2c, 0);
				mp_i2c_txStart(cirr->i2c);

				cirr->enableRX(cirr);
			}
		}
	}
}

