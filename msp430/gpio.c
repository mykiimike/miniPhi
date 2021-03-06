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

static void _receive_port_interrupt(void *user);

mp_gpio_port_t __ports[100];
unsigned int __ports_idx = 1;

void mp_gpio_init() {
	memset(&__ports, 0, sizeof(__ports));

#ifdef __MSP430_HAS_PASEL__
	mp_gpio_port_t *p;
	mp_gpio_port_t *i;
	int a;
#endif

#ifdef __MSP430_HAS_PASEL__
	i = &__ports[__ports_idx];
	for(a=0; a<8; a++) {
		p = &__ports[__ports_idx];
		p->base = PA_BASE;
		p->port = 1;
		p->pin = a;
		p->used = NO;
		p->isr = PORT1_VECTOR;
		__ports_idx++;
	}
	mp_interrupt_set(PORT1_VECTOR, _receive_port_interrupt, i, "PORT 1");

	i = &__ports[__ports_idx];
	for(a=0; a<8; a++) {
		p = &__ports[__ports_idx];
		p->base = PA_BASE;
		p->port = 2;
		p->pin = a;
		p->used = NO;
		p->isr = PORT2_VECTOR;
		__ports_idx++;
	}
	mp_interrupt_set(PORT2_VECTOR, _receive_port_interrupt, i, "PORT 2");
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

mp_gpio_port_t *mp_gpio_handle(unsigned int port, unsigned int slot, char *who) {
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

/* format pX.X */
mp_gpio_port_t *mp_gpio_text_handle(char *text, char *who) {
	unsigned int buffer_size;
	unsigned int port = 0;
	unsigned int pin = 0;
	mp_gpio_port_t *gpio;
	char buffer[8];
	int len;
	int a;

	len = strlen(text);
	if(len < 4 || len > sizeof(buffer))
		return(NULL);

	/* get port */
	buffer_size = 0;
	for(a=1, text++; a<len; a++, text++) {
		if(*text == '.') {
			buffer[buffer_size] = '\0';
			buffer_size++;

			port = atoi(buffer);
			a++; text++;
			break;
		}
		else {
			buffer[buffer_size] = *text;
			buffer_size++;
		}
	}

	/* get pin */
	buffer_size = 0;
	for(; a<len; a++, text++) {
		buffer[buffer_size] = *text;
		buffer_size++;
	}
	buffer[buffer_size] = '\0';
	buffer_size++;

	pin = atoi(buffer);

	gpio = mp_gpio_handle(port, pin, who);
	return(gpio);
}

mp_ret_t mp_gpio_release(mp_gpio_port_t *port) {
	port->used = NO;
	port->who = NULL;

	_GPIO_REG8(port, _GPIO_DIR) &= ~(1<<port->pin);

	if(port->reverse == YES)
		_GPIO_REG8(port, _GPIO_OUT) |= 1<<port->pin;
	else
		_GPIO_REG8(port, _GPIO_OUT) &= ~(1<<port->pin);

	/* remove callback */
	port->callback = NULL;

	return(TRUE);
}

mp_ret_t mp_gpio_direction(mp_gpio_port_t *port, mp_gpio_direction_t direction) {

	if(direction == MP_GPIO_INPUT)
		_GPIO_REG8(port, _GPIO_DIR) &= ~(1<<port->pin);
	else
		_GPIO_REG8(port, _GPIO_DIR) |= 1<<port->pin;

	port->direction = direction;
	return(TRUE);
}



mp_ret_t mp_gpio_interrupt_set(mp_gpio_port_t *port, mp_interrupt_cb_t in, void *user, char *who) {
	/* not interruptible */
	if(port->isr == 0)
		return(FALSE);

	port->callback = in;
	port->user = user;

	_GPIO_REG8(port, _GPIO_SEL) &= ~(1<<port->pin);

	/* set port input */
	mp_gpio_direction(port, MP_GPIO_INPUT);

	/* interrupt enabled */
	//_GPIO_REG8(port, _GPIO_IE) |= 1<<port->pin;

	/* IFG cleared */
	_GPIO_REG8(port, _GPIO_IFG) &= ~(1<<port->pin);

	return(TRUE);
}

mp_ret_t mp_gpio_interrupt_unset(mp_gpio_port_t *port) {

	/* clear interrupt enabled */
	_GPIO_REG8(port, _GPIO_IE) &= ~(1<<port->pin);

	/* IFG cleared */
	_GPIO_REG8(port, _GPIO_IFG) &= ~(1<<port->pin);

	return(TRUE);
}

mp_bool_t mp_gpio_read(mp_gpio_port_t *port) {
	mp_bool_t r = !(_GPIO_REG8(port, _GPIO_IN) & 0x10) ? ON : OFF;
	return(r);
}


void mp_gpio_set(mp_gpio_port_t *port) {
	if(port->used != YES && port->direction == MP_GPIO_OUTPUT)
		return;
	if(port->reverse == YES)
		_GPIO_REG8(port, _GPIO_OUT) &= ~(1<<port->pin);
	else
		_GPIO_REG8(port, _GPIO_OUT) |= 1<<port->pin;
}

void mp_gpio_unset(mp_gpio_port_t *port) {
	if(port->used != YES && port->direction == MP_GPIO_OUTPUT)
		return;
	if(port->reverse == YES)
		_GPIO_REG8(port, _GPIO_OUT) |= 1<<port->pin;
	else
		_GPIO_REG8(port, _GPIO_OUT) &= ~(1<<port->pin);
}

void mp_gpio_turn(mp_gpio_port_t *port) {
	if(port->used != YES && port->direction == MP_GPIO_OUTPUT)
		return;
	_GPIO_REG8(port, _GPIO_OUT) ^= 1<<port->pin;
}

static void _receive_port_interrupt(void *user) {
	mp_gpio_port_t *portBase = user;
	mp_gpio_port_t *port;
	char ifg = _GPIO_REG8(portBase, _GPIO_IFG);
	int a;

	for(a=0; a<8; a++) {
		if(ifg & 1<<a) {
			port = portBase+a;
			if(port->callback)
				port->callback(port->user);
			_GPIO_REG8(port, _GPIO_IFG) &= ~(1<<port->pin);
		}
	}

}
