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

#ifdef SUPPORT_DRV_LEPTON

/**
@defgroup mpDrvLepton FLIR Lepton

@ingroup mpDriver

@{
*/

/* VoSPI packet: 2 bytes of id, 2 of CRC, 80 big-endian pixels */
#define LEPTON_PACKET 164
#define LEPTON_DISCARD 0x0F00
#define LEPTON_FAIL -1
#define LEPTON_SKIP -2
/*
 * A packet takes far less time than a line, so the module inserts discard
 * packets between real lines, and a whole frame of them can pass before line 0
 * comes around. Skipping only two frames worth of packets drops the picture.
 */
#define LEPTON_SKIP_LINE 128
#define LEPTON_SKIP_FRAME 1600

#define LEPTON_PHASE_BOOT 0
#define LEPTON_PHASE_SYNC 1
#define LEPTON_PHASE_RUN 2
/* nobody takes the frame: chip select high, only CCI runs */
#define LEPTON_PHASE_IDLE 3

#define LEPTON_REG_STATUS 0x0002
#define LEPTON_REG_COMMAND 0x0004
#define LEPTON_REG_LENGTH 0x0006
#define LEPTON_REG_DATA 0x0008
#define LEPTON_STATUS_BUSY 0x0001
#define LEPTON_STATUS_BOOT 0x0004
/*
 * The command word is the SDK command id. Bits 1:0 are the type, and a GET
 * leaves them at 0, so the id is written as it is. SYS FPA temperature is
 * 0x0214 and the answer is one word, kelvin times 100. The FLIR serial is
 * 0x0208, four words, most significant first. The part number is OEM 0x481C,
 * 32 ASCII bytes. The software version is OEM 0x4820, four words.
 */
#define LEPTON_CMD_SERIAL 0x0208
#define LEPTON_CMD_FPA 0x0214
#define LEPTON_CMD_PART 0x481C
#define LEPTON_CMD_VERSION 0x4820

/* half an I2C clock: about 100 kHz */
#define LEPTON_I2C_HALF 5000UL

/* survives close, so Devices > Camera keeps the chip name */
static char _lepton_kept[33];

static MP_TASK(_lepton_task);

static unsigned long _lepton_ticks(unsigned long ms) {
	unsigned long hz = mp_clock_tick_hz();
	unsigned long ticks = hz ? (ms * hz) / 1000 : ms;

	return(ticks ? ticks : 1);
}

