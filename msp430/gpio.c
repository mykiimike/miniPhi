#include <mp.h>

mp_gpio_port_t __ports[100];
unsigned int __ports_idx = 1;

void mp_gpio_init() {
	memset(&__ports, 0, sizeof(__ports));

#ifdef __MSP430_HAS_PASEL__
	mp_gpio_port_t *p;
	int a;
#endif

#ifdef __MSP430_HAS_PASEL__
	for(a=0; a<8; a++) {
		p = &__ports[__ports_idx];
		p->base = PA_BASE;
		p->port = 1;
		p->pin = a;
		p->used = NO;
		p->isr = PORT1_VECTOR;
		__ports_idx++;
	}

	for(a=0; a<8; a++) {
		p = &__ports[__ports_idx];
		p->base = PA_BASE;
		p->port = 2;
		p->pin = a;
		p->used = NO;
		p->isr = PORT2_VECTOR;
		__ports_idx++;
	}
#endif

#ifdef __MSP430_HAS_PBSEL__
	for(a=0; a<8; a++) {
		p = &__ports[__ports_idx];
		p->base = PB_BASE;
		p->port = 3;
		p->pin = a;
		p->used = NO;
		__ports_idx++;
	}

	for(a=0; a<8; a++) {
		p = &__ports[__ports_idx];
		p->base = PB_BASE;
		p->port = 4;
		p->pin = a;
		p->used = NO;
		__ports_idx++;
	}
#endif

#ifdef __MSP430_HAS_PCSEL__
	for(a=0; a<8; a++) {
		p = &__ports[__ports_idx];
		p->base = PC_BASE;
		p->port = 5;
		p->pin = a;
		p->used = NO;
		__ports_idx++;
	}

	for(a=0; a<8; a++) {
		p = &__ports[__ports_idx];
		p->base = PC_BASE;
		p->port = 6;
		p->pin = a;
		p->used = NO;
		__ports_idx++;
	}
#endif

#ifdef __MSP430_HAS_PDSEL__
	for(a=0; a<8; a++) {
		p = &__ports[__ports_idx];
		p->base = PD_BASE;
		p->port = 7;
		p->pin = a;
		p->used = NO;
		__ports_idx++;
	}

	for(a=0; a<8; a++) {
		p = &__ports[__ports_idx];
		p->base = PD_BASE;
		p->port = 8;
		p->pin = a;
		p->used = NO;
		__ports_idx++;
	}
#endif

#ifdef __MSP430_HAS_PESEL__
	for(a=0; a<8; a++) {
		p = &__ports[__ports_idx];
		p->base = PE_BASE;
		p->port = 9;
		p->pin = a;
		p->used = NO;
		__ports_idx++;
	}

	for(a=0; a<8; a++) {
		p = &__ports[__ports_idx];
		p->base = PE_BASE;
		p->port = 10;
		p->pin = a;
		p->used = NO;
		__ports_idx++;
	}
#endif

#ifdef __MSP430_HAS_PFSEL__
	for(a=0; a<8; a++) {
		p = &__ports[__ports_idx];
		p->base = PF_BASE;
		p->port = 11;
		p->pin = a;
		p->used = NO;
		__ports_idx++;
	}

	for(a=0; a<8; a++) {
		p = &__ports[__ports_idx];
		p->base = PF_BASE;
		p->port = 12;
		p->pin = a;
		p->used = NO;
		__ports_idx++;
	}
#endif


}

void mp_gpio_fini() {

}

mp_gpio_port_t *mp_gpio_handle(unsigned char port, unsigned char slot, char *who) {
	unsigned int position;
	mp_gpio_port_t *porthdl;

	/* get port */
	position = (port*8-7)+slot;
	porthdl = &__ports[position];
	if(!porthdl)
		return(NULL);

	/* port inuse */
	porthdl->used = YES;
	porthdl->who = who;

	/* select the port */
	return(porthdl);
}

mp_ret_t mp_gpio_release(mp_gpio_port_t *port) {
	port->used = NO;
	port->who = NULL;


	_GPIO_REG8(port, _GPIO_DIR) &= ~(1<<port->pin);
	_GPIO_REG8(port, _GPIO_OUT) &= ~(1<<port->pin);

	return(TRUE);
}

mp_ret_t mp_gpio_direction(mp_gpio_port_t *port, mp_gpio_direction_t direction) {

	switch(direction) {
		case MP_GPIO_INPUT:
			_GPIO_REG8(port, _GPIO_DIR) &= ~(1<<port->pin);
			break;

		case MP_GPIO_OUTPUT:
			_GPIO_REG8(port, _GPIO_DIR) |= 1<<port->pin;
			break;

		default:
			return(FALSE);
	}
	port->direction = direction;
	return(TRUE);
}


mp_interrupt_t *mp_gpio_interrupt_set(mp_gpio_port_t *port, mp_interrupt_cb_t in, void *user, char *who) {
	mp_interrupt_t *inter;

	/* not interruptible */
	if(port->isr == 0)
		return(NULL);

	/* set port input */
	mp_gpio_direction(port, MP_GPIO_INPUT);

	/* place interrupt */
	inter = mp_interrupt_set(port->isr, in, user, who);

	/* interrupt enabled */
	_GPIO_REG8(port, _GPIO_IE) |= 1<<port->pin;

	/* Hi/lo edge */
	_GPIO_REG8(port, _GPIO_IES) |= 1<<port->pin;

	/* IFG cleared */
	_GPIO_REG8(port, _GPIO_IFG) &= ~(1<<port->pin);

	return(inter);
}

mp_ret_t mp_gpio_interrupt_unset(mp_gpio_port_t *port) {

	/* clear interrupt enabled */
	_GPIO_REG8(port, _GPIO_IE) &= ~(1<<port->pin);

	/* IFG cleared */
	_GPIO_REG8(port, _GPIO_IFG) &= ~(1<<port->pin);

	/* remove interrupt slot */
	mp_interrupt_unset(port->isr);

	return(TRUE);
}

mp_bool_t mp_gpio_read(mp_gpio_port_t *port) {
	mp_bool_t r = !(_GPIO_REG8(port, _GPIO_IN) & 0x10) ? ON : OFF;
	return(r);
}


void mp_gpio_set(mp_gpio_port_t *port) {
	if(port->used != YES && port->direction == MP_GPIO_OUTPUT)
		return;
	_GPIO_REG8(port, _GPIO_OUT) |= 1<<port->pin;
}

void mp_gpio_unset(mp_gpio_port_t *port) {
	if(port->used != YES && port->direction == MP_GPIO_OUTPUT)
		return;
	_GPIO_REG8(port, _GPIO_OUT) &= ~(1<<port->pin);
}

void mp_gpio_turn(mp_gpio_port_t *port) {
	if(port->used != YES && port->direction == MP_GPIO_OUTPUT)
		return;
	_GPIO_REG8(port, _GPIO_OUT) ^= 1<<port->pin;
}
