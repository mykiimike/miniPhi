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

#ifdef SUPPORT_COMMON_USB

/**
@defgroup mpArchSTM32Usb USB PHY

@ingroup mpArchSTM32

The F103 has one device controller and is locked to device. The F407
lists OTG FS and OTG HS, both free. OTG HS uses the on-chip full-speed
PHY (PB14, PB15), not ULPI. Pins are taken only when a role is armed.
No endpoints: the interrupt reports reset, connect, disconnect, suspend
and fault.

@{
*/

typedef struct {
	uint32_t base;
	int irq;
	volatile uint32_t *enr;
	volatile uint32_t *rst;
	uint32_t bit;
	char *dm;
	char *dp;
	char *disc;
	unsigned int af;
	mp_gpio_port_t *dmPin;
	mp_gpio_port_t *dpPin;
	mp_gpio_port_t *discPin;
	mp_usb_role_t role;
	volatile unsigned char live;
	volatile unsigned char bus;
	volatile unsigned char se0;
} mp_usb_hw_t;

static mp_ret_t _mp_usb_set_role(mp_usb_port_t *port, mp_usb_role_t role);
static mp_ret_t _mp_usb_connect(mp_usb_port_t *port, mp_bool_t on);
#if !defined(STM32F407xx)
static void _mp_usb_poll(mp_usb_port_t *port);
#endif

static const mp_usb_ops_t _mp_usb_ops = {
	_mp_usb_set_role,
	_mp_usb_connect,
#if defined(STM32F407xx)
	NULL,
#else
	_mp_usb_poll,
#endif
};

static volatile uint32_t *_mp_usb_reg(mp_usb_hw_t *hw, uint32_t offset) {
	return((volatile uint32_t *)(hw->base + offset));
}

static mp_bool_t _mp_usb_clock_on(mp_usb_hw_t *hw) {
	return((*hw->enr & hw->bit) ? YES : NO);
}

static void _mp_usb_clock(mp_usb_hw_t *hw, mp_bool_t on) {
	if(on == YES) {
		*hw->enr |= hw->bit;
		(void)*hw->enr;
		*hw->rst |= hw->bit;
		*hw->rst &= ~hw->bit;
	}
	else
		*hw->enr &= ~hw->bit;
}

static void _mp_usb_pins_release(mp_usb_hw_t *hw) {
	if(hw->dmPin) {
		mp_gpio_release(hw->dmPin);
		hw->dmPin = NULL;
	}
	if(hw->dpPin) {
		mp_gpio_release(hw->dpPin);
		hw->dpPin = NULL;
	}
	if(hw->discPin) {
		mp_gpio_set(hw->discPin);
		mp_gpio_release(hw->discPin);
		hw->discPin = NULL;
	}
}

static mp_ret_t _mp_usb_pins(mp_usb_port_t *port) {
	mp_usb_hw_t *hw = port->hw;

	hw->dmPin = mp_gpio_text_handle(hw->dm, port->name);
	if(!hw->dmPin)
		return(FALSE);
	hw->dpPin = mp_gpio_text_handle(hw->dp, port->name);
	if(!hw->dpPin) {
		_mp_usb_pins_release(hw);
		return(FALSE);
	}

#if defined(STM32F407xx)
	mp_gpio_mode(hw->dmPin, MP_GPIO_MODE_AF);
	mp_gpio_mode(hw->dpPin, MP_GPIO_MODE_AF);
	mp_gpio_af(hw->dmPin, hw->af);
	mp_gpio_af(hw->dpPin, hw->af);
#else
	/* the USB block drives these itself; floating inputs leave it the pad */
	mp_gpio_mode(hw->dmPin, MP_GPIO_MODE_INPUT);
	mp_gpio_mode(hw->dpPin, MP_GPIO_MODE_INPUT);
#endif
	if(hw->disc) {
		hw->discPin = mp_gpio_text_handle(hw->disc, port->name);
		if(!hw->discPin) {
			_mp_usb_pins_release(hw);
			return(FALSE);
		}
		/* high turns the board PNP off; set it before the pin becomes an output */
		mp_gpio_set(hw->discPin);
		mp_gpio_direction(hw->discPin, MP_GPIO_OUTPUT);
	}
	return(TRUE);
}

#if defined(STM32F407xx)

