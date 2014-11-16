#ifndef _HAVE_MP_LED_H
#define _HAVE_MP_LED_H

	typedef struct mp_led_s mp_led_t;

	struct mp_led_s {
		mp_gpio_port_t *gpio;

		mp_bool_t state;
	};

	mp_ret_t mp_led_init(mp_led_t *led, unsigned char port, unsigned char pin, const char *who);
	mp_ret_t mp_led_fini(mp_led_t *led);
	void mp_led_turnOff(mp_led_t *led);
	void mp_led_turnOn(mp_led_t *led);
	void mp_led_turn(mp_led_t *led);

#endif
