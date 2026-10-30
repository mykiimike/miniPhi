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
@defgroup mpArchESP32S3ADC ADC

@ingroup mpArchESP32

ADC1 oneshot through the RTC controller. GPIO1 to GPIO10 are channels 0 to 9.
12 bit, 11 dB. The raw count is what a caller scales.

@{
*/

#define S3_SENS   0x60008800UL
#define S3_RTCIO  0x60008400UL

#define _READER1     (S3_SENS + 0x00UL)
#define _MEAS1_CTL2  (S3_SENS + 0x0CUL)
#define _MEAS1_MUX   (S3_SENS + 0x10UL)
#define _ATTEN1      (S3_SENS + 0x14UL)
#define _XPD         (S3_SENS + 0x3CUL)
#define _SLAVE1      (S3_SENS + 0x40UL)
#define _CLK_GATE    (S3_SENS + 0x104UL)

/* RTC_CNTL_PAD_HOLD, touch pad n is bit n */
#define _PAD_HOLD    (S3_RTC + 0xD8UL)
/* RTC_GPIO_ENABLE_W1TC, rtcio n is bit n+10 */
#define _RTC_OE_CLR  (S3_RTCIO + 0x14UL)

#define _ATTEN_11DB  3UL
#define _SPIN        200000UL

static int _ready;

static unsigned long _pad(int gpio) {
	/* TOUCH_PAD1 is GPIO1 at RTC_IO + 0x88; each pad register is 4 bytes */
	return(S3_RTCIO + 0x84UL + (unsigned long)gpio * 4UL);
}

static void _analog(int gpio) {
	unsigned long pad = _pad(gpio);
	unsigned long mux;

	/* digital driver off, no pull, input disabled */
	s3_write(S3_GPIO + 0x28UL, 1UL << gpio);
	mux = s3_read(S3_IOMUX + 0x04UL + (unsigned long)gpio * 4UL);
	mux &= ~((1UL << 7) | (1UL << 8) | (1UL << 9));
	s3_write(S3_IOMUX + 0x04UL + (unsigned long)gpio * 4UL, mux);

	if(gpio <= 14)
		s3_clr(_PAD_HOLD, 1UL << gpio);
	/* MUX_SEL routes the pad to the RTC. FUN_SEL 0, pulls and input off. */
	s3_set(pad, 1UL << 19);
	s3_clr(pad, (3UL << 17) | (1UL << 13) | (1UL << 27) | (1UL << 28));
	s3_write(_RTC_OE_CLR, 1UL << (gpio + 10));
}

static void _power(void) {
	unsigned long power;

	if(_ready)
		return;
	/* saradc clock and the RTC I/O clock */
	s3_set(_CLK_GATE, (1UL << 30) | (1UL << 31));
	power = s3_read(_XPD);
	power &= ~(3UL << 29);
	power |= 3UL << 29;
	s3_write(_XPD, power);

	/* RTC controller, clock divided by 1, data not inverted */
	s3_clr(_MEAS1_MUX, 1UL << 31);
	s3_write(_READER1, (s3_read(_READER1) & ~0xFFUL) | 1UL);
	s3_clr(_READER1, 1UL << 28);
	_ready = 1;
}

int mp_adc_convert(int gpio) {
	int channel;
	unsigned long ctl;
	unsigned long atten;
	unsigned long spins;
	int raw;

	if(gpio < 1 || gpio > 10)
		return(-1);
	channel = gpio - 1;
	_power();
	_analog(gpio);

	atten = s3_read(_ATTEN1);
	atten &= ~(3UL << (channel * 2));
	atten |= _ATTEN_11DB << (channel * 2);
	s3_write(_ATTEN1, atten);

	ctl = s3_read(_MEAS1_CTL2);
	ctl &= ~(0xFFFUL << 19);
	ctl |= 1UL << (19 + channel);
	ctl |= (1UL << 18) | (1UL << 31);
	ctl &= ~(1UL << 17);
	s3_write(_MEAS1_CTL2, ctl);

	spins = _SPIN;
	while(((s3_read(_SLAVE1) >> 22) & 0xFFUL) != 0 && spins)
		spins--;
	if(!spins)
		return(-1);

	s3_set(_MEAS1_CTL2, 1UL << 17);

	spins = _SPIN;
	while((s3_read(_MEAS1_CTL2) & (1UL << 16)) == 0 && spins)
		spins--;
	raw = spins ? (int)(s3_read(_MEAS1_CTL2) & 0xFFFUL) : -1;

	ctl = s3_read(_MEAS1_CTL2);
	ctl &= ~((1UL << 17) | (0xFFFUL << 19));
	s3_write(_MEAS1_CTL2, ctl);
	return(raw);
}

/**@}*/
