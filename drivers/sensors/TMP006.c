/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2014  Michael VERGOZ                                      *
 * Copyright (C) 2014  VERMAN                                              *
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

#ifdef SUPPORT_DRV_TMP006

static unsigned short mp_drv_TMP006_read(mp_drv_TMP006_t *TMP006, unsigned char address);
static void mp_drv_TMP006_write(mp_drv_TMP006_t *TMP006, unsigned char address, unsigned short writeByte);
static float mp_drv_TMP006_readObjTempC(mp_drv_TMP006_t *TMP006);
static void _mp_drv_TMP006_onDRDY(void *user);

MP_TASK(_mp_drv_TMP006_ASR);

/**
@defgroup mpDriverTiTMP006 Ti TMP006

@ingroup mpDriver

@brief Ti TMP006 Object temperature

@version 1.0.0

@author @htmlonly &copy; @endhtmlonly 2015
Michael Vergoz <mv@verman.fr>

@date 03 Feb 2015


Configuration for TMP006 example :
@li gate = USCI_B3
@li SDA = 10.1 / ext 1-17
@li SCL = 10.2 / ext 1-16
@li DRDY = 1.1 / ext 2-5

Initializing the driver :
@code
typedef struct olimex_msp430_s olimex_msp430_t;

struct olimex_msp430_s {
	mp_kernel_t kernel;

	mp_drv_TMP006_t tmp006;

};

// [...]
{
	mp_options_t options[] = {
		{ "gate", "USCI_B3" },
		{ "sda", "p10.1" },
		{ "clk", "p10.2" },
		{ "drdy", "p1.1" },
		{ NULL, NULL }
	};

	mp_drv_TMP006_init(&olimex->kernel, &olimex->tmp006, options, "Ti TMP006");
}
@endcode

@{
*/


mp_sensor_t *mp_drv_TMP006_init(mp_kernel_t *kernel, mp_drv_TMP006_t *TMP006, mp_options_t *options, char *who) {
	unsigned short info1, info2;
	char *value;
	mp_ret_t ret;

	memset(TMP006, 0, sizeof(*TMP006));
	TMP006->kernel = kernel;

	/* drdy */
	value = mp_options_get(options, "drdy");
	if(value) {
		TMP006->drdy = mp_gpio_text_handle(value, "TMP006 DRDY");
		if(!TMP006->drdy) {
			mp_printk("TMP006: need a valid DRDY port");
			mp_drv_TMP006_fini(TMP006);
			return(NULL);
		}

		/* set CS high */
		mp_gpio_direction(TMP006->drdy, MP_GPIO_INPUT);
	}
	else
		TMP006->drdy = NULL;

	/* open spi */
	ret = mp_i2c_open(kernel, &TMP006->i2c, options, "TMP006");
	if(ret == FALSE)
		return(NULL);

	mp_options_t setup[] = {
		{ "frequency", "400000" },
		{ "role", "master" },
		{ NULL, NULL }
	};
	ret = mp_i2c_setup(&TMP006->i2c, setup);
	if(ret == FALSE) {
		mp_i2c_close(&TMP006->i2c);
		return(NULL);
	}

	/* set slave address */
	mp_i2c_setSlaveAddress(&TMP006->i2c, 0x40);

	/* enable chip */
	if(TMP006->drdy) {
		/* install drdy interrupt high > low */
		ret = mp_gpio_interrupt_set(TMP006->drdy, _mp_drv_TMP006_onDRDY, TMP006, who);
		if(ret == FALSE) {
			mp_gpio_release(TMP006->drdy);
			return(NULL);
		}
		mp_gpio_interrupt_hi2lo(TMP006->drdy);

		mp_drv_TMP006_write(
			TMP006, TMP006_REG_WRITE_REG,
			TMP006_CFG_RESET
		);

		/* enable */
		mp_drv_TMP006_write(
			TMP006, TMP006_REG_WRITE_REG,
			TMP006_CFG_MODEON + TMP006_CFG_8SAMPLE + TMP006_CFG_DRDYEN
		);

	}
	else {
		mp_printk("TMP006 require DRDY interrupt for the moment");
		mp_i2c_close(&TMP006->i2c);
		return(NULL);
	}

	/* create ASR task */
	TMP006->task = mp_task_create(&kernel->tasks, "TMP006 ASR", _mp_drv_TMP006_ASR, TMP006, 0);
	TMP006->task->signal = MP_TASK_SIG_SLEEP;

	/* create sensor */
	TMP006->sensor = mp_sensor_register(kernel, MP_SENSOR_TEMPERATURE, who);

	/* check for communication */
	info1 = mp_drv_TMP006_read(TMP006, TMP006_REG_MAN_ID);
	info2 = mp_drv_TMP006_read(TMP006, TMP006_REG_DEVICE_ID);

	mp_printk("Loading TMP006 driver manId=0x%x deviceId=0x%x", info1, info2);

	return(TMP006->sensor);
}

void mp_drv_TMP006_fini(mp_drv_TMP006_t *TMP006) {
	mp_printk("Unloading TMP006 driver");

	mp_task_destroy(TMP006->task);
}


