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
MP_TASK(mp_regMaster_i2c_asr);

static void _mp_regMaster_spi_enableRX(mp_regMaster_t *cirr);
static void _mp_regMaster_spi_disableRX(mp_regMaster_t *cirr);
static void _mp_regMaster_spi_enableTX(mp_regMaster_t *cirr);
static void _mp_regMaster_spi_disableTX(mp_regMaster_t *cirr);
static void _mp_regMaster_spi_interrupt(mp_spi_t *spi, mp_spi_flag_t flag);


static unsigned char *_registers = NULL;
static int _register_references = 0;

static void _mp_regMaster_ginit(mp_kernel_t *kernel) {
	int a;

	if(_registers)
		return;

	_registers = malloc(256);
	for(a=0; a<256; a++)
		_registers[a] = a;

	_register_references++;
}

static void _mp_regMaster_gfini(mp_kernel_t *kernel) {
	_register_references--;
	if(_register_references == 0)
		free(_registers);

}

/**
@defgroup mpCommonRegMaster Register Master communication

@ingroup mpCommon

@brief Circular register model for Master node

@version 1.0.0

@author @htmlonly &copy; @endhtmlonly 2015
Michael Vergoz <mv@verman.fr>

@date 03 Feb 2015

Examples

Driver structure
@code
struct mp_drv_MPL3115A2_s {
	mp_kernel_t *kernel;

	mp_i2c_t i2c;
	mp_regMaster_t regMaster;

	// [...]
};
@endcode

Before initializing regMaster you must setup the I2C or SPI interfaces.
Here is an example using I2C

@code
ret = mp_regMaster_init_i2c(kernel, &MPL3115A2->regMaster,
		&MPL3115A2->i2c, MPL3115A2, "MPL3115A2 I2C");
if(ret == FALSE) {
	mp_printk("MPL3115A2 error while creating regMaster context");
	mp_i2c_close(&MPL3115A2->i2c);
	return(NULL);
}
@endcode


In regMaster a read operation comes with a write before to read.
This is how register communication works.
regMaster has it own queuing system which is splited in "operands".
Each operand has a reg pointer and a wait pointer respectively are the
register(s) to write and the data to receive.
The allocation of those pointers are not managed by regMaster then you
will have to control the buffer by yourself.

There are two ways to control it. Use a .bss pointer which doesn't
need to be free as followed :
@code
mp_regMaster_read(
	&MPL3115A2->regMaster,
	&_registers[MPL3115A2_WHO_AM_I], 1,
	(unsigned char *)&MPL3115A2->whoIam, 1,
	_mp_drv_MPL3115A2_onWhoIAm, MPL3115A2
);
@endcode

Or use an allocated memory space in the HEAP to receive or emit information
and then you will have to the end callback to free the buffer :
@code
void _mp_drv_MPL3115A2_writeControl(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_MPL3115A2_t *MPL3115A2 = operand->user;
	// free allocated register
	mp_mem_free(MPL3115A2->kernel, operand->reg);
}

// [...]

unsigned char *ptr = mp_mem_alloc(MPL3115A2->kernel, 2);
unsigned char *src = ptr;

// forge the registers to write
*(ptr++) = MPL3115A2_PT_DATA_CFG;
*ptr = 0x07;

mp_regMaster_write(
	&MPL3115A2->regMaster,
	src, 2,
	_mp_drv_MPL3115A2_writeControl, MPL3115A2
);
@endcode

@{
*/

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

	/* run ginit */
	_mp_regMaster_ginit(kernel);

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

	cirr->type = MP_REGMASTER_I2C;

	/* save actual slave address */
	cirr->slaveAddress = mp_i2c_getSlaveAddress(cirr->i2c);

	/* create task and place it in sleep mode */
	cirr->asr = mp_task_create(&kernel->tasks, who, mp_regMaster_i2c_asr, cirr, 100);
	if(!cirr->asr)
		return(FALSE);
	cirr->asr->signal = MP_TASK_SIG_SLEEP;

	return(TRUE);
}

/**
 * @brief Initiate circular register context
 *
 * This initiates a circular register context
 *
 * @param[in] kernel Kernel handler
 * @param[in] cirr Circular context.
 * @param[in] user User pointer embedded
 */
