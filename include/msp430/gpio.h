#ifndef _HAVE_MSP430_GPIO_H
	#define _HAVE_MSP430_GPIO_H

	/* compatible API */
	typedef enum {
		MP_GPIO_INPUT = 1,
		MP_GPIO_OUTPUT,
	} mp_gpio_direction_t;

	typedef struct mp_gpio_pair_s mp_gpio_pair_t;
	typedef struct mp_gpio_port_s mp_gpio_port_t;

	struct mp_gpio_pair_s {
		unsigned int port;
		unsigned int pin;
	};

	struct mp_gpio_port_s {
		/* common part */

		/** port number */
		unsigned int port;

		/** pin number */
		unsigned int pin;

		/** who is the handler */
		char *who;

		/** pin used */
		mp_bool_t used;

		/** pin is interruptible */
		char direction;

		/** interrupt callback  */
		mp_interrupt_cb_t callback;

		/** callback user pointer */
		void *user;

		/* if YES then unset/set is reversed */
		char reverse;

		/* msp430 dependant */
		unsigned int base;

		/* msp430 dependant interrupt vector  */
		unsigned char isr;


	};


	void mp_gpio_init();
	void mp_gpio_fini();
	mp_gpio_port_t *mp_gpio_handle(unsigned int port, unsigned int slot, char *who);
	mp_ret_t mp_gpio_release(mp_gpio_port_t *port);
	mp_ret_t mp_gpio_direction(mp_gpio_port_t *port, mp_gpio_direction_t direction);
	mp_bool_t mp_gpio_read(mp_gpio_port_t *port);

	void mp_gpio_set(mp_gpio_port_t *port);
	void mp_gpio_unset(mp_gpio_port_t *port);
	void mp_gpio_turn(mp_gpio_port_t *port);

	mp_ret_t mp_gpio_interrupt_set(mp_gpio_port_t *port, mp_interrupt_cb_t in, void *user, char *who);
	mp_ret_t mp_gpio_interrupt_unset(mp_gpio_port_t *port);

	/* machine specs */
	#define _GPIO_IN    0x00
	#define _GPIO_OUT   0x02
	#define _GPIO_DIR   0x04
	#define _GPIO_REN   0x06
	#define _GPIO_DRIVE 0x08
	#define _GPIO_SEL   0x0a

	#define _GPIO_IES   0x18
	#define _GPIO_IE    0x1a
	#define _GPIO_IFG   0x1c

	#define _GPIO_REG8(_port, _type) \
		*((volatile char *)(_port->base+_type+((_port->port%2)^1)))

	static inline void mp_gpio_interrupt_lo2hi(mp_gpio_port_t *port) {
		_GPIO_REG8(port, _GPIO_IES) &= ~(1<<port->pin);
	}

	static inline void mp_gpio_interrupt_hi2lo(mp_gpio_port_t *port) {
		_GPIO_REG8(port, _GPIO_IES) |= 1<<port->pin;
	}

	static inline void mp_gpio_interrupt_hilo_switch(mp_gpio_port_t *port) {
		_GPIO_REG8(port, _GPIO_IES) ^= 1<<port->pin;
	}

#endif
