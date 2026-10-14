/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2026  Michael VERGOZ                                      *
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

/**
@defgroup mpArchSTM32GPIO GPIO and EXTI

@ingroup mpArchSTM32

@{
*/

static GPIO_TypeDef * const __bases[MP_ARCH_GPIO_PORTS] = {
	GPIOA, GPIOB, GPIOC, GPIOD,
#if MP_ARCH_GPIO_PORTS > 4
	GPIOE, GPIOF, GPIOG,
#endif
};
static mp_gpio_port_t __ports[MP_ARCH_GPIO_PORTS][16];

/* owner of each EXTI line */
static mp_gpio_port_t *__exti[16];

static void _mp_gpio_exti_dispatch(void *user);

/* the debug port keeps its pins and pulls */
static mp_bool_t _mp_gpio_debug(unsigned int port, unsigned int pin) {
	if(port == 0 && (pin == 13 || pin == 14))
		return(YES);
#ifndef _SWD_ONLY
	if((port == 0 && pin == 15) || (port == 1 && (pin == 3 || pin == 4)))
		return(YES);
#endif
	return(NO);
}

void mp_gpio_init() {
	unsigned int port, pin;

	RCC->APB2ENR |= RCC_APB2_AFIO | RCC_APB2_IOPA | RCC_APB2_IOPB | RCC_APB2_IOPC | RCC_APB2_IOPD;
#if MP_ARCH_GPIO_PORTS > 4
	RCC->APB2ENR |= RCC_APB2_IOPE | RCC_APB2_IOPF | RCC_APB2_IOPG;
#endif
	(void)RCC->APB2ENR;

#ifdef _SWD_ONLY
	/* keeps SW-DP, gives PA15, PB3 and PB4 back */
	AFIO->MAPR = (AFIO->MAPR & ~AFIO_MAPR_SWJ_CFG) | AFIO_MAPR_SWJ_SWD;
#endif

	memset(__ports, 0, sizeof(__ports));
	memset(__exti, 0, sizeof(__exti));

	for(port=0; port<MP_ARCH_GPIO_PORTS; port++) {
		for(pin=0; pin<16; pin++) {
			__ports[port][pin].port = port;
			__ports[port][pin].pin = pin;
			__ports[port][pin].base = __bases[port];

			/* an analog pin has no input trigger to leak through */
			if(_mp_gpio_debug(port, pin) == NO)
				mp_gpio_mode(&__ports[port][pin], MP_GPIO_MODE_ANALOG);
		}
	}

	EXTI->IMR = 0;
	EXTI->PR = 0xFFFFF;
}

void mp_gpio_fini() {
	unsigned int port, pin;

	for(port=0; port<MP_ARCH_GPIO_PORTS; port++)
		for(pin=0; pin<16; pin++)
			if(__ports[port][pin].used == YES)
				mp_gpio_release(&__ports[port][pin]);
}

mp_gpio_port_t *mp_gpio_handle(unsigned int port, unsigned int slot, char *who) {
	mp_gpio_port_t *gpio;

	if(port >= MP_ARCH_GPIO_PORTS || slot > 15)
		return(NULL);

	gpio = &__ports[port][slot];
	if(gpio->used == YES) {
		mp_printk("GPIO P%c%u is used by %s, %s can not have it", 'A' + port, slot, gpio->who, who);
		return(NULL);
	}

	gpio->used = YES;
	gpio->who = who;
	gpio->reverse = NO;
	gpio->callback = NULL;
	gpio->user = NULL;

	/* handed out as after reset, a floating input */
	if(_mp_gpio_debug(port, slot) == NO)
		mp_gpio_mode(gpio, MP_GPIO_MODE_INPUT);

	return(gpio);
}

mp_gpio_port_t *mp_gpio_text_handle(char *text, char *who) {
	unsigned int port;
	unsigned int pin;
	char *p = text;

	if(!p || (*p != 'p' && *p != 'P'))
		return(NULL);
	p++;

	if(*p >= 'a' && *p <= 'z')
		port = *p - 'a';
	else if(*p >= 'A' && *p <= 'Z')
		port = *p - 'A';
	else
		return(NULL);
	p++;

	if(*p == '.')
		p++;

	if(*p < '0' || *p > '9')
		return(NULL);

	pin = 0;
	while(*p >= '0' && *p <= '9')
		pin = pin * 10 + (*p++ - '0');

	if(*p != '\0')
		return(NULL);

	return(mp_gpio_handle(port, pin, who));
}

mp_ret_t mp_gpio_release(mp_gpio_port_t *port) {
	if(!port || port->used == NO)
		return(FALSE);

	if(__exti[port->pin] == port)
		mp_gpio_interrupt_unset(port);

	/* analog until someone takes it */
	if(_mp_gpio_debug(port->port, port->pin) == NO)
		mp_gpio_mode(port, MP_GPIO_MODE_ANALOG);

	port->used = NO;
	port->who = NULL;
	port->reverse = NO;
	port->direction = 0;

	return(TRUE);
}

void mp_gpio_mode(mp_gpio_port_t *port, mp_gpio_mode_t mode) {
	volatile uint32_t *cr = port->pin < 8 ? &port->base->CRL : &port->base->CRH;
	unsigned int shift = (port->pin & 7) * 4;

	MP_INTERRUPT_SAFE_BEGIN
	*cr = (*cr & ~(0xFUL << shift)) | ((uint32_t)mode << shift);
	MP_INTERRUPT_SAFE_END
}

