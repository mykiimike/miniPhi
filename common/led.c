#include <mp.h>


mp_ret_t mp_led_init(mp_led_t *led, unsigned char port, unsigned char pin, char *who) {
    /* allocate GPIO */
    led->gpio = mp_gpio_handle(port, pin, who);
    if(!led->gpio)
    	return(FALSE);

    /* set direction */
    mp_gpio_direction(led->gpio, MP_GPIO_OUTPUT);

    /* set to off */
    mp_gpio_unset(led->gpio);
    led->state = OFF;
    return(TRUE);
}

mp_ret_t mp_led_fini(mp_led_t *led) {
	if(led->gpio == NULL)
		return(FALSE);

	/* release GPIO port */
	mp_gpio_release(led->gpio);

	return(TRUE);
}

void mp_led_turnOff(mp_led_t *led) {
    mp_gpio_unset(led->gpio);
    led->state = OFF;
}

void mp_led_turnOn(mp_led_t *led) {
    mp_gpio_set(led->gpio);
    led->state = ON;

}

void mp_led_turn(mp_led_t *led) {
	mp_gpio_turn(led->gpio);
	led->state = led->state == ON ? OFF : ON;
}
