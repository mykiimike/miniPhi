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
@defgroup mpArchESP32C6ADC ADC

@ingroup mpArchESP32

ADC1 oneshot through the HP APB_SARADC block. GPIO0 to GPIO6 are channels
0 to 6. 12 bit, 11 dB. The raw count is what a caller scales. Synchronous:
no DMA and no interrupt.

@{
*/

#define C6_ADC     0x6000E000UL
#define C6_LP_IO   0x600B2000UL
/* SAR power detector. The analog block stays dark until this agrees. */
#define C6_PWDET   0x600A0810UL

#define _CTRL      (C6_ADC + 0x00UL)
#define _FSM       (C6_ADC + 0x0CUL)
#define _ONCE      (C6_ADC + 0x20UL)
#define _DATA1     (C6_ADC + 0x2CUL)
#define _INT_RAW   (C6_ADC + 0x44UL)
#define _INT_CLR   (C6_ADC + 0x4CUL)

#define _PCR_CONF  (C6_PCR + 0x80UL)
#define _PCR_CLK   (C6_PCR + 0x84UL)
#define _FE_CLK    (C6_MODEM_SYSCON + 0x14UL)

#define _ATTEN_11DB 3UL
#define _SPIN       100000UL

static int _ready;

static void _pause(void) {
	volatile unsigned long n = 800UL;

	while(n)
		n--;
}

/* GPIO0..6 are LP pads. The analog path is the RTC function, driver off, no pull. */
static void _analog(int gpio) {
	unsigned long mux;
	unsigned long gpioReg;

	if(gpio < 0 || gpio > 6)
		return;

	c6_write(C6_GPIO + 0x28UL, 1UL << gpio);
	mux = c6_read(c6_iomux(gpio));
	mux &= ~((1UL << 7) | (1UL << 8) | (1UL << 9));
	c6_write(c6_iomux(gpio), mux);

	c6_set(C6_LPPERI + 0x00UL, 1UL << 27);
	c6_set(C6_LP_AON + 0x28UL, 1UL << gpio);
	c6_clr(C6_LP_IO + 0x28UL + (unsigned long)gpio * 4UL, 1UL << 2);
	c6_write(C6_LP_IO + 0x14UL, 1UL << gpio);
	gpioReg = C6_LP_IO + 0x48UL + (unsigned long)gpio * 4UL;
	c6_clr(gpioReg, (1UL << 7) | (1UL << 8) | (1UL << 9) | (7UL << 12));
}

static void _power(void) {
	unsigned long conf;
	unsigned long clk;
	unsigned long ctrl;

	if(_ready)
		return;

	/* apb register clock on, function clock from XTAL divided by 16 */
	conf = c6_read(_PCR_CONF);
	conf |= 1UL << 2;
	conf &= ~((1UL << 1) | (1UL << 3));
	c6_write(_PCR_CONF, conf);

	clk = c6_read(_PCR_CLK);
	clk &= ~((0x3FUL << 0) | (0x3FUL << 6) | (0xFFUL << 12) | (3UL << 20));
	clk |= (1UL << 6) | (15UL << 12) | (1UL << 22);
	c6_write(_PCR_CLK, clk);

	ctrl = c6_read(_CTRL);
	ctrl &= ~((0xFFUL << 7) | (3UL << 27));
	ctrl |= (1UL << 6) | (1UL << 7) | (3UL << 27) | (1UL << 29);
	c6_write(_CTRL, ctrl);
	c6_write(_FSM, 8UL | (8UL << 8) | (255UL << 16));

	/* the shared SAR follows the power detector, and the front-end clock */
	c6_set(C6_PWDET, (1UL << 24) | (1UL << 23));
	c6_set(_FE_CLK, (1UL << 13) | (1UL << 16));
	_pause();
	_ready = 1;
}

void mp_adc_init(void) {
	_power();
}

int mp_adc_read(int channel) {
	unsigned long once;
	unsigned long spins;
	int raw;

	if(channel < 0 || channel > 6)
		return(-1);
	_power();
	_analog(channel);

	c6_write(_INT_CLR, 1UL << 31);
	once = c6_read(_ONCE);
	once &= ~((3UL << 23) | (0xFUL << 25) | (1UL << 29) | (1UL << 30) | (1UL << 31));
	once |= (_ATTEN_11DB << 23) | ((unsigned long)channel << 25) | (1UL << 31);
	c6_write(_ONCE, once);

	/* the start bit is a step; it has to be low long enough for the ADC clock */
	c6_clr(_ONCE, 1UL << 29);
	_pause();
	c6_set(_ONCE, 1UL << 29);

	spins = _SPIN;
	while((c6_read(_INT_RAW) & (1UL << 31)) == 0 && spins)
		spins--;
	raw = spins ? (int)(c6_read(_DATA1) & 0xFFFUL) : -1;

	c6_clr(_ONCE, (1UL << 29) | (1UL << 31));
	c6_write(_INT_CLR, 1UL << 31);
	return(raw);
}

int mp_adc_read_gpio(int gpio) {
	if(gpio < 0 || gpio > 6)
		return(-1);
	return(mp_adc_read(gpio));
}

/**@}*/
