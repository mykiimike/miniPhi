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

#ifdef SUPPORT_COMMON_FS_BLOCK_SD

/* Ncr is at most 8 clocks. A data token may take about 100 ms at 100 kHz. */
#define _MP_FS_SD_RESPONSE 8
#define _MP_FS_SD_TOKEN 65536
#define _MP_FS_SD_BUSY 100000
#define _MP_FS_SD_READY 1024

static unsigned char _mp_fs_sd_crc7(const unsigned char *data, int len) {
	unsigned char crc = 0;
	int i, bit;

	for(i = 0; i < len; i++) {
		unsigned char byte = data[i];

		for(bit = 0; bit < 8; bit++) {
			crc = (unsigned char)(crc << 1);
			if((byte ^ crc) & 0x80)
				crc ^= 0x09;
			byte = (unsigned char)(byte << 1);
		}
	}
	return((unsigned char)((crc << 1) | 1));
}

static unsigned short _mp_fs_sd_crc16(const unsigned char *data, int len) {
	unsigned short crc = 0;
	int i, bit;

	for(i = 0; i < len; i++) {
		crc ^= (unsigned short)data[i] << 8;
		for(bit = 0; bit < 8; bit++) {
			if(crc & 0x8000)
				crc = (unsigned short)((crc << 1) ^ 0x1021);
			else
				crc = (unsigned short)(crc << 1);
		}
	}
	return(crc);
}

static unsigned long _mp_fs_sd_bits(const unsigned char *reg, int hi, int lo) {
	unsigned long value = 0;
	int bit;

	for(bit = hi; bit >= lo; bit--) {
		int byte = (127 - bit) / 8;

		value = (value << 1) | ((reg[byte] >> (bit % 8)) & 1UL);
	}
	return(value);
}

static void _mp_fs_sd_select(mp_fs_sd_t *card, int on) {
	if(on)
		mp_gpio_unset(card->cs);
	else
		mp_gpio_set(card->cs);
	mp_spi_xfer(&card->bus, 0xFF);
}

static unsigned char _mp_fs_sd_response(mp_fs_sd_t *card) {
	int n;

	for(n = 0; n < _MP_FS_SD_RESPONSE; n++) {
		unsigned char got = mp_spi_xfer(&card->bus, 0xFF);

		if((got & 0x80) == 0)
			return(got);
	}
	return(0xFF);
}

/**
 * Send one command and read R1. tail, when set, receives the four bytes that
 * follow R1 (R3 and R7). Chip select stays low; the caller drops it.
 */
static unsigned char _mp_fs_sd_command(mp_fs_sd_t *card, unsigned char cmd, unsigned long arg, unsigned char *tail) {
	unsigned char frame[5];
	unsigned char r1;
	int i;

	frame[0] = (unsigned char)(0x40 | cmd);
	frame[1] = (unsigned char)(arg >> 24);
	frame[2] = (unsigned char)(arg >> 16);
	frame[3] = (unsigned char)(arg >> 8);
	frame[4] = (unsigned char)arg;
	_mp_fs_sd_select(card, 1);
	for(i = 0; i < 5; i++)
		mp_spi_xfer(&card->bus, frame[i]);
	mp_spi_xfer(&card->bus, _mp_fs_sd_crc7(frame, 5));
	r1 = _mp_fs_sd_response(card);
	if(tail) {
		for(i = 0; i < 4; i++)
			tail[i] = mp_spi_xfer(&card->bus, 0xFF);
	}
	return(r1);
}

static int _mp_fs_sd_token(mp_fs_sd_t *card, unsigned char want) {
	int n;

	for(n = 0; n < _MP_FS_SD_TOKEN; n++) {
		if(mp_spi_xfer(&card->bus, 0xFF) == want)
			return(1);
	}
	return(0);
}

static mp_ret_t _mp_fs_sd_register(mp_fs_sd_t *card, unsigned char cmd, unsigned char *reg) {
	unsigned short crc, got;
	unsigned char r1;
	int i;

	r1 = _mp_fs_sd_command(card, cmd, 0, NULL);
	if(r1 != 0x00 || !_mp_fs_sd_token(card, 0xFE)) {
		_mp_fs_sd_select(card, 0);
		return(FALSE);
	}
	for(i = 0; i < 16; i++)
		reg[i] = mp_spi_xfer(&card->bus, 0xFF);
	crc = _mp_fs_sd_crc16(reg, 16);
	got = (unsigned short)mp_spi_xfer(&card->bus, 0xFF) << 8;
	got |= mp_spi_xfer(&card->bus, 0xFF);
	_mp_fs_sd_select(card, 0);
	if(got != crc)
		return(FALSE);
	if(reg[15] != _mp_fs_sd_crc7(reg, 15))
		return(FALSE);
	return(TRUE);
}