mp_ret_t mp_regMaster_init_spi(
		mp_kernel_t *kernel, mp_regMaster_t *cirr,
		mp_spi_t *spi,
		void *user,
		char *who
	) {
	memset(cirr, 0, sizeof(*cirr));

	/* run ginit */
	_mp_regMaster_ginit(kernel);

	cirr->kernel = kernel;

	mp_list_init(&cirr->pending);
	mp_list_init(&cirr->executing);

	cirr->enableRX = _mp_regMaster_spi_enableRX;
	cirr->disableRX = _mp_regMaster_spi_disableRX;

	cirr->enableTX = _mp_regMaster_spi_enableTX;
	cirr->disableTX = _mp_regMaster_spi_disableTX;

	cirr->user = user;

	cirr->spi = spi;
	cirr->spi->user = cirr;

	//mp_i2c_setInterruption(i2c, _mp_regMaster_i2c_interrupt);

	cirr->enableRX(cirr);
	cirr->enableTX(cirr);

	cirr->type = MP_REGMASTER_SPI;

	/* save actual slave address */
	//cirr->slaveAddress = mp_i2c_getSlaveAddress(cirr->i2c);

	/* create task and place it in sleep mode */
	cirr->asr = mp_task_create(&kernel->tasks, who, mp_regMaster_i2c_asr, cirr, 100);
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
 * @brief Extended circular register read operation
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
 * @param[in] swap set to TRUE to swap RX buffer
 */
mp_ret_t mp_regMaster_readExt(
		mp_regMaster_t *cirr,
		unsigned char *reg, int regSize,
		unsigned char *wait, int waitSize,
		mp_regMaster_cb_t callback, void *user,
		mp_bool_t swap
	) {
	mp_regMaster_op_t *operand;

	/* allocate new operand */
	operand = mp_mem_alloc(cirr->kernel, sizeof(*operand));
	//operand = malloc(sizeof(*operand));

	if(cirr->type == MP_REGMASTER_I2C)
		operand->slaveAddress = cirr->slaveAddress;

	operand->state = MP_REGMASTER_STATE_TX;

	operand->reg = reg;
	operand->regSize = regSize;
	operand->regPos = 0;

	operand->wait = wait;
	operand->waitSize = waitSize;
	operand->waitPos = 0;

	operand->callback = callback;
	operand->user = user;

	operand->swap = swap;

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

	if(cirr->type == MP_REGMASTER_I2C)
		operand->slaveAddress = cirr->slaveAddress;

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

/**
 * @brief Get HEAP memory for register
 *
 * This function is very useful because it allows to get a valid HEAP pointer which contains the register to write.
 * In this case you don't need to allocate or free register after its usage.
 * Only limitation register is limited to 8bits.
 *
 * @param[in] reg Register content
 * @return HEAP memory pointer contains the register
 */
unsigned char *mp_regMaster_register(unsigned char reg) {
	return(&_registers[reg]);
}

/**@}*/

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

		if(rest == 1) {
			if(operand->waitSize > 1)
				mp_i2c_txStop(i2c);
		}

		if(!operand->swap)
			operand->wait[operand->waitPos++] = mp_i2c_rx(i2c);
		else {
			operand->wait[rest] = mp_i2c_rx(i2c);
			operand->waitPos++;
		}

		if(rest == 0) {
			/* switch buffer into ASR space */
			mp_list_switch_last(&cirr->executing, &cirr->pending, &operand->item);
			cirr->asr->signal = MP_TASK_SIG_PENDING;

			cirr->disableRX(cirr);
			cirr->disableTX(cirr);
		}

	}

	return;
}

MP_TASK(mp_regMaster_i2c_asr) {
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

		/* run ginit */
		_mp_regMaster_gfini(cirr->kernel);

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
			/* send slave address*/
			mp_i2c_setSlaveAddress(cirr->i2c, cur->slaveAddress);

			/* change mode and here we go */
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




static void _mp_regMaster_spi_enableRX(mp_regMaster_t *cirr) {
	mp_spi_enable_rx(cirr->spi);
}

static void _mp_regMaster_spi_disableRX(mp_regMaster_t *cirr) {
	mp_spi_disable_rx(cirr->spi);
}

static void _mp_regMaster_spi_enableTX(mp_regMaster_t *cirr) {
	mp_spi_enable_tx(cirr->spi);
}

static void _mp_regMaster_spi_disableTX(mp_regMaster_t *cirr) {
	mp_spi_disable_tx(cirr->spi);
}

static void _mp_regMaster_spi_interrupt(mp_spi_t *spi, mp_spi_flag_t flag) {
	mp_regMaster_t *cirr = spi->user;
	mp_regMaster_op_t *operand;
	int rest;

	/* get first operand */
	operand = cirr->pending.first ? cirr->pending.first->user : NULL;
	if(!operand)
		return;

	/* send registers */
	if(operand->state == MP_REGMASTER_STATE_TX && flag == MP_SPI_FL_TX) {

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
				/* switch buffer into ASR space */
				mp_list_switch_last(&cirr->executing, &cirr->pending, &operand->item);
				cirr->asr->signal = MP_TASK_SIG_PENDING;

				cirr->disableTX(cirr);
			}
		}
		else
			mp_spi_tx(spi, operand->reg[operand->regPos++]);

	}

	/* read data, CTR and start has already been sent */
	else if(operand->state == MP_REGMASTER_STATE_RX && flag == MP_SPI_FL_RX) {

		rest = operand->waitSize-operand->waitPos-1;

		if(!operand->swap)
			operand->wait[operand->waitPos++] = mp_spi_rx(spi);
		else {
			operand->wait[rest] = mp_spi_rx(spi);
			operand->waitPos++;
		}

		if(rest == 0) {
			/* switch buffer into ASR space */
			mp_list_switch_last(&cirr->executing, &cirr->pending, &operand->item);
			cirr->asr->signal = MP_TASK_SIG_PENDING;

			cirr->disableRX(cirr);
			cirr->disableTX(cirr);
		}

	}

	return;
}