#define _GUSBCFG 0x00C
#define _GRSTCTL 0x010
#define _GINTSTS 0x014
#define _GINTMSK 0x018
#define _GRXFSIZ 0x024
#define _TX0     0x028
#define _GCCFG   0x038
#define _HPTXFSIZ 0x100
#define _HCFG    0x400
#define _HPRT    0x440
#define _DCFG    0x800
#define _DCTL    0x804
#define _DSTS    0x808
#define _PCGCCTL 0xE00

#define _PHYSEL  (1UL << 6)
#define _TRDT    (0xFUL << 10)
#define _FHMOD   (1UL << 29)
#define _FDMOD   (1UL << 30)
#define _CSRST   (1UL << 0)
#define _AHBIDL  (1UL << 31)
#define _PWRDWN  (1UL << 16)
#define _NOVBUS  (1UL << 21)
#define _USBSUSP (1UL << 11)
#define _USBRST  (1UL << 12)
#define _ENUMDNE (1UL << 13)
#define _PRTINT  (1UL << 24)
#define _DISCINT (1UL << 29)
#define _WKUINT  (1UL << 31)
#define _SDIS    (1UL << 1)
#define _PCSTS   (1UL << 0)
#define _PCDET   (1UL << 1)
#define _PENA    (1UL << 2)
#define _PENCHNG (1UL << 3)
#define _POCCHNG (1UL << 5)
#define _PPWR    (1UL << 12)
#define _HPRT_W1C (_PCDET | _PENCHNG | _POCCHNG)

static mp_ret_t _mp_usb_reset(mp_usb_hw_t *hw) {
	volatile uint32_t *rst = _mp_usb_reg(hw, _GRSTCTL);
	unsigned long n;

	n = 200000;
	while((*rst & _AHBIDL) == 0 && --n);
	if(n == 0)
		return(FALSE);

	*rst |= _CSRST;
	n = 200000;
	while((*rst & _CSRST) && --n);
	if(n == 0)
		return(FALSE);

	n = 200000;
	while((*rst & _AHBIDL) == 0 && --n);
	return(n ? TRUE : FALSE);
}

static void _mp_usb_irq(void *user);

static void _mp_usb_core_down(mp_usb_hw_t *hw) {
	if(hw->role != MP_USB_ROLE_NONE) {
		mp_interrupt_unset(hw->irq);
		hw->role = MP_USB_ROLE_NONE;
	}
	if(_mp_usb_clock_on(hw) == NO)
		return;
	*_mp_usb_reg(hw, 0x008) = 0;
	*_mp_usb_reg(hw, _GCCFG) = 0;
	_mp_usb_clock(hw, NO);
}

static void _mp_usb_hprt(mp_usb_hw_t *hw, uint32_t set, uint32_t clear) {
	uint32_t hprt = *_mp_usb_reg(hw, _HPRT);

	/* writing PENA as read disables the port; the status bits clear on 1 */
	hprt &= ~(_PENA | _HPRT_W1C);
	hprt = (hprt | set) & ~clear;
	*_mp_usb_reg(hw, _HPRT) = hprt;
}