static mp_ret_t _mp_fs_sd_capacity(const unsigned char *csd, unsigned long *sectors) {
	unsigned long structure = _mp_fs_sd_bits(csd, 127, 126);
	unsigned long c_size, mult, block_len, blocks;

	if(structure == 1) {
		c_size = _mp_fs_sd_bits(csd, 69, 48);
		/* (2^22) * 1024 is 2 TiB and does not fit in 32 bits. One sector short. */
		if(c_size >= 0x3FFFFFUL)
			*sectors = 0xFFFFFFFFUL;
		else
			*sectors = (c_size + 1UL) * 1024UL;
		return(*sectors ? TRUE : FALSE);
	}
	if(structure != 0)
		return(FALSE);
	block_len = _mp_fs_sd_bits(csd, 83, 80);
	c_size = _mp_fs_sd_bits(csd, 73, 62);
	mult = _mp_fs_sd_bits(csd, 49, 47);
	if(block_len < 9 || block_len > 11)
		return(FALSE);
	block_len = 1UL << block_len;
	mult = 1UL << (mult + 2);
	blocks = c_size + 1UL;
	if(mult > 0xFFFFFFFFUL / blocks)
		return(FALSE);
	blocks *= mult;
	if(block_len > 512) {
		unsigned long factor = block_len / 512UL;

		if(blocks > 0xFFFFFFFFUL / factor)
			return(FALSE);
		*sectors = blocks * factor;
	}
	else
		*sectors = blocks;
	return(*sectors ? TRUE : FALSE);
}

static void _mp_fs_sd_wait(unsigned long us) {
	unsigned long start = mp_clock_us();

	while((unsigned long)(mp_clock_us() - start) < us)
		;
}

static mp_ret_t _mp_fs_sd_wake(mp_fs_sd_t *card) {
	unsigned char tail[4];
	unsigned char r1;
	unsigned long arg;
	int v2 = 0;
	int n;

	_mp_fs_sd_wait(1000);
	_mp_fs_sd_select(card, 0);
	for(n = 0; n < 10; n++)
		mp_spi_xfer(&card->bus, 0xFF);

	r1 = _mp_fs_sd_command(card, 0, 0, NULL);
	_mp_fs_sd_select(card, 0);
	if(r1 != 0x01) {
		mp_printk("SD: no answer");
		return(FALSE);
	}

	r1 = _mp_fs_sd_command(card, 8, 0x1AAUL, tail);
	_mp_fs_sd_select(card, 0);
	if(r1 == 0x01) {
		if(tail[2] != 0x01 || tail[3] != 0xAA) {
			mp_printk("SD: CMD8 rejected");
			return(FALSE);
		}
		v2 = 1;
	}
	else if(r1 != 0x05) {
		mp_printk("SD: CMD8 rejected");
		return(FALSE);
	}

	arg = v2 ? 0x40000000UL : 0;
	for(n = 0; n < _MP_FS_SD_READY; n++) {
		r1 = _mp_fs_sd_command(card, 55, 0, NULL);
		_mp_fs_sd_select(card, 0);
		if(r1 != 0x01 && r1 != 0x00) {
			mp_printk("SD: card stays idle");
			return(FALSE);
		}
		r1 = _mp_fs_sd_command(card, 41, arg, NULL);
		_mp_fs_sd_select(card, 0);
		if(r1 == 0x00)
			break;
		if(r1 != 0x01) {
			mp_printk("SD: card stays idle");
			return(FALSE);
		}
	}
	if(r1 != 0x00) {
		mp_printk("SD: card stays idle");
		return(FALSE);
	}

	r1 = _mp_fs_sd_command(card, 58, 0, tail);
	_mp_fs_sd_select(card, 0);
	if(r1 != 0x00 || (tail[0] & 0x80) == 0) {
		mp_printk("SD: no OCR");
		return(FALSE);
	}
	card->block_address = (tail[0] & 0x40) ? 1 : 0;

	r1 = _mp_fs_sd_command(card, 16, 512, NULL);
	_mp_fs_sd_select(card, 0);
	if(r1 != 0x00) {
		mp_printk("SD: block length rejected");
		return(FALSE);
	}
	return(TRUE);
}

static void _mp_fs_sd_quiet(mp_fs_sd_t *card) {
	card->ready = NO;
	card->block.sectors = 0;
	card->block_address = 0;
	card->serial = 0;
	card->product[0] = 0;
}

