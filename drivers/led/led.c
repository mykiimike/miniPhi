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

#include <mp.h>

#ifdef SUPPORT_DRV_LED

mp_ret_t mp_drv_led_init(mp_kernel_t *kernel, mp_drv_led_t *led, mp_options_t *options, char *who) {
	char *value, *port;

	port = value = mp_options_get(options, "port");
	if(!value)
		return(FALSE);

    /* allocate GPIO */
    led->gpio = mp_gpio_text_handle(value, who);
    if(!led->gpio)
    	return(FALSE);

    value = mp_options_get(options, "reverse");
    if(value && mp_options_cmp(value, "true"))
    	led->gpio->reverse = YES;

    /* set direction */
    mp_gpio_direction(led->gpio, MP_GPIO_OUTPUT);

    /* set to off */
    mp_gpio_unset(led->gpio);
    led->state = OFF;

    mp_printk("Initializing LED control on %s called %s", port, who);

    return(TRUE);
}

mp_ret_t mp_drv_led_fini(mp_drv_led_t *led) {
	if(led->gpio == NULL)
		return(FALSE);

	mp_printk("Terminating LED control on %s", led->gpio->who);

	/* release GPIO port */
	mp_gpio_release(led->gpio);


	return(TRUE);
}

void mp_drv_led_turnOff(mp_drv_led_t *led) {
    mp_gpio_unset(led->gpio);
    led->state = OFF;
}

void mp_drv_led_turnOn(mp_drv_led_t *led) {
    mp_gpio_set(led->gpio);
    led->state = ON;

}

void mp_drv_led_turn(mp_drv_led_t *led) {
	mp_gpio_turn(led->gpio);
	led->state = led->state == ON ? OFF : ON;
}

#endif
