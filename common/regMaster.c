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
		void *user
	) {
	memset(cirr, 0, sizeof(*cirr));

	cirr->kernel = kernel;

	cirr->enableRX = _mp_regMaster_i2c_enableRX;
	cirr->disableRX = _mp_regMaster_i2c_disableRX;

	cirr->enableTX = _mp_regMaster_i2c_enableTX;
	cirr->disableTX = _mp_regMaster_i2c_disableTX;

	cirr = user;

	cirr->i2c = i2c;
	cirr->i2c->user = cirr;

	cirr->disableRX(cirr);
	cirr->disableTX(cirr);

	return(TRUE);
}

/**
 * @brief Terminate circular register context
 *
 * This terminate a circular register context
 *
 * @param[in] cirr Circular context.
 */
void mp_register_fini(mp_regMaster_t *cirr) {

	cirr->disableRX(cirr);
	cirr->disableTX(cirr);

}



/**
 * @brief Start circular register read operation
 *
 * This initiates a read operation using circular register
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
	operand = mp_mem_alloc(cirr->kernel, sizeof(*operand));

	operand->state = MP_REGMASTER_STATE_TX;

	operand->reg = reg;
	operand->regSize = regSize;
	operand->regPos = 0;

	operand->wait = reg;
	operand->waitSize = regSize;
	operand->waitPos = 0;

	operand->callback = callback;
	operand->user = user;

	operand->next = NULL;

	/* then move buffers */
	cirr->disableTX(cirr);

	if(cirr->last)
		cirr->last->next = operand;
	else
		cirr->first = operand;
	cirr->last = operand;

	cirr->enableTX(cirr);

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

	operand->next = NULL;

	/* then move buffers */
	cirr->disableTX(cirr);

	if(cirr->last)
		cirr->last->next = operand;
	else
		cirr->first = operand;
	cirr->last = operand;

	cirr->enableTX(cirr);

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
	operand = cirr->first;
	if(!operand) {
		cirr->disableRX(cirr);
		cirr->disableTX(cirr);
		return;
	}

	/* send registers */
	if(operand->state == MP_REGMASTER_STATE_TX && flag == MP_I2C_FL_TX) {

		/* check for end of register */
		if(operand->regPos == operand->regSize) {
			/* need to read data */
			if(operand->waitSize > 0) {
				cirr->disableTX(cirr);
				cirr->enableRX(cirr);

				/* receiver mode */
				mp_i2c_mode(i2c, 0);

				/* restart */
				mp_i2c_txStart(i2c);

				operand->state = MP_REGMASTER_STATE_RX;

			}
			/* no need to read */
			else {
				/* execute callback */
				if(operand->callback)
					operand->callback(operand);

				/* free operand */
				if(operand->next) {
					cirr->first = operand->next;

					mp_mem_free(cirr->kernel, operand);

					/* don't touch interrupts */
				}
				else {
					/* free last buffer */
					mp_mem_free(cirr->kernel, operand);

					cirr->first = NULL;
					cirr->last = NULL;

					cirr->disableTX(cirr);
					cirr->disableRX(cirr);
				}
			}

		}
		/* initiate start */
		else if(operand->regPos == 0) {
			/* TX mode */
			mp_i2c_mode(i2c, 1);
			mp_i2c_txStart(i2c);
			mp_i2c_tx(i2c, operand->reg[operand->regPos++]);
		}
		else
			mp_i2c_tx(i2c, operand->reg[operand->regPos++]);
	}

	/* read data, CTR and start has already been sent */
	else if(operand->state == MP_REGMASTER_STATE_RX && flag == MP_I2C_FL_RX) {
		rest = operand->waitSize-operand->waitPos-1;

		if(rest) {
			operand->wait[operand->waitPos++] = mp_i2c_rx(i2c);

			/* Only one byte left? */
			if (rest == 1)
				mp_i2c_txStop(i2c);
		}
		/* Move final RX data */
		else {
			operand->wait[operand->waitPos++] = mp_i2c_rx(i2c);

			/* execute callback */
			if(operand->callback)
				operand->callback(operand);

			/* free operand */
			if(operand->next) {
				cirr->first = operand->next;

				mp_mem_free(cirr->kernel, operand);

				/* change interrupt context */
				cirr->enableTX(cirr);
				cirr->disableRX(cirr);
			}
			else {
				/* free last buffer */
				mp_mem_free(cirr->kernel, operand);

				cirr->first = NULL;
				cirr->last = NULL;

				cirr->disableTX(cirr);
				cirr->disableRX(cirr);
			}
		}
	}
}