static void _mp_fs_sd_lost(mp_fs_sd_t *card) {
	mp_bool_t was = card->ready;

	if(was == YES)
		mp_printk("SD: card gone");
	_mp_fs_sd_quiet(card);
	if(was == YES && card->on_media)
		card->on_media(card, NO, card->on_media_user);
}

/** The pin reads high when the socket is empty. No pin means the probe decides. */
static int _mp_fs_sd_switch(mp_fs_sd_t *card) {
	if(!card->cd)
		return(1);
	return(mp_gpio_read(card->cd) == YES ? 0 : 1);
}

static mp_ret_t _mp_fs_sd_usable(mp_fs_sd_t *card) {
	if(!card || card->ready != YES)
		return(FALSE);
	if(!_mp_fs_sd_switch(card)) {
		_mp_fs_sd_lost(card);
		return(FALSE);
	}
	return(TRUE);
}

static mp_ret_t _mp_fs_sd_read(mp_fs_block_t *block, unsigned long sector, unsigned char *data, unsigned long count) {
	mp_fs_sd_t *card = block->user;
	unsigned long n;

	if(_mp_fs_sd_usable(card) != TRUE)
		return(FALSE);
	for(n = 0; n < count; n++) {
		unsigned long addr = card->block_address ? sector + n : (sector + n) * 512UL;
		unsigned short crc, got;
		unsigned char r1;
		int i;
		int token;

		r1 = _mp_fs_sd_command(card, 17, addr, NULL);
		token = r1 == 0x00 && _mp_fs_sd_token(card, 0xFE);
		if(!token) {
			_mp_fs_sd_select(card, 0);
			if(r1 == 0xFF || r1 == 0x00)
				_mp_fs_sd_lost(card);
			return(FALSE);
		}
		for(i = 0; i < 512; i++)
			data[n * 512UL + i] = mp_spi_xfer(&card->bus, 0xFF);
		crc = _mp_fs_sd_crc16(data + n * 512UL, 512);
		got = (unsigned short)mp_spi_xfer(&card->bus, 0xFF) << 8;
		got |= mp_spi_xfer(&card->bus, 0xFF);
		_mp_fs_sd_select(card, 0);
		if(got != crc)
			return(FALSE);
	}
	return(TRUE);
}

static mp_ret_t _mp_fs_sd_write(mp_fs_block_t *block, unsigned long sector, const unsigned char *data, unsigned long count) {
	mp_fs_sd_t *card = block->user;
	unsigned long n;

	if(_mp_fs_sd_usable(card) != TRUE)
		return(FALSE);
	for(n = 0; n < count; n++) {
		unsigned long addr = card->block_address ? sector + n : (sector + n) * 512UL;
		unsigned short crc = _mp_fs_sd_crc16(data + n * 512UL, 512);
		unsigned char r1, answer;
		int i, tries;

		r1 = _mp_fs_sd_command(card, 24, addr, NULL);
		if(r1 != 0x00) {
			_mp_fs_sd_select(card, 0);
			if(r1 == 0xFF)
				_mp_fs_sd_lost(card);
			return(FALSE);
		}
		mp_spi_xfer(&card->bus, 0xFF);
		mp_spi_xfer(&card->bus, 0xFE);
		for(i = 0; i < 512; i++)
			mp_spi_xfer(&card->bus, data[n * 512UL + i]);
		mp_spi_xfer(&card->bus, (unsigned char)(crc >> 8));
		mp_spi_xfer(&card->bus, (unsigned char)crc);
		answer = 0xFF;
		for(tries = 0; tries < _MP_FS_SD_RESPONSE; tries++) {
			answer = mp_spi_xfer(&card->bus, 0xFF);
			if(answer != 0xFF)
				break;
		}
		if(answer == 0xFF) {
			_mp_fs_sd_select(card, 0);
			_mp_fs_sd_lost(card);
			return(FALSE);
		}
		if((answer & 0x1F) != 0x05) {
			_mp_fs_sd_select(card, 0);
			return(FALSE);
		}
		for(tries = 0; tries < _MP_FS_SD_BUSY; tries++) {
			if(mp_spi_xfer(&card->bus, 0xFF) == 0xFF)
				break;
		}
		_mp_fs_sd_select(card, 0);
		if(tries == _MP_FS_SD_BUSY)
			return(FALSE);
	}
	return(TRUE);
}

static mp_ret_t _mp_fs_sd_fail(mp_fs_sd_t *card) {
	if(card->cs) {
		mp_gpio_set(card->cs);
		mp_gpio_release(card->cs);
	}
	if(card->cd)
		mp_gpio_release(card->cd);
	if(card->bus.kernel)
		mp_spi_close(&card->bus);
	memset(card, 0, sizeof(*card));
	return(FALSE);
}

