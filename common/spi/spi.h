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

#ifndef _HAVE_MP_COMMON_SPI_H
	#define _HAVE_MP_COMMON_SPI_H

	#include <mp.h>

	/**
	 * @defgroup mpCommonSPI Shared SPI bus
	 * @ingroup mpCommon
	 *
	 * A synchronous SPI master on GPIO pins, mode 0, MSB first, about 100 kHz.
	 * One bus owns SCLK and MOSI. MISO is claimed only when the options name it.
	 * Chip select stays with the device. A driver opens the bus once and shares
	 * the handle. mp_spi_xfer reads MISO on that same clock. Without MISO the
	 * transfer still clocks MOSI and returns 0.
	 *
	 * Clock pin options, first match: sclk, sck, clk.
	 * Data pin options, first match: mosi, simo.
	 * @{
	 */

	typedef struct mp_spi_bus_s mp_spi_bus_t;

	struct mp_spi_bus_s {
		mp_kernel_t *kernel;
		mp_gpio_port_t *sclk;
		mp_gpio_port_t *mosi;
		/** NULL when the device is write-only */
		mp_gpio_port_t *miso;
		/** half-clock in microseconds; 5 gives about 100 kHz */
		unsigned long half_us;
	};

	/** Claim SCLK and MOSI. half_us 0 defaults to 5. Returns 1 on success. */
	mp_ret_t mp_spi_open(mp_kernel_t *kernel, mp_spi_bus_t *bus, mp_options_t *options, char *who);
	/** Release the pins. */
	void mp_spi_close(mp_spi_bus_t *bus);
	/** Change the half-clock (half_us 0 keeps the current value). */
	void mp_spi_setup(mp_spi_bus_t *bus, unsigned long half_us);

	/**
	 * One byte each way, MSB first. SCLK idles low. MOSI changes before the
	 * rising edge. MISO is sampled while SCLK is high, after that edge and
	 * before it falls. A NULL miso still clocks MOSI and the call returns 0.
	 * mp_spi_write_byte is this transfer with the read byte dropped, so the
	 * bus has one clock loop.
	 */
	unsigned char mp_spi_xfer(mp_spi_bus_t *bus, unsigned char byte);
	/** Same clock as mp_spi_xfer. The byte read back is dropped. */
	void mp_spi_write_byte(mp_spi_bus_t *bus, unsigned char byte);
	/** Send len bytes. Returns 1 when the buffer was clocked out. */
	int mp_spi_tx(mp_spi_bus_t *bus, const unsigned char *data, int len);

	/** @} */

#endif