static unsigned char _lepton_address(char *text) {
	unsigned int value = 0;

	if(!text || !*text)
		return(0x2A);
	if(text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
		text += 2;
	while(*text) {
		char c = *text++;

		value <<= 4;
		if(c >= '0' && c <= '9')
			value += (unsigned int)(c - '0');
		else if(c >= 'a' && c <= 'f')
			value += (unsigned int)(c - 'a' + 10);
		else if(c >= 'A' && c <= 'F')
			value += (unsigned int)(c - 'A' + 10);
		else
			return(0x2A);
	}
	if(value == 0 || value > 0x7F)
		return(0x2A);
	return((unsigned char)value);
}

/*
 * CCI is driven by hand on the sda and scl pins. On the F103xC/D/E the FSMC
 * of the screen holds PB7, the I2C1 SDA, high while its clock runs
 * (ES0340 2.3.9), so the I2C1 peripheral never gets an answer. One whole
 * command runs inside one pass of this task with the FSMC stopped; no other
 * task can draw meanwhile. A released line is an input with its pull-up.
 */
static void _lepton_line(mp_gpio_port_t *pin, int high) {
	if(high) {
		mp_gpio_set(pin);
		mp_gpio_mode(pin, MP_GPIO_MODE_INPUT_PULL);
	}
	else {
		mp_gpio_mode(pin, MP_GPIO_MODE_OUTPUT_OD);
		mp_gpio_unset(pin);
	}
	mp_clock_nanoDelay(LEPTON_I2C_HALF);
}

/* the module may hold scl low while it prepares the byte */
static int _lepton_scl_high(mp_drv_lepton_t *lep) {
	int spin;

	_lepton_line(lep->scl, 1);
	for(spin = 0; spin < 4000 && !mp_gpio_raw_read(lep->scl); spin++)
		mp_clock_nanoDelay(LEPTON_I2C_HALF);
	return(mp_gpio_raw_read(lep->scl) ? 1 : 0);
}

static void _lepton_i2c_start(mp_drv_lepton_t *lep) {
	_lepton_line(lep->sda, 1);
	_lepton_scl_high(lep);
	_lepton_line(lep->sda, 0);
	_lepton_line(lep->scl, 0);
}

static void _lepton_i2c_stop(mp_drv_lepton_t *lep) {
	_lepton_line(lep->sda, 0);
	_lepton_scl_high(lep);
	_lepton_line(lep->sda, 1);
}

/* 1 when the module acknowledged */
static int _lepton_i2c_put(mp_drv_lepton_t *lep, unsigned char byte) {
	int bit;
	int ack;

	for(bit = 7; bit >= 0; bit--) {
		_lepton_line(lep->sda, (byte >> bit) & 1);
		if(!_lepton_scl_high(lep))
			return(0);
		_lepton_line(lep->scl, 0);
	}
	_lepton_line(lep->sda, 1);
	if(!_lepton_scl_high(lep))
		return(0);
	ack = mp_gpio_raw_read(lep->sda) ? 0 : 1;
	_lepton_line(lep->scl, 0);
	return(ack);
}

/* 0 when the module held the clock too long; the transfer is then dropped */
static int _lepton_i2c_get(mp_drv_lepton_t *lep, unsigned char *byte, int more) {
	int bit;

	*byte = 0;
	_lepton_line(lep->sda, 1);
	for(bit = 0; bit < 8; bit++) {
		if(!_lepton_scl_high(lep))
			return(0);
		*byte = (unsigned char)((*byte << 1) | (mp_gpio_raw_read(lep->sda) ? 1 : 0));
		_lepton_line(lep->scl, 0);
	}
	_lepton_line(lep->sda, more ? 0 : 1);
	if(!_lepton_scl_high(lep))
		return(0);
	_lepton_line(lep->scl, 0);
	_lepton_line(lep->sda, 1);
	return(1);
}

/* reg is sent first, most significant byte first, then n bytes come back */
static int _lepton_cci_read(mp_drv_lepton_t *lep, unsigned short reg, unsigned char *dst, int n) {
	int i;

	_lepton_i2c_start(lep);
	if(!_lepton_i2c_put(lep, (unsigned char)(lep->address << 1)) ||
		!_lepton_i2c_put(lep, (unsigned char)(reg >> 8)) ||
		!_lepton_i2c_put(lep, (unsigned char)reg)) {
		_lepton_i2c_stop(lep);
		return(0);
	}
	_lepton_i2c_start(lep);
	if(!_lepton_i2c_put(lep, (unsigned char)((lep->address << 1) | 1))) {
		_lepton_i2c_stop(lep);
		return(0);
	}
	for(i = 0; i < n; i++) {
		if(!_lepton_i2c_get(lep, &dst[i], i + 1 < n)) {
			_lepton_i2c_stop(lep);
			return(0);
		}
	}
	_lepton_i2c_stop(lep);
	return(1);
}

static int _lepton_cci_write(mp_drv_lepton_t *lep, unsigned short reg, unsigned short value) {
	int ok;

	_lepton_i2c_start(lep);
	ok = _lepton_i2c_put(lep, (unsigned char)(lep->address << 1)) &&
		_lepton_i2c_put(lep, (unsigned char)(reg >> 8)) &&
		_lepton_i2c_put(lep, (unsigned char)reg) &&
		_lepton_i2c_put(lep, (unsigned char)(value >> 8)) &&
		_lepton_i2c_put(lep, (unsigned char)value);
	_lepton_i2c_stop(lep);
	return(ok);
}

static unsigned short _lepton_word(unsigned char *raw) {
	return((unsigned short)((raw[0] << 8) | raw[1]));
}

/* status until not busy; 0 when the module does not answer or stays busy */
static int _lepton_cci_idle(mp_drv_lepton_t *lep, unsigned short *status, int polls) {
	unsigned char raw[2];

	while(polls-- > 0) {
		if(!_lepton_cci_read(lep, LEPTON_REG_STATUS, raw, 2))
			return(0);
		*status = _lepton_word(raw);
		if(!(*status & LEPTON_STATUS_BUSY))
			return(1);
		mp_clock_nanoDelay(100000UL);
	}
	return(0);
}

/*
 * One GET, as the SDK does it: wait not busy, write the word count, write
 * the command, wait not busy, check the error byte, read the data words
 * into cciRx. 1 when the answer is there.
 */
static int _lepton_cci_get(mp_drv_lepton_t *lep, unsigned short cmd, int words) {
	unsigned short status = 0;
	mp_bool_t fsmc;
	int ok = 0;

	if(words * 2 > (int)sizeof(lep->cciRx))
		return(0);
	fsmc = mp_fsmc_pause();
	if(_lepton_cci_idle(lep, &status, 10) &&
		_lepton_cci_write(lep, LEPTON_REG_LENGTH, (unsigned short)words) &&
		_lepton_cci_write(lep, LEPTON_REG_COMMAND, cmd) &&
		_lepton_cci_idle(lep, &status, 200) &&
		!(status & 0xFF00))
		ok = _lepton_cci_read(lep, LEPTON_REG_DATA, lep->cciRx, words * 2);
	_lepton_line(lep->sda, 1);
	_lepton_line(lep->scl, 1);
	mp_fsmc_resume(fsmc);
	if(!ok && lep->gaveUp == NO) {
		mp_printk("Lepton: command 0x%04x status 0x%04x", (unsigned int)cmd, (unsigned int)status);
		lep->gaveUp = YES;
	}
	return(ok);
}

static int _lepton_filled(const unsigned char *raw, int n) {
	int i;

	for(i = 0; i < n; i++)
		if(raw[i])
			return(1);
	return(0);
}

static int _lepton_text(char *dst, int cap, const unsigned char *raw, int n, int swap) {
	int i;
	int o = 0;

	for(i = 0; i < n && o + 1 < cap; i++) {
		unsigned char c = raw[swap ? (i ^ 1) : i];

		if(c == 0)
			break;
		if(c < 0x20 || c > 0x7e)
			break;
		dst[o++] = (char)c;
	}
	while(o > 0 && dst[o - 1] == ' ')
		o--;
	dst[o] = 0;
	return(o);
}

/* the OEM string somewhere in the buffer; a swapped word does not start with 500- */
static const char *_lepton_part_at(const char *text) {
	if(!text)
		return(NULL);
	while(text[0]) {
		if(text[0] == '5' && text[1] == '0' && text[2] == '0' && text[3] == '-')
			return(text);
		text++;
	}
	return(NULL);
}

/* FLIR OEM part numbers (datasheet 500-0659-00-09 and the current FLIR list) */
static const struct {
	const char *part;
	const char *name;
} _lepton_names[] = {
	{ "500-0643-00", "FLIR Lepton 1.5" },
	{ "500-0643-01", "FLIR Lepton 1.5" },
	{ "500-0690-00", "FLIR Lepton 1.6" },
	{ "500-0659-00", "FLIR Lepton 2" },
	{ "500-0659-01", "FLIR Lepton 2" },
	{ "500-0763-01", "FLIR Lepton 2.5" },
	{ "500-0726-01", "FLIR Lepton 3" },
	{ "500-0758-03", "FLIR Lepton 3.1R" },
	{ "500-0771-01", "FLIR Lepton 3.5" },
	{ "500-0771-FS1", "FLIR Lepton FS1" },
	{ "500-1387-00", "FLIR Lepton UW" },
	{ "500-1395-00", "FLIR Lepton XDS" }
};

/* a part number the table does not know is still a Lepton */
static const char *_lepton_name(const char *part) {
	int i;

	if(!part || !part[0])
		return(NULL);
	for(i = 0; i < (int)(sizeof(_lepton_names) / sizeof(_lepton_names[0])); i++) {
		size_t n = strlen(_lepton_names[i].part);

		if(strncmp(part, _lepton_names[i].part, n) == 0 && (part[n] == 0 || part[n] == '-' || part[n] == ' '))
			return(_lepton_names[i].name);
	}
	return("FLIR Lepton");
}

/* the module's own name, part, serial and version. The board option is not a name. */
static void _lepton_identity(mp_drv_lepton_t *lep) {
	const char *model = _lepton_kept[0] ? _lepton_kept : (lep->model[0] ? lep->model : NULL);
	const char *serial = lep->serial[0] ? lep->serial : NULL;
	const char *version = lep->version[0] ? lep->version : NULL;

	if(model)
		lep->label = _lepton_name(model);
	if(!lep->label)
		return;
	mp_device_identify(lep, lep->label, model, serial, version);
	/* load is NULL, so loaded stays NO; the driver is still registered */
	if(model && lep->binding.kernel == lep->kernel &&
		mp_device_registry_register(&lep->binding, &lep->tempDev, MP_DEVICE_TEMPERATURE, 0, &lep->temperature, model, serial ? serial : lep->askedSerial, version))
		lep->tempDev.label = lep->label;
}

static int _lepton_take_serial(mp_drv_lepton_t *lep) {
	int i;

	if(_lepton_filled(lep->cciRx, 8) == 0)
		return(0);
	for(i = 0; i < 8; i++)
		snprintf(lep->serial + i * 2, 3, "%02x", lep->cciRx[i]);
	lep->serial[16] = 0;
	mp_printk("Lepton serial %s", lep->serial);
	_lepton_identity(lep);
	return(1);
}

static int _lepton_take_part(mp_drv_lepton_t *lep) {
	char swapped[33];
	char plain[33];
	const char *plainAt;
	const char *swappedAt;
	const char *use;

	_lepton_text(swapped, (int)sizeof(swapped), lep->cciRx, 32, 1);
	_lepton_text(plain, (int)sizeof(plain), lep->cciRx, 32, 0);
	plainAt = _lepton_part_at(plain);
	swappedAt = _lepton_part_at(swapped);
	if(plainAt == plain)
		use = plain;
	else if(swappedAt == swapped)
		use = swapped;
	else
		use = plainAt ? plainAt : swappedAt;
	if(!use)
		return(0);
	snprintf(_lepton_kept, sizeof(_lepton_kept), "%s", use);
	snprintf(lep->model, sizeof(lep->model), "%s", use);
	mp_printk("Lepton model %s", lep->model);
	_lepton_identity(lep);
	return(1);
}

static int _lepton_take_version(mp_drv_lepton_t *lep) {
	const unsigned char *raw = lep->cciRx;

	/*
	 * Each word arrives most significant byte first. The SDK stores the
	 * swapped word, so gpp major/minor/build are raw[1], raw[0], raw[3]
	 * and the dsp triple follows one word later.
	 */
	snprintf(lep->version, sizeof(lep->version), "gpp %u.%u.%u  dsp %u.%u.%u",
		raw[1], raw[0], raw[3], raw[2], raw[5], raw[4]);
	mp_printk("Lepton version %s", lep->version);
	_lepton_identity(lep);
	return(1);
}

#define LEPTON_DMA_IDLE 0
#define LEPTON_DMA_OK 1
#define LEPTON_DMA_FAIL 2

static int _lepton_number(int id);
static int _lepton_blank(mp_drv_lepton_t *lep);
static void _lepton_store(mp_drv_lepton_t *lep, int row);
static void _lepton_span_commit(mp_drv_lepton_t *lep);
static mp_bool_t _lepton_fail(mp_drv_lepton_t *lep);

#ifdef SUPPORT_COMMON_DMA
static void _lepton_dma_kick(mp_drv_lepton_t *lep);
static void _lepton_dma_finish(mp_drv_lepton_t *lep, signed char result);
#endif

static void _lepton_dma_stop(mp_drv_lepton_t *lep) {
#ifdef SUPPORT_COMMON_DMA
	if(lep->dmaUse == NO)
		return;
	lep->dmaOn = NO;
	mp_dma_stop(&lep->dmaRx);
	mp_dma_stop(&lep->dmaTx);
	mp_dma_unmap(&lep->dmaRx);
	mp_dma_unmap(&lep->dmaTx);
	lep->dmaUse = NO;
	if(lep->cs)
		mp_gpio_set(lep->cs);
#else
	(void)lep;
#endif
}

#ifdef SUPPORT_COMMON_DMA

static void _lepton_dma_finish(mp_drv_lepton_t *lep, signed char result) {
	lep->dmaOn = NO;
	lep->dmaResult = result;
	mp_dma_stop(&lep->dmaRx);
	mp_dma_stop(&lep->dmaTx);
	if(result != LEPTON_DMA_OK)
		_lepton_fail(lep);
	if(lep->cs)
		mp_gpio_set(lep->cs);
	if(lep->task)
		mp_task_wake(lep->task);
}

/* one packet. The RX completion is the end of it; TX has no callback. */
static void _lepton_dma_kick(mp_drv_lepton_t *lep) {
	mp_gpio_unset(lep->cs);
	if(mp_dma_start(&lep->dmaRx) == FALSE || mp_dma_start(&lep->dmaTx) == FALSE) {
		mp_dma_stop(&lep->dmaRx);
		mp_dma_stop(&lep->dmaTx);
		_lepton_dma_finish(lep, LEPTON_DMA_FAIL);
	}
}

static void _lepton_dma_done(mp_dma_t *dma, mp_dma_result_t result) {
	mp_drv_lepton_t *lep = dma->user;
	SPI_TypeDef *regs;
	unsigned long spin;
	int line;

	if(lep->dmaOn == NO)
		return;
	/* the TX interrupt may still be pending; the next kick needs the engine idle */
	mp_dma_stop(&lep->dmaTx);
	if(result != MP_DMA_DONE) {
		_lepton_dma_finish(lep, LEPTON_DMA_FAIL);
		return;
	}

	regs = lep->spi.regs;
	spin = 2000UL;
	while((regs->SR & SPI_SR_BSY) && --spin)
		;
	mp_gpio_set(lep->cs);
	if(spin == 0) {
		_lepton_dma_finish(lep, LEPTON_DMA_FAIL);
		return;
	}

	if(lep->task && lep->task->signal == MP_TASK_SIG_STOP) {
		_lepton_dma_finish(lep, LEPTON_DMA_FAIL);
		return;
	}

	line = _lepton_number(((int)lep->packet[0] << 8) | lep->packet[1]);
	if(line == LEPTON_FAIL) {
		_lepton_dma_finish(lep, LEPTON_DMA_FAIL);
		return;
	}
	if(line != LEPTON_SKIP && _lepton_blank(lep))
		line = LEPTON_SKIP;

	if(lep->dmaRow < 0) {
		if(line == 0) {
			_lepton_store(lep, 0);
			lep->dmaRow = 1;
			lep->dmaSkips = 0;
		}
		else if(++lep->dmaSkips >= LEPTON_SKIP_FRAME) {
			_lepton_dma_finish(lep, LEPTON_DMA_FAIL);
			return;
		}
	}
	else if(line == lep->dmaRow) {
		_lepton_store(lep, lep->dmaRow);
		lep->dmaSkips = 0;
		if(++lep->dmaRow >= MP_DRV_LEPTON_HEIGHT) {
			_lepton_span_commit(lep);
			lep->frame.sequence++;
			_lepton_dma_finish(lep, LEPTON_DMA_OK);
			return;
		}
	}
	else if(line != LEPTON_SKIP || ++lep->dmaSkips >= LEPTON_SKIP_LINE) {
		_lepton_dma_finish(lep, LEPTON_DMA_FAIL);
		return;
	}

	if(lep->dmaOn == YES)
		_lepton_dma_kick(lep);
}

static void _lepton_dma_open(mp_drv_lepton_t *lep) {
	mp_dma_engine_t *rx;
	mp_dma_engine_t *tx;

	rx = mp_spi_engine(&lep->spi, MP_DMA_FROM_DEVICE);
	tx = mp_spi_engine(&lep->spi, MP_DMA_TO_DEVICE);
	if(!rx || !tx) {
		mp_spi_engine_drop(&lep->spi);
		mp_printk("Lepton: no SPI DMA, reading in the task");
		return;
	}

	lep->rxSeg.cpu = lep->packet;
	lep->rxSeg.len = LEPTON_PACKET;
	lep->rxSeg.fixed = NO;
	lep->txSeg.cpu = &lep->dmaZero;
	lep->txSeg.len = LEPTON_PACKET;
	lep->txSeg.fixed = YES;

	if(mp_dma_bind(&lep->dmaRx, rx) == FALSE || mp_dma_bind(&lep->dmaTx, tx) == FALSE) {
		mp_spi_engine_drop(&lep->spi);
		mp_printk("Lepton: no SPI DMA, reading in the task");
		return;
	}
	lep->dmaRx.dir = MP_DMA_FROM_DEVICE;
	lep->dmaRx.in = &lep->rxSeg;
	lep->dmaRx.nin = 1;
	lep->dmaRx.done = _lepton_dma_done;
	lep->dmaRx.user = lep;
	lep->dmaTx.dir = MP_DMA_TO_DEVICE;
	lep->dmaTx.out = &lep->txSeg;
	lep->dmaTx.nout = 1;

	if(mp_dma_map(&lep->dmaRx) == FALSE || mp_dma_map(&lep->dmaTx) == FALSE) {
		mp_dma_unmap(&lep->dmaRx);
		mp_dma_unmap(&lep->dmaTx);
		mp_spi_engine_drop(&lep->spi);
		mp_printk("Lepton: no SPI DMA, reading in the task");
		return;
	}
	lep->dmaUse = YES;
}

#endif

static int _lepton_transfer(mp_drv_lepton_t *lep) {
	SPI_TypeDef *regs = lep->spi.regs;
	int i;

	if(regs->SR & SPI_SR_OVR) {
		(void)regs->DR;
		(void)regs->SR;
	}

	mp_gpio_unset(lep->cs);
	for(i = 0; i < LEPTON_PACKET; i++) {
		unsigned long spin = 100000UL;

		mp_spi_tx(&lep->spi, 0x00);
		while(!(regs->SR & SPI_SR_RXNE)) {
			if(--spin == 0) {
				mp_gpio_set(lep->cs);
				return(LEPTON_FAIL);
			}
		}
		lep->packet[i] = mp_spi_rx(&lep->spi);
	}
	while(regs->SR & SPI_SR_BSY);
	mp_gpio_set(lep->cs);
	return((int)lep->packet[0] << 8) | lep->packet[1];
}

static int _lepton_number(int id) {
	if(id < 0)
		return(LEPTON_FAIL);
	if((id & LEPTON_DISCARD) == LEPTON_DISCARD)
		return(LEPTON_SKIP);
	/* a Lepton 2 line number lives in the low 12 bits and stops at 59 */
	if((id & 0x0FFF) >= MP_DRV_LEPTON_HEIGHT)
		return(LEPTON_FAIL);
	return(id & 0x0FFF);
}

/* an idle MISO reads as line 0. A real line is never all zeros. */
static int _lepton_blank(mp_drv_lepton_t *lep) {
	int i;

	for(i = 4; i < LEPTON_PACKET; i++)
		if(lep->packet[i])
			return(0);
	return(1);
}

static void _lepton_span_begin(mp_drv_lepton_t *lep) {
	lep->seenMin = 0x3fff;
	lep->seenMax = 0;
}

/* the next frame uses this one's raw ends, so a narrow scene fills the byte */
static void _lepton_span_commit(mp_drv_lepton_t *lep) {
	unsigned int span;

	if(lep->seenMax <= lep->seenMin + 16)
		return;
	span = lep->seenMax - lep->seenMin;
	lep->bias = lep->seenMin;
	lep->scale = (255u << 16) / span;
}

static void _lepton_store(mp_drv_lepton_t *lep, int row) {
	unsigned char *dst = lep->pixels + row * MP_DRV_LEPTON_WIDTH;
	unsigned short lo = lep->bias;
	unsigned int scale = lep->scale;
	int i;

	if(!scale)
		scale = (255u << 16) / 16383u;
	for(i = 0; i < MP_DRV_LEPTON_WIDTH; i++) {
		unsigned char *src = lep->packet + 4 + (i * 2);
		unsigned short sample = ((unsigned short)src[0] << 8) | src[1];
		unsigned int level;

		if(sample) {
			if(sample < lep->seenMin)
				lep->seenMin = sample;
			if(sample > lep->seenMax)
				lep->seenMax = sample;
		}
		if(sample <= lo)
			dst[i] = 0;
		else {
			level = ((unsigned int)(sample - lo) * scale) >> 16;
			dst[i] = level > 255u ? 255 : (unsigned char)level;
		}
	}
}

/*
 * A failed read has already replaced the top rows. Rolling the sequence
 * back to even would publish that cut. Leave it odd until a full frame ends.
 */
static mp_bool_t _lepton_fail(mp_drv_lepton_t *lep) {
	(void)lep;
	return(NO);
}

static int _lepton_pull(mp_drv_lepton_t *lep, mp_task_t *task) {
	int line;

	if(task->signal == MP_TASK_SIG_STOP)
		return(LEPTON_FAIL);
	line = _lepton_number(_lepton_transfer(lep));
	if(line != LEPTON_SKIP && line != LEPTON_FAIL && _lepton_blank(lep))
		return(LEPTON_SKIP);
	return(line);
}

/* line 0, whenever it shows up. Any other line is still this frame or the one before. */
static int _lepton_seek_row(mp_drv_lepton_t *lep, mp_task_t *task) {
	int skips = 0;
	int line;

	while(skips < LEPTON_SKIP_FRAME) {
		line = _lepton_pull(lep, task);
		if(line == LEPTON_FAIL || line == 0)
			return(line);
		skips++;
	}
	return(LEPTON_SKIP);
}

/* the next line. Discards between lines are normal; a different line means the frame broke. */
static int _lepton_take_row(mp_drv_lepton_t *lep, mp_task_t *task, int want) {
	int skips = 0;
	int line;

	while(skips < LEPTON_SKIP_LINE) {
		line = _lepton_pull(lep, task);
		if(line == LEPTON_FAIL || line == want)
			return(line);
		if(line != LEPTON_SKIP)
			return(line);
		skips++;
	}
	return(LEPTON_SKIP);
}

static mp_bool_t _lepton_frame(mp_drv_lepton_t *lep, mp_task_t *task) {
	int line;
	int row;

	if((lep->frame.sequence & 1UL) == 0)
		lep->frame.sequence++;
	_lepton_span_begin(lep);

	/*
	 * No yield in here. Chip select frames one packet, and a gap after line 0
	 * makes the module drop the rest of the frame.
	 */
	line = _lepton_seek_row(lep, task);
	if(line != 0)
		return(_lepton_fail(lep));

	_lepton_store(lep, 0);
	for(row = 1; row < MP_DRV_LEPTON_HEIGHT; row++) {
		line = _lepton_take_row(lep, task, row);
		if(line != row)
			return(_lepton_fail(lep));
		_lepton_store(lep, row);
	}

	lep->frame.sequence++;
	_lepton_span_commit(lep);
	return(YES);
}

/* once a second: what is still unknown of the identity, then the temperature */
static void _lepton_cci(mp_drv_lepton_t *lep, unsigned long now) {
	unsigned short kelvin;

	if(lep->tempAt && (long)(now - lep->tempAt) < (long)_lepton_ticks(1000))
		return;
	lep->tempAt = now;
	if(!lep->model[0]) {
		if(!lep->serial[0] && _lepton_cci_get(lep, LEPTON_CMD_SERIAL, 4))
			_lepton_take_serial(lep);
		if(_lepton_cci_get(lep, LEPTON_CMD_PART, 16))
			_lepton_take_part(lep);
		return;
	}
	if(!lep->version[0]) {
		if(_lepton_cci_get(lep, LEPTON_CMD_VERSION, 4))
			_lepton_take_version(lep);
		return;
	}
	if(!_lepton_cci_get(lep, LEPTON_CMD_FPA, 1))
		return;
	kelvin = _lepton_word(lep->cciRx);
	/* the word is kelvin times 100; a cold module is still above 200 K */
	if(lep->tempDev.driver && kelvin >= 20000 && kelvin <= 40000)
		mp_device_measure_publish(&lep->temperature, (float)kelvin / 100.0f - 273.15f);
}

/* the driver of the temperature; the device itself waits for the part number */
static void _lepton_publish(mp_drv_lepton_t *lep) {
	static const mp_device_driver_ops_t ops = {
		"camera/lepton", "FLIR Lepton", "1.0.0",
		(unsigned int)(1u << MP_DEVICE_TEMPERATURE),
		NULL, NULL, NULL, NULL, NULL
	};

	if(lep->binding.kernel != lep->kernel && mp_device_driver_register(lep->kernel, &lep->binding, &ops, lep, NULL) != TRUE)
		mp_printk("Lepton: can not register the temperature");
	_lepton_identity(lep);
	mp_printk("Lepton ready");
}

static MP_TASK(_lepton_task) {
	mp_drv_lepton_t *lep = task->user;
	unsigned long now;
	int watched;

	if(task->signal == MP_TASK_SIG_STOP) {
		_lepton_dma_stop(lep);
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	now = mp_clock_ticks();
	if(lep->phase == LEPTON_PHASE_BOOT) {
		lep->phase = LEPTON_PHASE_IDLE;
		lep->bootAt = now;
		mp_gpio_set(lep->cs);
		_lepton_publish(lep);
		mp_task_delay(task, _lepton_ticks(185));
		return;
	}

	/*
	 * No command while frames stream: a command in the middle of VoSPI
	 * left the module busy for good, and only its power brings it back.
	 */
	watched = lep->watchAt && (long)(now - lep->watchAt) < (long)_lepton_ticks(500);
#ifdef SUPPORT_COMMON_DMA
	if(!watched && (lep->dmaUse == NO || lep->dmaOn == NO))
#else
	if(!watched)
#endif
		_lepton_cci(lep, now);
	/* a module that never answers on I2C is still listed, under the family name */
	if(!lep->label && (long)(now - lep->bootAt) > (long)_lepton_ticks(6000)) {
		lep->label = "FLIR Lepton";
		mp_printk("Lepton: no part number on I2C");
		_lepton_identity(lep);
	}

	/* video only while someone takes the frame */
	if(!watched) {
#ifdef SUPPORT_COMMON_DMA
		if(lep->dmaUse == YES && lep->dmaOn == YES)
			_lepton_dma_finish(lep, LEPTON_DMA_FAIL);
		if(lep->dmaUse == YES)
			lep->dmaResult = LEPTON_DMA_IDLE;
#endif
		mp_gpio_set(lep->cs);
		lep->phase = LEPTON_PHASE_IDLE;
		if(lep->power.held == YES)
			mp_power_release(lep->kernel, &lep->power);
		mp_task_delay(task, _lepton_ticks(100));
		return;
	}
	if(lep->power.held != YES)
		mp_power_hold(lep->kernel, &lep->power, MP_POWER_HIGH, "Lepton");
	if(lep->phase == LEPTON_PHASE_IDLE)
		lep->phase = LEPTON_PHASE_SYNC;

	/* chip select high long enough for the module to resync, then frames */
	if(lep->phase == LEPTON_PHASE_SYNC) {
		mp_gpio_set(lep->cs);
		lep->phase = LEPTON_PHASE_RUN;
		mp_task_delay(task, _lepton_ticks(185));
		return;
	}

#ifdef SUPPORT_COMMON_DMA
	if(lep->dmaUse == YES) {
		/* the interrupt is the only thing that clears dmaOn. A lost one would sleep forever. */
		if(lep->dmaOn == YES) {
			_lepton_dma_finish(lep, LEPTON_DMA_FAIL);
			return;
		}
		if(lep->dmaResult == LEPTON_DMA_OK) {
			lep->dmaResult = LEPTON_DMA_IDLE;
			lep->booted = YES;
			if(lep->frame.sequence == 2)
				mp_printk("Lepton frame");
			/* the screen paints this frame in bands before the next read replaces it */
			mp_task_delay(task, _lepton_ticks(80));
			return;
		}
		if(lep->dmaResult == LEPTON_DMA_FAIL) {
			lep->dmaResult = LEPTON_DMA_IDLE;
			lep->phase = LEPTON_PHASE_SYNC;
			mp_task_delay(task, _lepton_ticks(1));
			return;
		}

		if((lep->frame.sequence & 1UL) == 0)
			lep->frame.sequence++;
		_lepton_span_begin(lep);
		lep->dmaRow = -1;
		lep->dmaSkips = 0;
		lep->dmaOn = YES;
		_lepton_dma_kick(lep);
		if(lep->dmaOn == NO)
			return;
		mp_task_delay(task, _lepton_ticks(1000));
		return;
	}
#endif
	if(_lepton_frame(lep, task) == NO) {
		lep->phase = LEPTON_PHASE_SYNC;
		mp_task_delay(task, _lepton_ticks(1));
		return;
	}

	lep->booted = YES;
	if(lep->frame.sequence == 2)
		mp_printk("Lepton frame");
	/* the screen paints this frame in bands before the next read replaces it */
	mp_task_delay(task, _lepton_ticks(80));
}

static mp_ret_t _lepton_bus(mp_kernel_t *kernel, mp_drv_lepton_t *lep, mp_options_t *options, char *who) {
	mp_options_t spiOpen[] = {
		{ "gate", mp_options_get(options, "spiGate") },
		{ "simo", mp_options_get(options, "simo") },
		{ "somi", mp_options_get(options, "somi") },
		{ "clk", mp_options_get(options, "sck") },
		{ NULL, NULL }
	};
	/* mode 3: polarity defaults to idle high, phase "change" is CPHA 1 */
	mp_options_t spiSetup[] = {
		{ "frequency", "10000000" },
		{ "phase", "change" },
		{ "role", "master" },
		{ NULL, NULL }
	};
	if(mp_spi_open(kernel, &lep->spi, spiOpen, who) == FALSE)
		return(FALSE);
	if(mp_spi_setup(&lep->spi, spiSetup) == FALSE)
		return(FALSE);

	lep->sda = mp_gpio_text_handle(mp_options_get(options, "sda"), "Lepton SDA");
	lep->scl = mp_gpio_text_handle(mp_options_get(options, "scl"), "Lepton SCL");
	if(!lep->sda || !lep->scl) {
		mp_printk("Lepton: need sda and scl");
		return(FALSE);
	}
	_lepton_line(lep->sda, 1);
	_lepton_line(lep->scl, 1);
	return(TRUE);
}

mp_ret_t mp_drv_lepton_init(mp_kernel_t *kernel, mp_drv_lepton_t *lep, mp_options_t *options, char *who) {
	const char *model;
	const char *serial;

	memset(lep, 0, sizeof(*lep));
	lep->kernel = kernel;
	model = mp_device_option(options, "model");
	serial = mp_machine_serial();
	if(!model || !serial) {
		mp_printk("Lepton: model is required, and the board serial");
		return(FALSE);
	}
	lep->askedModel = model;
	lep->askedSerial = serial;
	_lepton_identity(lep);
	lep->address = _lepton_address(mp_options_get(options, "address"));
	lep->frame.pixels = lep->pixels;
	lep->frame.width = MP_DRV_LEPTON_WIDTH;
	lep->frame.height = MP_DRV_LEPTON_HEIGHT;
	lep->scale = (255u << 16) / 16383u;
	_lepton_span_begin(lep);

	if(!who || !*who)
		who = "Lepton";

	lep->cs = mp_gpio_text_handle(mp_options_get(options, "cs"), "Lepton CS");
	if(!lep->cs) {
		mp_printk("Lepton: need a chip select");
		return(FALSE);
	}
	/* high before the pin becomes an output, so the select does not pulse */
	mp_gpio_set(lep->cs);
	mp_gpio_mode(lep->cs, MP_GPIO_MODE_OUTPUT);

	if(_lepton_bus(kernel, lep, options, who) == FALSE) {
		mp_drv_lepton_fini(lep);
		return(FALSE);
	}
#ifdef SUPPORT_COMMON_DMA
	_lepton_dma_open(lep);
#endif

	lep->task = mp_task_create(&kernel->tasks, "Lepton", _lepton_task, lep, _lepton_ticks(50));
	if(!lep->task) {
		mp_printk("Lepton: can not create the task");
		mp_drv_lepton_fini(lep);
		return(FALSE);
	}
	return(TRUE);
}

void mp_drv_lepton_fini(mp_drv_lepton_t *lep) {
	if(lep->power.held == YES)
		mp_power_release(lep->kernel, &lep->power);
	_lepton_dma_stop(lep);
	if(lep->task) {
		mp_task_destroy(lep->task);
		lep->task = NULL;
	}

	mp_spi_close(&lep->spi);
	if(lep->sda) {
		mp_gpio_mode(lep->sda, MP_GPIO_MODE_INPUT);
		mp_gpio_release(lep->sda);
		lep->sda = NULL;
	}
	if(lep->scl) {
		mp_gpio_mode(lep->scl, MP_GPIO_MODE_INPUT);
		mp_gpio_release(lep->scl);
		lep->scl = NULL;
	}

	if(lep->cs) {
		mp_gpio_set(lep->cs);
		mp_gpio_release(lep->cs);
		lep->cs = NULL;
	}

	mp_device_registry_unregister(&lep->tempDev);
	mp_device_driver_unregister(lep->kernel, &lep->binding);
}

#ifdef SUPPORT_COMMON_DEVICE
static mp_ret_t _lepton_device_open(mp_kernel_t *kernel, void *self, mp_options_t *options, char *who) {
	return(mp_drv_lepton_init(kernel, self, options, who));
}

static void _lepton_device_close(void *self) {
	mp_drv_lepton_t *lep = self;

	if(lep->power.held == YES)
		mp_power_release(lep->kernel, &lep->power);
	mp_drv_lepton_fini(self);
}

/* taking the frame is what keeps the video running */
static void *_lepton_device_face(void *self) {
	mp_drv_lepton_t *lep = self;

	lep->watchAt = mp_clock_ticks();
	if(!lep->watchAt)
		lep->watchAt = 1;
	if(lep->phase == LEPTON_PHASE_IDLE && lep->task)
		mp_task_wake(lep->task);
	return(&lep->frame);
}

const mp_device_ops_t mp_drv_lepton_device = {
	"LEPTON", MP_DEVICE_CAMERA, _lepton_device_open, _lepton_device_close, _lepton_device_face, NULL,
	"FLIR Lepton", "1.0.0"
};
#endif

#endif