static mp_ret_t _mp_usb_core(mp_usb_port_t *port, mp_usb_role_t role) {
	mp_usb_hw_t *hw = port->hw;
	uint32_t cfg;
	uint32_t host;

	_mp_usb_clock(hw, YES);
	cfg = *_mp_usb_reg(hw, _GUSBCFG);
	cfg |= _PHYSEL;
	/* full-speed turnaround; endpoints will pick it from HCLK */
	cfg = (cfg & ~_TRDT) | (6UL << 10);
	*_mp_usb_reg(hw, _GUSBCFG) = cfg;
	if(_mp_usb_reset(hw) != TRUE)
		return(FALSE);

	/* no VBUS pin bound yet: do not wait on it */
	*_mp_usb_reg(hw, _GCCFG) = _PWRDWN | _NOVBUS;
	cfg = *_mp_usb_reg(hw, _GUSBCFG) & ~(_FHMOD | _FDMOD);
	cfg |= role == MP_USB_ROLE_HOST ? _FHMOD : _FDMOD;
	*_mp_usb_reg(hw, _GUSBCFG) = cfg;
	mp_clock_delay(25);

	host = *_mp_usb_reg(hw, _GINTSTS) & 1;
	if(role == MP_USB_ROLE_HOST && host == 0)
		return(FALSE);
	if(role == MP_USB_ROLE_DEVICE && host == 1)
		return(FALSE);

	*_mp_usb_reg(hw, _PCGCCTL) = 0;
	*_mp_usb_reg(hw, _GRXFSIZ) = 128;
	*_mp_usb_reg(hw, _TX0) = (64UL << 16) | 128;

	if(role == MP_USB_ROLE_HOST) {
		*_mp_usb_reg(hw, _HPTXFSIZ) = (64UL << 16) | 192;
		/* 48 MHz PHY, full speed */
		*_mp_usb_reg(hw, _HCFG) = 1 | (1UL << 2);
		*_mp_usb_reg(hw, _GINTMSK) = _PRTINT | _DISCINT;
	}
	else {
		*_mp_usb_reg(hw, _DCTL) |= _SDIS;
		*_mp_usb_reg(hw, _DCFG) = 3;
		*_mp_usb_reg(hw, _GINTMSK) = _USBRST | _ENUMDNE | _USBSUSP | _WKUINT;
	}

	*_mp_usb_reg(hw, _GINTSTS) = 0xFFFFFFFF;
	hw->role = role;
	mp_interrupt_set(hw->irq, _mp_usb_irq, port, port->name);
	*_mp_usb_reg(hw, 0x008) = 1;
	return(TRUE);
}

static mp_usb_speed_t _mp_usb_enum_speed(uint32_t dsts) {
	switch((dsts >> 1) & 3) {
		case 2: return(MP_USB_SPEED_LOW);
		case 0: return(MP_USB_SPEED_HIGH);
		default: return(MP_USB_SPEED_FULL);
	}
}

static void _mp_usb_irq(void *user) {
	mp_usb_port_t *port = user;
	mp_usb_hw_t *hw = port->hw;
	uint32_t sts = *_mp_usb_reg(hw, _GINTSTS) & *_mp_usb_reg(hw, _GINTMSK);

	if(hw->role == MP_USB_ROLE_DEVICE) {
		if(sts & _USBSUSP) {
			*_mp_usb_reg(hw, _GINTSTS) = _USBSUSP;
			mp_usb_event(port, MP_USB_EVENT_SUSPEND, port->speed);
		}
		if(sts & _USBRST) {
			*_mp_usb_reg(hw, _GINTSTS) = _USBRST;
			mp_usb_event(port, MP_USB_EVENT_RESET, MP_USB_SPEED_NONE);
		}
		if(sts & _ENUMDNE) {
			mp_usb_speed_t speed = _mp_usb_enum_speed(*_mp_usb_reg(hw, _DSTS));

			*_mp_usb_reg(hw, _GINTSTS) = _ENUMDNE;
			mp_usb_event(port, MP_USB_EVENT_CONNECT, speed);
		}
		if(sts & _WKUINT) {
			*_mp_usb_reg(hw, _GINTSTS) = _WKUINT;
			mp_usb_event(port, MP_USB_EVENT_RESUME, port->speed);
		}
		return;
	}

	if(hw->role == MP_USB_ROLE_HOST) {
		uint32_t hprt = *_mp_usb_reg(hw, _HPRT);

		if(hprt & _PCDET) {
			if(hprt & _PCSTS) {
				uint32_t spd = (hprt >> 17) & 3;
				mp_usb_event(port, MP_USB_EVENT_CONNECT, spd == 2 ? MP_USB_SPEED_LOW : MP_USB_SPEED_FULL);
			}
			else
				mp_usb_event(port, MP_USB_EVENT_DISCONNECT, MP_USB_SPEED_NONE);
		}
		if(hprt & _POCCHNG)
			mp_usb_event(port, MP_USB_EVENT_FAULT, port->speed);
		if((sts & _DISCINT) && (hprt & _PCSTS) == 0)
			mp_usb_event(port, MP_USB_EVENT_DISCONNECT, MP_USB_SPEED_NONE);

		*_mp_usb_reg(hw, _GINTSTS) = sts & (_PRTINT | _DISCINT);
		*_mp_usb_reg(hw, _HPRT) = (hprt & ~(_PENA | _HPRT_W1C)) | (hprt & _HPRT_W1C);
	}
}

