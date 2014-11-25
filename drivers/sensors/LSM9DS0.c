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

/*
 * Some parts of this source has been ported from Kris Winer code's.
 * https://github.com/kriswiner/LSM9DS0
 */

#include <mp.h>

#ifdef SUPPORT_DRV_LSM9DS0

static void __on_int1(void *user);
static void __on_int2(void *user);
static void __on_intG(void *user);
static void __on_drdy(void *user);

mp_ret_t mp_drv_LSM9DS0_init(mp_kernel_t *kernel, mp_drv_LSM9DS0_t *LSM9DS0, mp_options_t *options, char *who) {
	char *value;
	mp_ret_t ret;

	memset(LSM9DS0, 0, sizeof(*LSM9DS0));

	/* csG */
	value = mp_options_get(options, "csG");
	if(!value) {
		mp_printk("LSM9DS0: need csG port");
		return(FALSE);
	}
	LSM9DS0->csG = mp_gpio_text_handle(value, "LSM9DS0 csG");
    if(!LSM9DS0->csG) {
    	mp_printk("LSM9DS0: need csG port");
    	mp_drv_LSM9DS0_fini(LSM9DS0);
		return(FALSE);
	}

	/* csXM */
	value = mp_options_get(options, "csXM");
	if(!value) {
		mp_printk("LSM9DS0: need csXM port");
		return(FALSE);
	}
	LSM9DS0->csXM = mp_gpio_text_handle(value, "LSM9DS0 csXM");
    if(!LSM9DS0->csXM) {
    	mp_printk("LSM9DS0: need csXM port");
    	mp_drv_LSM9DS0_fini(LSM9DS0);
		return(FALSE);
	}

	/* int1 */
	value = mp_options_get(options, "int1");
	if(value) {
		LSM9DS0->int1 = mp_gpio_text_handle(value, "LSM9DS0 int1");
		if(!LSM9DS0->int1) {
			mp_printk("LSM9DS0: need int1 port");
			mp_drv_LSM9DS0_fini(LSM9DS0);
			return(FALSE);
		}
	}

	/* int2 */
	value = mp_options_get(options, "int2");
	if(value) {
		LSM9DS0->int2 = mp_gpio_text_handle(value, "LSM9DS0 int2");
		if(!LSM9DS0->int2) {
			mp_printk("LSM9DS0: need int2 port");
			mp_drv_LSM9DS0_fini(LSM9DS0);
			return(FALSE);
		}
	}

	/* intG */
	value = mp_options_get(options, "intG");
	if(value) {
		LSM9DS0->intG = mp_gpio_text_handle(value, "LSM9DS0 intG");
		if(!LSM9DS0->intG) {
			mp_printk("LSM9DS0: need intG port");
			mp_drv_LSM9DS0_fini(LSM9DS0);
			return(FALSE);
		}
	}

	/* drdy */
	value = mp_options_get(options, "drdy");
	if(value) {
		LSM9DS0->drdy = mp_gpio_text_handle(value, "LSM9DS0 drdy");
		if(!LSM9DS0->drdy) {
			mp_printk("LSM9DS0: need drdy port");
			mp_drv_LSM9DS0_fini(LSM9DS0);
			return(FALSE);
		}
	}

	/* set CS high */
	mp_gpio_direction(LSM9DS0->csG, MP_GPIO_OUTPUT);
	mp_gpio_direction(LSM9DS0->csXM, MP_GPIO_OUTPUT);
	mp_gpio_set(LSM9DS0->csG);
	mp_gpio_set(LSM9DS0->csXM);

	/* open spi */
	ret = mp_spi_open(kernel, &LSM9DS0->spi, options, "LSM9DS0");
	if(ret == FALSE)
		return(FALSE);

	mp_options_t setup[] = {
			{ "frequency", "10000000" },
			{ "phase", "capture" },
			{ "polarity", "high" },
			{ "first", "MSB" },
			{ "role", "master" },
			{ "bit", "8" },
			{ "flow", "sync" },
			{ NULL, NULL }
	};
	ret = mp_spi_setup(&LSM9DS0->spi, setup);
	if(ret == FALSE) {
		mp_spi_close(&LSM9DS0->spi);
		return(FALSE);
	}

	mp_drv_LSM9DS0_start(LSM9DS0);

	mp_printk("Initialize LSM9DS0 SPI bind");
	return(TRUE);
}


mp_ret_t mp_drv_LSM9DS0_fini(mp_drv_LSM9DS0_t *LSM9DS0) {

	mp_drv_LSM9DS0_stop(LSM9DS0);

	if(LSM9DS0->csG)
		mp_gpio_release(LSM9DS0->csG);

	if(LSM9DS0->csXM)
		mp_gpio_release(LSM9DS0->csXM);

	if(LSM9DS0->int1)
		mp_gpio_release(LSM9DS0->int1);

	if(LSM9DS0->int2)
		mp_gpio_release(LSM9DS0->int2);

	if(LSM9DS0->intG)
		mp_gpio_release(LSM9DS0->intG);

	if(LSM9DS0->drdy)
		mp_gpio_release(LSM9DS0->drdy);

	mp_spi_close(&LSM9DS0->spi);

	mp_printk("Stopping LSM9DS0 SPI bind");
	return(TRUE);
}


mp_ret_t mp_drv_LSM9DS0_start(mp_drv_LSM9DS0_t *LSM9DS0) {

	/* setup interrupts */
	mp_gpio_interrupt_set(LSM9DS0->int1, __on_int1, LSM9DS0, "LSM9DS0 INT1");
	mp_gpio_interrupt_set(LSM9DS0->int2, __on_int2, LSM9DS0, "LSM9DS0 INT2");
	mp_gpio_interrupt_set(LSM9DS0->intG, __on_intG, LSM9DS0, "LSM9DS0 INTG");
	mp_gpio_interrupt_set(LSM9DS0->drdy, __on_drdy, LSM9DS0, "LSM9DS0 DRDY");

	return(TRUE);
}

mp_ret_t mp_drv_LSM9DS0_stop(mp_drv_LSM9DS0_t *LSM9DS0) {

	/* remove interrupts */
	mp_gpio_interrupt_unset(LSM9DS0->int1);
	mp_gpio_interrupt_unset(LSM9DS0->int2);
	mp_gpio_interrupt_unset(LSM9DS0->intG);
	mp_gpio_interrupt_unset(LSM9DS0->drdy);


}




static void __on_int1(void *user) {
	mp_drv_LSM9DS0_t *LSM9DS0 = user;
	P10OUT ^= 0x40;

}

static void __on_int2(void *user) {
	mp_drv_LSM9DS0_t *LSM9DS0 = user;

}

static void __on_intG(void *user) {
	mp_drv_LSM9DS0_t *LSM9DS0 = user;

}

static void __on_drdy(void *user) {
	mp_drv_LSM9DS0_t *LSM9DS0 = user;

}




#endif

