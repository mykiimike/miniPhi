#ifndef _HAVE_MP_BUTTON_H
	#define _HAVE_MP_BUTTON_H

	#ifdef _SUPPORT_BUTTON
		typedef struct mp_button_s mp_button_t;
		typedef struct mp_button_event_s mp_button_event_t;

		typedef void (*mp_button_on_t)(void *user);
		typedef void (*mp_button_event_on_t)(void *user);

		struct mp_button_s {
			/** user callback */
			mp_button_on_t onSwitch;

			/** user pointer */
			void *user;

			/** is pressed YES/NO */
			char pressed;

			/** how many time it was pressed */
			unsigned long pressDelay;

			/** GPIO port */
			mp_gpio_port_t *gpio;

			/** list of sub button events */
			mp_list_t events;
		};

		struct mp_button_event_s {
			/** configured delay */
			int delay;

			/** configured time count */
			int time;

			/** applyied callback */
			mp_button_event_on_t cb;

			/** user pointer */
			void *user;

			/** how many down time */
			int howManyDown;

			/** internal delay between each button down */
			unsigned long downDelay;

			/** button link pointer */
			mp_button_t *button;

			/** list link with button */
			mp_list_item_t item;
		};


		mp_ret_t mp_button_init(mp_button_t *button, unsigned int port, unsigned int pin, char *who);
		mp_ret_t mp_button_fini(mp_button_t *button);

		mp_ret_t mp_button_event_create(
			mp_button_t *button, mp_button_event_t *bac,
			int delay, int time, mp_button_event_on_t cb, void *user
		);
		mp_ret_t mp_button_event_destroy(mp_button_event_t *bac);
	#endif

#endif