static mp_ret_t _mp_usb_pull(mp_usb_port_t *port, mp_bool_t on) {
	mp_usb_hw_t *hw = port->hw;

	if(hw->role == MP_USB_ROLE_DEVICE) {
		if(on == YES)
			*_mp_usb_reg(hw, _DCTL) &= ~_SDIS;
		else
			*_mp_usb_reg(hw, _DCTL) |= _SDIS;
		return(TRUE);
	}
	if(hw->role == MP_USB_ROLE_HOST) {
		/* the board power switch is not bound yet; this is the core port power */
		if(on == YES)
			_mp_usb_hprt(hw, _PPWR, 0);
		else
			_mp_usb_hprt(hw, 0, _PPWR);
		return(TRUE);
	}
	return(FALSE);
}

#else

#define _CNTR 0x40
#define _ISTR 0x44
#define _FNR  0x48
#define _FRES   (1UL << 0)
#define _PDWN   (1UL << 1)
#define _LPMODE (1UL << 2)
#define _FSUSP  (1UL << 3)
#define _ESOFM  (1UL << 8)
#define _SOFM   (1UL << 9)
#define _RESETM (1UL << 10)
#define _SUSPM  (1UL << 11)
#define _WKUPM  (1UL << 12)
#define _ESOF   (1UL << 8)
#define _SOF    (1UL << 9)
#define _RESETF (1UL << 10)
#define _SUSPF  (1UL << 11)
#define _WKUPF  (1UL << 12)
#define _RXDM   (1UL << 14)
#define _RXDP   (1UL << 15)
/* FNR[15:14]: 0 SE0, 1 K (D-), 2 J (D+), 3 SE1 */
#define _LINE_SE0 0
#define _LINE_K   1
#define _LINE_J   2
#define _MASK_OFF (_FSUSP | _LPMODE | _ESOFM | _SOFM | _SUSPM | _RESETM | _WKUPM)
#define _MASK_IDLE (_FSUSP | _RESETM | _WKUPM)
#define _MASK_RUN  (_RESETM | _WKUPM | _SUSPM)
/* 20 ms samples; a bus reset is shorter than this */
#define _SE0_GONE 8

static void _mp_usb_irq(void *user);

/* write 0 to clear; read-modify-write drops a flag that sets in between */
static void _mp_usb_istr_clr(volatile uint32_t *istr, uint32_t bits) {
	*istr = (uint16_t)~(uint16_t)bits;
}

static void _mp_usb_mask(mp_usb_hw_t *hw, uint32_t mask) {
	volatile uint32_t *cntr = _mp_usb_reg(hw, _CNTR);

	MP_INTERRUPT_SAFE_BEGIN
	*cntr = (*cntr & ~_MASK_OFF) | mask;
	MP_INTERRUPT_SAFE_END
}

static int _mp_usb_line(mp_usb_hw_t *hw) {
	return((int)((*_mp_usb_reg(hw, _FNR) & (_RXDP | _RXDM)) >> 14));
}

static void _mp_usb_host(mp_usb_port_t *port) {
	mp_usb_hw_t *hw = port->hw;

	hw->bus = 0;
	hw->se0 = 0;
	_mp_usb_mask(hw, _MASK_RUN);
	mp_usb_event(port, MP_USB_EVENT_RESET, MP_USB_SPEED_FULL);
	mp_usb_event(port, MP_USB_EVENT_CONNECT, MP_USB_SPEED_FULL);
}

static void _mp_usb_unplug(mp_usb_port_t *port) {
	mp_usb_hw_t *hw = port->hw;

	hw->bus = 0;
	hw->se0 = 0;
	/* pull-up stays; the next host reset is still visible */
	_mp_usb_mask(hw, _MASK_IDLE);
	mp_usb_event(port, MP_USB_EVENT_DISCONNECT, MP_USB_SPEED_NONE);
}

static void _mp_usb_core_down(mp_usb_hw_t *hw) {
	if(hw->role != MP_USB_ROLE_NONE) {
		mp_interrupt_unset(hw->irq);
		hw->role = MP_USB_ROLE_NONE;
	}
	if(_mp_usb_clock_on(hw) == NO)
		return;
	*_mp_usb_reg(hw, _CNTR) = _PDWN | _FRES;
	_mp_usb_clock(hw, NO);
}

