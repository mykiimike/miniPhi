
#define SUPPORT_DRV_LED

#ifdef SUPPORT_DRV_LED

#ifndef _HAVE_MP_DRV_LED_H
#define _HAVE_MP_DRV_LED_H

	typedef struct mp_drv_led_s mp_drv_led_t;

	struct mp_drv_led_s {
		mp_gpio_port_t *gpio;

		mp_bool_t state;

		/** in some bad case the set/unset must be reverted */
		mp_bool_t reverted;
	};

	mp_ret_t mp_drv_led_init(mp_kernel_t *kernel, mp_drv_led_t *led, mp_options_t *options, char *who);
	mp_ret_t mp_drv_led_fini(mp_drv_led_t *led);
	void mp_drv_led_turnOff(mp_drv_led_t *led);
	void mp_drv_led_turnOn(mp_drv_led_t *led);
	void mp_drv_led_turn(mp_drv_led_t *led);

#endif

#endif
