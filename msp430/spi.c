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


static mp_list_t __spi;
static unsigned int __spi_count;


void mp_spi_init() {
	mp_list_init(&__spi);
	__spi_count = 0;
}

void mp_spi_fini() {
/* should be ok */
}


mp_ret_t mp_spi_open(
		mp_spi_t *spi, char *who,
		mp_spi_phase_t phase,
		mp_spi_polarity_t polarity,
		mp_spi_role_t role,
		mp_spi_mode_t mode,
		mp_spi_bit_t bit
	) {
	mp_list_add_last(&__spi, &spi->item, spi);
	__spi_count++;
	return(TRUE);
}

mp_ret_t mp_spi_close(mp_spi_t *spi) {
	mp_list_remove(&__spi, &spi->item);
	__spi_count--;
	return(TRUE);

}