static mp_ret_t _mp_usb_core(mp_usb_port_t *port, mp_usb_role_t role) {
	mp_usb_hw_t *hw = port->hw;

	if(role != MP_USB_ROLE_DEVICE)
		return(FALSE);

	_mp_usb_clock(hw, YES);
	*_mp_usb_reg(hw, _CNTR) = _PDWN;
	*_mp_usb_reg(hw, _ISTR) = 0;
	hw->role = MP_USB_ROLE_DEVICE;
	/* shared with CAN1 RX0; CAN is not driven */
	mp_interrupt_set(hw->irq, _mp_usb_irq, port, port->name);
	return(TRUE);
}

static void _mp_usb_irq(void *user) {
	mp_usb_port_t *port = user;
	mp_usb_hw_t *hw = port->hw;
	volatile uint32_t *istr = _mp_usb_reg(hw, _ISTR);
	uint32_t got;
	int line;

	if(hw->live == 0 || (*_mp_usb_reg(hw, _CNTR) & _PDWN)) {
		*istr = 0;
		return;
	}

	got = *istr;
	/* reset released: a host arrived (or re-arrived) */
	if(got & _RESETF) {
		_mp_usb_istr_clr(istr, _RESETF | _SUSPF | _WKUPF | _ESOF);
		_mp_usb_host(port);
		return;
	}
	line = _mp_usb_line(hw);
	if(line != _LINE_SE0) {
		hw->bus = 1;
		hw->se0 = 0;
	}
	if(got & _SUSPF) {
		_mp_usb_istr_clr(istr, _SUSPF);
		/* no endpoints: a host that stops SOF has left; the pull-up stays */
		_mp_usb_unplug(port);
	}
	if(got & _WKUPF) {
		_mp_usb_istr_clr(istr, _WKUPF);
		/* a reset or a resume reached the port while it was idle */
		_mp_usb_host(port);
	}
}

static void _mp_usb_poll(mp_usb_port_t *port) {
	mp_usb_hw_t *hw = port->hw;
	volatile uint32_t *istr;
	int line;
	int gone = 0;

	if(hw->live == 0 || hw->role != MP_USB_ROLE_DEVICE)
		return;

	istr = _mp_usb_reg(hw, _ISTR);
	if(*istr & _RESETF) {
		_mp_usb_istr_clr(istr, _RESETF | _SUSPF | _WKUPF | _ESOF);
		_mp_usb_host(port);
		return;
	}
	/* a held suspend freezes the line bits; WKUP reports a change */
	if(*_mp_usb_reg(hw, _CNTR) & _FSUSP)
		return;
	MP_INTERRUPT_SAFE_BEGIN
	line = _mp_usb_line(hw);
	if(line != _LINE_SE0) {
		hw->bus = 1;
		hw->se0 = 0;
	}
	else if(hw->bus) {
		if(hw->se0 < _SE0_GONE)
			hw->se0++;
		gone = hw->se0 >= _SE0_GONE;
	}
	MP_INTERRUPT_SAFE_END
	if(gone)
		_mp_usb_unplug(port);
}

static mp_ret_t _mp_usb_pull(mp_usb_port_t *port, mp_bool_t on) {
	mp_usb_hw_t *hw = port->hw;

	if(hw->role != MP_USB_ROLE_DEVICE)
		return(FALSE);
	if(on == YES) {
		/* PD3 low turns on the board 1.5k; the PHY pull-up comes out of power-down */
		if(hw->discPin)
			mp_gpio_unset(hw->discPin);
		/* FRES itself sets RESET; clear it before the masks, or boot looks like a host */
		*_mp_usb_reg(hw, _CNTR) = _FRES;
		mp_clock_delay(1);
		*_mp_usb_reg(hw, _CNTR) = 0;
		*_mp_usb_reg(hw, _ISTR) = 0;
		hw->bus = 0;
		hw->se0 = 0;
		hw->live = 1;
		_mp_usb_mask(hw, _MASK_IDLE);
	}
	else {
		hw->live = 0;
		hw->bus = 0;
		hw->se0 = 0;
		if(hw->discPin)
			mp_gpio_set(hw->discPin);
		*_mp_usb_reg(hw, _CNTR) = _PDWN;
	}
	return(TRUE);
}

#endif

static void _mp_usb_off(mp_usb_port_t *port) {
	mp_usb_hw_t *hw = port->hw;

	_mp_usb_core_down(hw);
	hw->live = 0;
	hw->bus = 0;
	hw->se0 = 0;
	_mp_usb_pins_release(hw);
}

