#include <mp.h>


mp_ret_t mp_rtc_init() {

}


mp_rtc_fini() {


}


typedef void (*mp_rtc_callback_t)(void *);

typedef struct mp_rtc_s mp_rtc_t;

struct mp_rtc_s {
	/* user definition */

	/** GPIO pair define */
	mp_gpio_pair_t device;

	/** ping direction */
	mp_gpio_direction_t direction;

	/** callback on direction input */
	mp_rtc_callback_t callback;

	/** user pointer used at callback */
	void *user;

	/* internal definition */

	/** gpio port */
	mp_gpio_port_t *_port;
};

mp_bool_t mp_rtc_create(mp_rtc_t *rtc, char *who) {
	if(rtc->device.port == 0 || rtc->device.pin == 0)
		return(FALSE);

	/* handle GPIO */
	rtc->_port = mp_gpio_handle(rtc->device.port, rtc->device.pin, who);
	if(rtc->_port == NULL)
		return(FALSE);

	/* configure GPIO */
	_GPIO_REG8(rtc->_port, _GPIO_SEL) &= ~(1<<rtc->device.pin);
	mp_gpio_direction(rtc->_port, rtc->direction);

	return(TRUE);
}

mp_bool_t mp_rtc_remove(mp_rtc_t *rtc) {

	/* release GPIO */
	mp_gpio_release(rtc->_port);

	return(TRUE);


}