void mp_drv_TMP006_sleep(mp_drv_TMP006_t *TMP006) {
	unsigned int settings;

	/* Read current settings */
	settings = mp_drv_TMP006_read(TMP006, TMP006_REG_WRITE_REG);

	/* Power-up TMP006 */
	settings &= ~(TMP006_CFG_MODEON);

	mp_drv_TMP006_write(TMP006, TMP006_REG_WRITE_REG, settings);
}

void mp_drv_TMP006_wakeUp(mp_drv_TMP006_t *TMP006) {
	unsigned int settings;

	/* Read current settings */
	settings = mp_drv_TMP006_read(TMP006, TMP006_REG_WRITE_REG);

	/* Power-up TMP006 */
	settings |= TMP006_CFG_MODEON;

	mp_drv_TMP006_write(TMP006, TMP006_REG_WRITE_REG, settings);
}

/**@}*/

static unsigned short mp_drv_TMP006_read(mp_drv_TMP006_t *TMP006, unsigned char address) {
	unsigned short val = 0;

	mp_i2c_mode(&TMP006->i2c, 1);
	mp_i2c_txStart(&TMP006->i2c);

	/* write register */
	mp_i2c_tx(&TMP006->i2c, address);
	mp_i2c_waitTX(&TMP006->i2c);

	/* no stop let do a restart */
	mp_i2c_mode(&TMP006->i2c, 0); /* receiver */
	mp_i2c_txStart(&TMP006->i2c); /* start */

	/* get a char */
	mp_i2c_waitRX(&TMP006->i2c);
	val = mp_i2c_rx(&TMP006->i2c)<<8;

	/* prepare stop */
	mp_i2c_txStop(&TMP006->i2c);

	/* and receive last char */
	mp_i2c_waitRX(&TMP006->i2c);
	val |= mp_i2c_rx(&TMP006->i2c);

	/* Return val */
	return val;
}


static void mp_drv_TMP006_write(mp_drv_TMP006_t *TMP006, unsigned char address, unsigned short writeByte) {
    /* I2C TX, start condition
     * This will also send out slave address
     */
	mp_i2c_mode(&TMP006->i2c, 1);
	mp_i2c_txStart(&TMP006->i2c);

    /* Send pointer byte */
    mp_i2c_tx(&TMP006->i2c, address);

    /* Wait for TX buffer to empty */
    mp_i2c_waitTX(&TMP006->i2c);

    /* Send MSB byte */
    mp_i2c_tx(&TMP006->i2c, (unsigned char)(writeByte>>8));

    /* Wait for TX buffer to empty */
    mp_i2c_waitTX(&TMP006->i2c);

    /* Send LSB byte */
    mp_i2c_tx(&TMP006->i2c, (unsigned char)(writeByte&0x0F));

    /* Wait for TX buffer to empty */
    mp_i2c_waitTX(&TMP006->i2c);

    mp_i2c_txStop(&TMP006->i2c);
}

static unsigned short mp_drv_TMP006_readRawDieTemperature(mp_drv_TMP006_t *TMP006) {
	unsigned short raw = mp_drv_TMP006_read(TMP006, TMP006_REG_TABT);
	raw >>= 2;
	return(raw);
}

static unsigned short mp_drv_TMP006_readRawVoltage(mp_drv_TMP006_t *TMP006) {
	unsigned short raw = mp_drv_TMP006_read(TMP006, TMP006_REG_VOBJ);
	return(raw);
}

static float mp_drv_TMP006_readObjTempC(mp_drv_TMP006_t *TMP006) {
	float Tdie = mp_drv_TMP006_readRawDieTemperature(TMP006);
	float Vobj = mp_drv_TMP006_readRawVoltage(TMP006);

	Vobj *= 156.25;  // 156.25 nV per LSB
	Vobj /= 1000; // nV -> uV
	Vobj /= 1000; // uV -> mV
	Vobj /= 1000; // mV -> V
	Tdie *= 0.03125; // convert to celsius
	Tdie += 273.15; // convert to kelvin

	float tdie_tref = Tdie - TMP006_TREF;
	float S = (1 + TMP006_A1*tdie_tref +
		 TMP006_A2*tdie_tref*tdie_tref);
	S *= TMP006_S0;
	S /= 10000000;
	S /= 10000000;

	float Vos = TMP006_B0 + TMP006_B1*tdie_tref +
		TMP006_B2*tdie_tref*tdie_tref;

	float fVobj = (Vobj - Vos) + TMP006_C2*(Vobj-Vos)*(Vobj-Vos);

	float Tobj = sqrt(sqrt(Tdie * Tdie * Tdie * Tdie + fVobj/S));

	Tobj -= 273.15; // Kelvin -> *C

	return(Tobj);
}

static void _mp_drv_TMP006_onDRDY(void *user) {
	mp_drv_TMP006_t *TMP006 = user;

	/* reroute asr */
	TMP006->task->signal = MP_TASK_SIG_PENDING;

}

MP_TASK(_mp_drv_TMP006_ASR) {
	mp_drv_TMP006_t *TMP006 = task->user;

	/* acknowledge task end */
	if(task->signal == MP_TASK_SIG_STOP) {
		mp_sensor_unregister(TMP006->kernel, TMP006->sensor);
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	/* update sensor */
	TMP006->sensor->temperature.result = mp_drv_TMP006_readObjTempC(TMP006);

	TMP006->task->signal = MP_TASK_SIG_SLEEP;
}

#endif