static mp_gpio_port_t *_mp_fs_sd_cs(mp_options_t *options, char *who) {
	char *text = mp_options_get(options, "cs");
	mp_gpio_port_t *pin;

	if(!text || !text[0])
		return(NULL);
	pin = mp_gpio_text_handle(text, who);
	if(!pin)
		return(NULL);
	mp_gpio_mode(pin, MP_GPIO_MODE_OUTPUT);
	mp_gpio_set(pin);
	return(pin);
}

static mp_ret_t _mp_fs_sd_identify(mp_fs_sd_t *card) {
	unsigned char reg[16];
	int i;

	if(_mp_fs_sd_wake(card) != TRUE)
		return(FALSE);
	if(_mp_fs_sd_register(card, 9, reg) != TRUE || _mp_fs_sd_capacity(reg, &card->block.sectors) != TRUE) {
		mp_printk("SD: CSD rejected");
		return(FALSE);
	}
	if(_mp_fs_sd_register(card, 10, reg) != TRUE) {
		mp_printk("SD: no CID");
		return(FALSE);
	}
	for(i = 0; i < 5; i++)
		card->product[i] = (char)_mp_fs_sd_bits(reg, 103 - i * 8, 96 - i * 8);
	card->product[5] = 0;
	card->serial = _mp_fs_sd_bits(reg, 55, 24);
	card->ready = YES;
	mp_printk("SD %s, %lu sectors%s", card->product, card->block.sectors,
		card->block_address ? ", block address" : "");
	if(card->on_media)
		card->on_media(card, YES, card->on_media_user);
	return(card->ready);
}

mp_bool_t mp_fs_sd_present(mp_fs_sd_t *card) {
	if(!card || !card->kernel)
		return(NO);
	if(card->cd)
		return(_mp_fs_sd_switch(card) ? YES : NO);
	return(card->ready);
}

mp_ret_t mp_fs_sd_probe(mp_fs_sd_t *card) {
	if(!card || !card->kernel)
		return(FALSE);
	if(!_mp_fs_sd_switch(card)) {
		if(card->ready == YES)
			_mp_fs_sd_lost(card);
		return(FALSE);
	}
	if(card->ready == YES)
		return(TRUE);
	if(_mp_fs_sd_identify(card) != TRUE) {
		_mp_fs_sd_quiet(card);
		return(FALSE);
	}
	return(TRUE);
}

mp_ret_t mp_fs_sd_init(mp_kernel_t *kernel, mp_fs_sd_t *card, mp_options_t *options, char *who) {
	char *miso;
	char *sck;
	char *mosi;
	char *cs;
	char *cd;

	if(!kernel || !card)
		return(FALSE);
	memset(card, 0, sizeof(*card));
	card->kernel = kernel;
	if(!who || !who[0])
		who = "SD";
	sck = mp_options_get(options, "sck");
	mosi = mp_options_get(options, "mosi");
	miso = mp_options_get(options, "miso");
	cs = mp_options_get(options, "cs");
	if(!sck || !sck[0] || !mosi || !mosi[0] || !miso || !miso[0] || !cs || !cs[0]) {
		mp_printk("SD: sck, mosi, miso and cs are required");
		return(_mp_fs_sd_fail(card));
	}
	if(mp_spi_open(kernel, &card->bus, options, who) != TRUE)
		return(_mp_fs_sd_fail(card));
	card->cs = _mp_fs_sd_cs(options, who);
	if(!card->cs) {
		mp_printk("SD: cs is not free");
		return(_mp_fs_sd_fail(card));
	}
	card->block.user = card;
	card->block.size = 512;
	card->block.read = _mp_fs_sd_read;
	card->block.write = _mp_fs_sd_write;
	cd = mp_options_get(options, "cd");
	if(cd && cd[0]) {
		card->cd = mp_gpio_text_handle(cd, who);
		if(!card->cd) {
			mp_printk("SD: cd is not free");
			return(_mp_fs_sd_fail(card));
		}
		mp_gpio_mode(card->cd, MP_GPIO_MODE_INPUT_PULL);
		mp_gpio_pull_up(card->cd);
	}
	if(mp_fs_sd_probe(card) != TRUE && card->cd && mp_fs_sd_present(card) == NO)
		mp_printk("SD: no card");
	return(TRUE);
}

void mp_fs_sd_fini(mp_fs_sd_t *card) {
	if(!card || !card->kernel)
		return;
	if(card->cs) {
		mp_gpio_set(card->cs);
		mp_gpio_release(card->cs);
	}
	if(card->cd)
		mp_gpio_release(card->cd);
	mp_spi_close(&card->bus);
	memset(card, 0, sizeof(*card));
}

#endif
