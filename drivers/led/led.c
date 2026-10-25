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

#ifdef SUPPORT_COMMON_DEVICE
static void _led_publish(mp_kernel_t *kernel, mp_drv_led_t *led, char *who);
#endif

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

#ifdef SUPPORT_COMMON_DEVICE
	_led_publish(kernel, led, who);
#endif
    return(TRUE);
}

mp_ret_t mp_drv_led_fini(mp_drv_led_t *led) {
	if(led->gpio == NULL)
		return(FALSE);

	mp_printk("Terminating LED control on %s", led->gpio->who);

#ifdef SUPPORT_COMMON_DEVICE
	mp_device_registry_unregister(&led->device);
	mp_device_driver_unregister(led->kernel, &led->binding);
#endif

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

#ifdef SUPPORT_COMMON_DEVICE
static void _led_face_set(mp_device_led_t *face, int on) {
	mp_drv_led_t *led = face->user;

	if(on)
		mp_drv_led_turnOn(led);
	else
		mp_drv_led_turnOff(led);
	face->state = on ? 1 : 0;
}

static void _led_face_turn(mp_device_led_t *face) {
	mp_drv_led_t *led = face->user;

	mp_drv_led_turn(led);
	face->state = led->state == ON ? 1 : 0;
}

static void _led_publish(mp_kernel_t *kernel, mp_drv_led_t *led, char *who) {
	static const mp_device_driver_ops_t ops = {
		"led/gpio", "GPIO LED", "1.0.0",
		(unsigned int)(1u << MP_DEVICE_LED),
		NULL, NULL, NULL, NULL, NULL
	};
	const char *serial = who && who[0] ? who : "led";
	const char *label = who && who[0] ? who : "GPIO LED";

	led->kernel = kernel;
	led->face.set = _led_face_set;
	led->face.turn = _led_face_turn;
	led->face.user = led;
	if(mp_device_driver_register(kernel, &led->binding, &ops, led, NULL) == TRUE &&
		mp_device_registry_register(&led->binding, &led->device, MP_DEVICE_LED, 0, &led->face, "led", serial, NULL))
		led->device.label = label;
	mp_device_identify(led, label, "led", serial, NULL);
}

static mp_ret_t _led_device_open(mp_kernel_t *kernel, void *self, mp_options_t *options, char *who) {
	return(mp_drv_led_init(kernel, self, options, who));
}

/* a closed LED is off */
static void _led_device_close(void *self) {
	mp_drv_led_turnOff(self);
	mp_drv_led_fini(self);
}

static void _led_device_set(void *self, int value) {
	mp_drv_led_t *led = self;

	if(value)
		mp_drv_led_turnOn(led);
	else
		mp_drv_led_turnOff(led);
	led->face.state = value ? 1 : 0;
}

const mp_device_ops_t mp_drv_led_device = {
	"LED", MP_DEVICE_LED, _led_device_open, _led_device_close, NULL, _led_device_set,
	"GPIO LED", "1.0.0"
};
#endif

#endif
