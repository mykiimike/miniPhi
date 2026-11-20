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
@defgroup mpArchESP32ClassicSpi SPI

@ingroup mpArchESP32

The common SPI bus hands SCK GPIO14 and MOSI GPIO13 to SPI2 (HSPI) through
their IO_MUX function 1. Other pins stay on the GPIO bitbang. The master is
write-only: MISO GPIO12 is the flash-voltage strap and is never muxed.

SPI2 runs at most 40 MHz: APB/2 on either tree, 20 MHz on the crystal and
40 MHz on the PLL. The divider is checked before each transfer, so a
level change takes effect on the next one. One transfer is at most the
16-word buffer at +0x80. It only takes 32-bit stores, byte 0 of W0
leaving first.

@{
*/

#ifdef SUPPORT_COMMON_SPI

#define _CMD      (CL_SPI2 + 0x00UL)
#define _CTRL     (CL_SPI2 + 0x08UL)
#define _CLOCK    (CL_SPI2 + 0x18UL)
#define _USER     (CL_SPI2 + 0x1CUL)
#define _MOSI_LEN (CL_SPI2 + 0x28UL)
#define _PIN      (CL_SPI2 + 0x34UL)
#define _SLAVE    (CL_SPI2 + 0x38UL)
#define _W0       (CL_SPI2 + 0x80UL)

#define _USR      (1UL << 18)
#define _USR_MOSI (1UL << 27)
#define _SLV_RST  (1UL << 31)
/* PERI_CLK_EN and PERI_RST_EN bit 6 */
#define _SPI2_CLK (1UL << 6)

/* MCU_SEL 1 is HSPID on GPIO13 and HSPICLK on GPIO14 */
#define _IOMUX_SPI ((1UL << 12) | (1UL << 9) | (2UL << 10))
#define _IOMUX_13  (CL_IOMUX + 0x38UL)
#define _IOMUX_14  (CL_IOMUX + 0x30UL)

#define _CHUNK 64
#define _SPI_MAX_HZ 40000000UL

static unsigned long _mp_spi_apb;

/* divide by n: clkcnt_n and clkcnt_l are n - 1, clkcnt_h is n / 2 - 1 */
static void _mp_spi_rate(void) {
	unsigned long apb = cl_apb_hz();
	unsigned long n;

	if(apb == _mp_spi_apb)
		return;
	n = (apb + _SPI_MAX_HZ - 1UL) / _SPI_MAX_HZ;
	if(n < 2)
		n = 2;
	cl_write(_CLOCK, ((n - 1UL) << 12) | ((n / 2UL - 1UL) << 6) | (n - 1UL));
	_mp_spi_apb = apb;
}

static mp_bool_t _mp_spi_busy(void) {
	unsigned long spins = 1000000UL;

	while((cl_read(_CMD) & _USR) && --spins)
		;
	return(spins ? NO : YES);
}

mp_bool_t mp_spi_bus_hw_open(mp_spi_bus_t *bus) {
	if(bus->sclk->pin != 14 || bus->mosi->pin != 13)
		return(NO);
	cl_periph(_SPI2_CLK, 1);
	cl_pad_digital(13);
	cl_pad_digital(14);
	cl_write(_IOMUX_13, _IOMUX_SPI);
	cl_write(_IOMUX_14, _IOMUX_SPI);
	/* MSB first, mode 0 */
	cl_write(_CTRL, 0);
	_mp_spi_apb = 0;
	_mp_spi_rate();
	cl_write(_USER, _USR_MOSI);
	/* CS0 to CS2 stay off; the device owns its chip select */
	cl_write(_PIN, 7UL);
	cl_write(_SLAVE, _SLV_RST);
	cl_write(_SLAVE, 0);
	return(YES);
}

void mp_spi_bus_hw_flush(mp_spi_bus_t *bus) {
	(void)bus;
	(void)_mp_spi_busy();
}

int mp_spi_bus_hw_tx(mp_spi_bus_t *bus, const unsigned char *data, int len) {
	(void)bus;
	while(len > 0) {
		int n = len > _CHUNK ? _CHUNK : len;
		int words = (n + 3) / 4;
		int index = 0;

		if(_mp_spi_busy() == YES)
			return(0);
		_mp_spi_rate();
		if(((unsigned long)data & 3UL) == 0) {
			const unsigned long *src = (const unsigned long *)data;

			for(; index < n / 4; index++)
				cl_write(_W0 + (unsigned long)index * 4UL, src[index]);
		}
		for(; index < words; index++) {
			int base = index * 4;
			unsigned long word = 0;
			int byte;

			for(byte = 0; byte < 4 && base + byte < n; byte++)
				word |= (unsigned long)data[base + byte] << (8 * byte);
			cl_write(_W0 + (unsigned long)index * 4UL, word);
		}
		__asm__ volatile("memw" ::: "memory");
		cl_write(_MOSI_LEN, (unsigned long)(n * 8 - 1));
		cl_write(_CMD, _USR);
		data += n;
		len -= n;
	}
	/* the caller may move DC or CS as soon as this returns */
	if(_mp_spi_busy() == YES)
		return(0);
	return(1);
}

void mp_spi_bus_hw_close(mp_spi_bus_t *bus) {
	(void)_mp_spi_busy();
	cl_write(_SLAVE, _SLV_RST);
	cl_write(_SLAVE, 0);
	/* back on the GPIO matrix before the bus releases them */
	mp_gpio_mode(bus->sclk, MP_GPIO_MODE_OUTPUT);
	mp_gpio_mode(bus->mosi, MP_GPIO_MODE_OUTPUT);
}

#endif

/**@}*/