mp_ret_t mp_gpio_direction(mp_gpio_port_t *port, mp_gpio_direction_t direction) {
	if(direction == MP_GPIO_OUTPUT)
		mp_gpio_mode(port, MP_GPIO_MODE_OUTPUT);
	else if(direction == MP_GPIO_INPUT)
		mp_gpio_mode(port, MP_GPIO_MODE_INPUT);
	else
		return(FALSE);

	port->direction = direction;
	return(TRUE);
}

mp_bool_t mp_gpio_read(mp_gpio_port_t *port) {
	return(port->base->IDR & (1UL << port->pin) ? ON : OFF);
}

void mp_gpio_set(mp_gpio_port_t *port) {
	if(port->reverse == YES)
		port->base->BRR = 1UL << port->pin;
	else
		port->base->BSRR = 1UL << port->pin;
}

void mp_gpio_unset(mp_gpio_port_t *port) {
	if(port->reverse == YES)
		port->base->BSRR = 1UL << port->pin;
	else
		port->base->BRR = 1UL << port->pin;
}

void mp_gpio_turn(mp_gpio_port_t *port) {
	if(port->base->ODR & (1UL << port->pin))
		port->base->BRR = 1UL << port->pin;
	else
		port->base->BSRR = 1UL << port->pin;
}

static int _mp_gpio_exti_irq(unsigned int line) {
	if(line <= 4)
		return(EXTI0_IRQn + line);
	if(line <= 9)
		return(EXTI9_5_IRQn);
	return(EXTI15_10_IRQn);
}

static mp_bool_t _mp_gpio_exti_shared_used(unsigned int line) {
	unsigned int first = line <= 9 ? 5 : 10;
	unsigned int last = line <= 9 ? 9 : 15;
	unsigned int a;

	for(a=first; a<=last; a++)
		if(a != line && __exti[a])
			return(YES);
	return(NO);
}

mp_ret_t mp_gpio_interrupt_set(mp_gpio_port_t *port, mp_interrupt_cb_t in, void *user, char *who) {
	unsigned int line = port->pin;
	unsigned int reg = line >> 2;
	unsigned int shift = (line & 3) * 4;

	if(__exti[line] && __exti[line] != port) {
		mp_printk("EXTI line %u is used by P%c%u, P%c%u can not have it",
			line, 'A' + __exti[line]->port, line, 'A' + port->port, line);
		return(FALSE);
	}

	MP_INTERRUPT_SAFE_BEGIN

	port->callback = in;
	port->user = user;
	__exti[line] = port;

	/* route the line to this port, rising edge, masked until enabled */
	AFIO->EXTICR[reg] = (AFIO->EXTICR[reg] & ~(0xFUL << shift)) | ((uint32_t)port->port << shift);
	EXTI->IMR &= ~(1UL << line);
	EXTI->RTSR |= 1UL << line;
	EXTI->FTSR &= ~(1UL << line);
	EXTI->PR = 1UL << line;

	mp_interrupt_set(_mp_gpio_exti_irq(line), _mp_gpio_exti_dispatch, (void *)(intptr_t)line, who);

	MP_INTERRUPT_SAFE_END

	return(TRUE);
}

mp_ret_t mp_gpio_interrupt_unset(mp_gpio_port_t *port) {
	unsigned int line = port->pin;

	if(__exti[line] != port)
		return(FALSE);

	MP_INTERRUPT_SAFE_BEGIN

	EXTI->IMR &= ~(1UL << line);
	EXTI->RTSR &= ~(1UL << line);
	EXTI->FTSR &= ~(1UL << line);
	EXTI->PR = 1UL << line;

	__exti[line] = NULL;
	port->callback = NULL;
	port->user = NULL;

	if(line <= 4 || _mp_gpio_exti_shared_used(line) == NO)
		mp_interrupt_unset(_mp_gpio_exti_irq(line));

	MP_INTERRUPT_SAFE_END

	return(TRUE);
}

/**
 * @brief Let a falling edge on a pin wake the core from Stop
 *
 * For a pin owned by a peripheral, the RX of a UART: only an EXTI line
 * survives Stop. The line has to be free.
 *
 * @param[in] port The pin
 * @return YES when armed
 */
mp_bool_t mp_gpio_wake_arm(mp_gpio_port_t *port) {
	if(__exti[port->pin])
		return(NO);
	if(mp_gpio_interrupt_set(port, NULL, NULL, "Wake") == FALSE)
		return(NO);
	mp_gpio_interrupt_hi2lo(port);
	mp_gpio_interrupt_enable(port);
	return(YES);
}

void mp_gpio_wake_disarm(mp_gpio_port_t *port) {
	mp_gpio_interrupt_unset(port);
}

/* user is the line that set the vector, shared vectors scan their group */
static void _mp_gpio_exti_dispatch(void *user) {
	unsigned int line = (unsigned int)(intptr_t)user;
	unsigned int first, last, a;
	uint32_t pending;
	mp_gpio_port_t *port;

	if(line <= 4)
		first = last = line;
	else if(line <= 9) {
		first = 5;
		last = 9;
	}
	else {
		first = 10;
		last = 15;
	}

	pending = EXTI->PR & EXTI->IMR;
	for(a=first; a<=last; a++) {
		if(!(pending & (1UL << a)))
			continue;

		/* clear first: the callback may flip the edge and wait for the next one */
		EXTI->PR = 1UL << a;

		port = __exti[a];
		if(port && port->callback)
			port->callback(port->user);
	}
}

/**@}*/