/* the PHY is off and common still believes the previous role */
static void _mp_usb_died(mp_usb_port_t *port) {
	_mp_usb_off(port);
	port->role = MP_USB_ROLE_NONE;
	port->connect = NO;
	port->fault = NO;
	port->speed = MP_USB_SPEED_NONE;
	port->link = MP_USB_LINK_OFF;
}

static mp_ret_t _mp_usb_set_role(mp_usb_port_t *port, mp_usb_role_t role) {
	mp_usb_hw_t *hw = port->hw;
	mp_usb_role_t prev;

	if(role == MP_USB_ROLE_NONE) {
		_mp_usb_off(port);
		return(TRUE);
	}
	if(mp_clock_usb48() == NO) {
		mp_printk("%s: no 48 MHz clock", port->name);
		return(FALSE);
	}

	prev = hw->role;
	if(hw->dmPin == NULL && _mp_usb_pins(port) != TRUE)
		return(FALSE);
	if(hw->role != MP_USB_ROLE_NONE)
		_mp_usb_core_down(hw);
	if(_mp_usb_core(port, role) == TRUE)
		return(TRUE);

	mp_printk("%s: %s did not start", port->name, mp_usb_role_name(role));
	_mp_usb_core_down(hw);
	if(prev == MP_USB_ROLE_NONE) {
		_mp_usb_pins_release(hw);
		return(FALSE);
	}
	/* the core was reset, so a pull-up or VBUS from the previous role is gone */
	if(_mp_usb_core(port, prev) == TRUE) {
		port->connect = NO;
		port->fault = NO;
		port->speed = MP_USB_SPEED_NONE;
		port->link = MP_USB_LINK_DISCONNECTED;
		return(FALSE);
	}

	_mp_usb_died(port);
	return(FALSE);
}

static mp_ret_t _mp_usb_connect(mp_usb_port_t *port, mp_bool_t on) {
	return(_mp_usb_pull(port, on ? YES : NO));
}

static void _mp_usb_add(mp_kernel_t *kernel, mp_usb_port_t *port, mp_usb_hw_t *hw,
	char *name, uint32_t base, int irq, volatile uint32_t *enr, volatile uint32_t *rst,
	uint32_t bit, char *dm, char *dp, char *disc, unsigned int af, mp_usb_caps_t caps, mp_usb_role_t fixed) {

	if(port->kernel)
		return;
	memset(hw, 0, sizeof(*hw));
	hw->base = base;
	hw->irq = irq;
	hw->enr = enr;
	hw->rst = rst;
	hw->bit = bit;
	hw->dm = dm;
	hw->dp = dp;
	hw->disc = disc;
	hw->af = af;
	if(mp_usb_port_add(kernel, port, name, caps, fixed, &_mp_usb_ops, hw) != TRUE)
		mp_printk("USB: %s not listed", name);
}

void mp_usb_arch_register(mp_kernel_t *kernel) {
#if defined(STM32F407xx)
	static mp_usb_port_t port[2];
	static mp_usb_hw_t hw[2];

	_mp_usb_add(kernel, &port[0], &hw[0], "otg-fs", 0x50000000, OTG_FS_IRQn,
		&RCC->AHB2ENR, &RCC->AHB2RSTR, 1UL << 7,
		"pa11", "pa12", NULL, 10, MP_USB_CAP_DEVICE | MP_USB_CAP_HOST, MP_USB_ROLE_NONE);
	_mp_usb_add(kernel, &port[1], &hw[1], "otg-hs", 0x40040000, OTG_HS_IRQn,
		&RCC->AHB1ENR, &RCC->AHB1RSTR, 1UL << 29,
		"pb14", "pb15", NULL, 12, MP_USB_CAP_DEVICE | MP_USB_CAP_HOST, MP_USB_ROLE_NONE);
#else
	static mp_usb_port_t port;
	static mp_usb_hw_t hw;

	_mp_usb_add(kernel, &port, &hw, "usb", 0x40005C00, USB_LP_CAN1_RX0_IRQn,
		&RCC->APB1ENR, &RCC->APB1RSTR, 1UL << 23,
		"pa11", "pa12", "pd3", 0, MP_USB_CAP_DEVICE, MP_USB_ROLE_DEVICE);
#endif
}

/**@}*/

#endif
