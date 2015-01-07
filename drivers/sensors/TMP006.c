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
static void _mp_drv_TMP006_onDRDY(void *user);

mp_ret_t mp_drv_TMP006_init(mp_kernel_t *kernel, mp_drv_TMP006_t *TMP006, mp_options_t *options, char *who) {
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
			return(FALSE);
		}
	}
	else
		TMP006->drdy = NULL;

	/* set CS high */
	mp_gpio_direction(TMP006->drdy, MP_GPIO_INPUT);

	/* open spi */
	ret = mp_i2c_open(kernel, &TMP006->i2c, options, "TMP006");
	if(ret == FALSE)
		return(FALSE);

	mp_options_t setup[] = {
		{ "frequency", "10000" },
		{ "role", "master" },
		{ NULL, NULL }
	};
	ret = mp_i2c_setup(&TMP006->i2c, setup);
	if(ret == FALSE) {
		mp_i2c_close(&TMP006->i2c);
		return(FALSE);
	}

	/* set slave address */
	mp_i2c_setSlaveAddress(&TMP006->i2c, 0x40);

	/* enable chip */
	if(TMP006->drdy) {
		/* install drdy interrupt high > low */
		ret = mp_gpio_interrupt_set(TMP006->drdy, _mp_drv_TMP006_onDRDY, TMP006, who);
		if(ret == FALSE) {
			mp_gpio_release(TMP006->drdy);
			return(FALSE);
		}
		mp_gpio_interrupt_hi2lo(TMP006->drdy);

		/* enable */
		mp_drv_TMP006_write(
			TMP006, TMP006_P_WRITE_REG,
			TMP006_POWER_UP + TMP006_CR_0_5 + TMP006_EN
		);
	}
	else {
		/** \todo create a task */
		mp_printk("TMP006 require DRDY interrupt for the moment");
		mp_i2c_close(&TMP006->i2c);
		return(FALSE);
	}

	/* check for communication */
	info1 = mp_drv_TMP006_read(TMP006, TMP006_P_MAN_ID);
	info2 = mp_drv_TMP006_read(TMP006, TMP006_P_DEVICE_ID);

	mp_printk("Loading TMP006 driver manId=0x%x deviceId=0x%x", info1, info2);

	return(TRUE);
}

void mp_drv_TMP006_sleep(mp_drv_TMP006_t *TMP006) {
	unsigned int settings;

	/* Read current settings */
	settings = mp_drv_TMP006_read(TMP006, TMP006_P_WRITE_REG);

	/* Power-up TMP006 */
	settings &= ~(TMP006_POWER_UP);

	mp_drv_TMP006_write(TMP006, TMP006_P_WRITE_REG, settings);
}

void mp_drv_TMP006_wakeUp(mp_drv_TMP006_t *TMP006) {
	unsigned int settings;

	/* Read current settings */
	settings = mp_drv_TMP006_read(TMP006, TMP006_P_WRITE_REG);

	/* Power-up TMP006 */
	settings |= TMP006_POWER_UP;

	mp_drv_TMP006_write(TMP006, TMP006_P_WRITE_REG, settings);
}

void mp_drv_TMP006_fini(mp_drv_TMP006_t *TMP006) {
	mp_printk("Unloading TMP006 driver");
}


static unsigned short mp_drv_TMP006_read(mp_drv_TMP006_t *TMP006, unsigned char address) {
	unsigned short val = 0;

	mp_i2c_mode(&TMP006->i2c, 1);
	mp_i2c_txStart(&TMP006->i2c);

	/* write register */
	mp_i2c_tx(&TMP006->i2c, address);

	/* no stop let do a restart */
	mp_i2c_mode(&TMP006->i2c, 0); /* receiver */
	mp_i2c_txStart(&TMP006->i2c); /* start */

	/* get a char */
	val = mp_i2c_rx(&TMP006->i2c)<<8;

	/* prepare stop */
	mp_i2c_txStop(&TMP006->i2c);

	/* and receive last char */
	val |= mp_i2c_rx(&TMP006->i2c);

	/* Return val */
	return val;
}


static void mp_drv_TMP006_write(mp_drv_TMP006_t *TMP006, unsigned char address, unsigned short writeByte) {
	mp_i2c_mode(&TMP006->i2c, 1);
	mp_i2c_txStart(&TMP006->i2c);

	/* write chip address */
	mp_i2c_tx(&TMP006->i2c, address);

	/* MSB */
	mp_i2c_tx(&TMP006->i2c, (unsigned char)writeByte>>8);

	/* LSB */
	mp_i2c_tx(&TMP006->i2c, (unsigned char)writeByte);


	mp_i2c_txStop(&TMP006->i2c);

}

static void _mp_drv_TMP006_onDRDY(void *user) {
	P10OUT ^= 0x40;
}
#endif

