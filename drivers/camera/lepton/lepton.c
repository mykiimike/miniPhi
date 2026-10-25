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

#define LEPTON_STEP_IDLE 0
#define LEPTON_STEP_STATUS 1
#define LEPTON_STEP_LENGTH 2
#define LEPTON_STEP_COMMAND 3
#define LEPTON_STEP_RESULT 4
#define LEPTON_STEP_DATA 5

#define LEPTON_OP_BOOT 0
#define LEPTON_OP_GET 1

#define LEPTON_IDENT_SERIAL 0
#define LEPTON_IDENT_PART 1
#define LEPTON_IDENT_VERSION 2
#define LEPTON_IDENT_DONE 3

static void _lepton_onCci(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _lepton_next(mp_drv_lepton_t *lep);
static mp_bool_t _lepton_read(mp_drv_lepton_t *lep, unsigned short reg, int n);
static mp_bool_t _lepton_write(mp_drv_lepton_t *lep, unsigned short reg, unsigned short value);
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

static unsigned short _lepton_word(unsigned char *raw) {
	return((unsigned short)((raw[0] << 8) | raw[1]));
}

static mp_bool_t _lepton_read(mp_drv_lepton_t *lep, unsigned short reg, int n) {
	if(n < 1)
		n = 1;
	if(n > (int)sizeof(lep->cciRx))
		n = (int)sizeof(lep->cciRx);
	lep->cciReg[0] = (unsigned char)(reg >> 8);
	lep->cciReg[1] = (unsigned char)reg;
	lep->busy = YES;
	if(mp_regMaster_read(
		&lep->regMaster, lep->cciReg, 2, lep->cciRx, n, _lepton_onCci, lep
	) == FALSE) {
		lep->busy = NO;
		lep->step = LEPTON_STEP_IDLE;
		return(FALSE);
	}
	return(TRUE);
}

static mp_bool_t _lepton_write(mp_drv_lepton_t *lep, unsigned short reg, unsigned short value) {
	lep->cciTx[0] = (unsigned char)(reg >> 8);
	lep->cciTx[1] = (unsigned char)reg;
	lep->cciTx[2] = (unsigned char)(value >> 8);
	lep->cciTx[3] = (unsigned char)value;
	lep->busy = YES;
	if(mp_regMaster_write(&lep->regMaster, lep->cciTx, 4, _lepton_onCci, lep) == FALSE) {
		lep->busy = NO;
		lep->step = LEPTON_STEP_IDLE;
		return(FALSE);
	}
	return(TRUE);
}

static void _lepton_begin(mp_drv_lepton_t *lep, unsigned char op) {
	lep->op = op;
	lep->step = LEPTON_STEP_STATUS;
	lep->poll = 0;
	if(_lepton_read(lep, LEPTON_REG_STATUS, 2) == FALSE)
		lep->step = LEPTON_STEP_IDLE;
}

static void _lepton_get(mp_drv_lepton_t *lep, unsigned short cmd, int words) {
	lep->cciCmd = cmd;
	lep->cciWords = (unsigned char)words;
	_lepton_begin(lep, LEPTON_OP_GET);
}

/* an identity command that failed is skipped; temperature is tried again later */
static void _lepton_done(mp_drv_lepton_t *lep) {
	lep->step = LEPTON_STEP_IDLE;
	if(lep->cciCmd != LEPTON_CMD_FPA && lep->ident < LEPTON_IDENT_DONE)
		lep->ident++;
	if(lep->ident < LEPTON_IDENT_DONE)
		_lepton_next(lep);
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

static int _lepton_dash(const char *text) {
	while(*text) {
		if(*text == '-')
			return(1);
		text++;
	}
	return(0);
}

static void _lepton_take_serial(mp_drv_lepton_t *lep) {
	int i;

	for(i = 0; i < 8; i++)
		snprintf(lep->serial + i * 2, 3, "%02x", lep->cciRx[i]);
	lep->serial[16] = 0;
	mp_printk("Lepton serial %s", lep->serial);
	mp_device_identify(lep, lep->model, lep->serial, lep->version);
}

static void _lepton_take_part(mp_drv_lepton_t *lep) {
	char swapped[33];
	char plain[33];
	const char *use;

	_lepton_text(swapped, (int)sizeof(swapped), lep->cciRx, 32, 1);
	_lepton_text(plain, (int)sizeof(plain), lep->cciRx, 32, 0);
	if(_lepton_dash(swapped))
		use = swapped;
	else if(_lepton_dash(plain))
		use = plain;
	else if(swapped[0])
		use = swapped;
	else
		use = plain;
	snprintf(lep->model, sizeof(lep->model), "%s", use);
	if(lep->model[0])
		mp_printk("Lepton model %s", lep->model);
	else
		mp_printk("Lepton model empty");
	mp_device_identify(lep, lep->model, lep->serial, lep->version);
}

static void _lepton_take_version(mp_drv_lepton_t *lep) {
	const unsigned char *raw = lep->cciRx;

	/*
	 * Each word arrives most significant byte first. The SDK stores the
	 * swapped word, so gpp major/minor/build are raw[1], raw[0], raw[3]
	 * and the dsp triple follows one word later.
	 */
	snprintf(lep->version, sizeof(lep->version), "gpp %u.%u.%u  dsp %u.%u.%u",
		raw[1], raw[0], raw[3], raw[2], raw[5], raw[4]);
	mp_printk("Lepton version %s", lep->version);
	mp_device_identify(lep, lep->model, lep->serial, lep->version);
}

static void _lepton_onCci(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_lepton_t *lep = operand->user;
	unsigned short status;
	unsigned short kelvin;

	lep->busy = NO;
	if(terminate == YES) {
		lep->step = LEPTON_STEP_IDLE;
		return;
	}

	switch(lep->step) {
		case LEPTON_STEP_STATUS:
			status = _lepton_word(lep->cciRx);
			if(status & LEPTON_STATUS_BOOT)
				lep->booted = YES;
			if(status & LEPTON_STATUS_BUSY) {
				if(++lep->poll < 8 && _lepton_read(lep, LEPTON_REG_STATUS, 2) == TRUE)
					return;
				lep->step = LEPTON_STEP_IDLE;
				return;
			}
			if(lep->op != LEPTON_OP_GET) {
				lep->step = LEPTON_STEP_IDLE;
				return;
			}
			lep->step = LEPTON_STEP_LENGTH;
			if(_lepton_write(lep, LEPTON_REG_LENGTH, lep->cciWords) == FALSE)
				lep->step = LEPTON_STEP_IDLE;
			return;

		case LEPTON_STEP_LENGTH:
			lep->step = LEPTON_STEP_COMMAND;
			if(_lepton_write(lep, LEPTON_REG_COMMAND, lep->cciCmd) == FALSE)
				lep->step = LEPTON_STEP_IDLE;
			return;

		case LEPTON_STEP_COMMAND:
			lep->poll = 0;
			lep->step = LEPTON_STEP_RESULT;
			if(_lepton_read(lep, LEPTON_REG_STATUS, 2) == FALSE)
				lep->step = LEPTON_STEP_IDLE;
			return;

		case LEPTON_STEP_RESULT:
			status = _lepton_word(lep->cciRx);
			if(status & LEPTON_STATUS_BOOT)
				lep->booted = YES;
			if(status & LEPTON_STATUS_BUSY) {
				if(++lep->poll < 40 && _lepton_read(lep, LEPTON_REG_STATUS, 2) == TRUE)
					return;
				lep->step = LEPTON_STEP_IDLE;
				return;
			}
			if(status & 0xFF00) {
				mp_printk("Lepton: command 0x%04x status 0x%04x",
					(unsigned int)lep->cciCmd, (unsigned int)status);
				_lepton_done(lep);
				return;
			}
			lep->step = LEPTON_STEP_DATA;
			if(_lepton_read(lep, LEPTON_REG_DATA, (int)lep->cciWords * 2) == FALSE)
				lep->step = LEPTON_STEP_IDLE;
			return;

		case LEPTON_STEP_DATA:
			if(lep->cciCmd == LEPTON_CMD_SERIAL)
				_lepton_take_serial(lep);
			else if(lep->cciCmd == LEPTON_CMD_PART)
				_lepton_take_part(lep);
			else if(lep->cciCmd == LEPTON_CMD_VERSION)
				_lepton_take_version(lep);
			else if(lep->cciCmd == LEPTON_CMD_FPA) {
				kelvin = _lepton_word(lep->cciRx);
				/* the word is kelvin times 100; a cold module is still above 200 K */
				if(lep->tempDev.driver && kelvin >= 20000 && kelvin <= 40000)
					mp_device_measure_publish(&lep->temperature, (float)kelvin / 100.0f - 273.15f);
			}
			_lepton_done(lep);
			return;

		default:
			lep->step = LEPTON_STEP_IDLE;
			return;
	}
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

static void _lepton_next(mp_drv_lepton_t *lep) {
	if(lep->ident == LEPTON_IDENT_SERIAL)
		_lepton_get(lep, LEPTON_CMD_SERIAL, 4);
	else if(lep->ident == LEPTON_IDENT_PART)
		_lepton_get(lep, LEPTON_CMD_PART, 16);
	else if(lep->ident == LEPTON_IDENT_VERSION)
		_lepton_get(lep, LEPTON_CMD_VERSION, 4);
}

static void _lepton_temp(mp_drv_lepton_t *lep) {
	unsigned long now = mp_clock_ticks();
	unsigned long period = _lepton_ticks(1000);

	if(lep->tempAt && (long)(now - lep->tempAt) < (long)period)
		return;
	lep->tempAt = now;
	_lepton_get(lep, LEPTON_CMD_FPA, 1);
}

static void _lepton_query(mp_drv_lepton_t *lep) {
	if(lep->step != LEPTON_STEP_IDLE || lep->busy == YES)
		return;
	if(lep->ident < LEPTON_IDENT_DONE) {
		_lepton_next(lep);
		return;
	}
	_lepton_temp(lep);
}

static MP_TASK(_lepton_task) {
	mp_drv_lepton_t *lep = task->user;

	if(task->signal == MP_TASK_SIG_STOP) {
		_lepton_dma_stop(lep);
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	/*
	 * A status read that never calls back used to sit here forever, so the
	 * picture and the identity never started. Drop that transfer and run
	 * the video anyway; the next pass asks the chip again.
	 */
	if(lep->step != LEPTON_STEP_IDLE || lep->busy == YES) {
		if(++lep->tries >= 20) {
			lep->tries = 0;
			if(lep->gaveUp == NO) {
				mp_printk("Lepton: I2C stalled");
				lep->gaveUp = YES;
			}
			mp_i2c_reset(&lep->i2c);
			mp_regMaster_cancel(&lep->regMaster);
		}
	}
	else {
		lep->tries = 0;
		if(lep->booted == NO)
			_lepton_begin(lep, LEPTON_OP_BOOT);
		else
			_lepton_query(lep);
	}

	if(lep->phase == LEPTON_PHASE_BOOT) {
		lep->phase = LEPTON_PHASE_SYNC;
		{
			static const mp_device_driver_ops_t ops = {
				"camera/lepton", "FLIR Lepton", "1.0.0",
				(unsigned int)(1u << MP_DEVICE_TEMPERATURE),
				NULL, NULL, NULL, NULL, NULL
			};

			if(mp_device_driver_register(lep->kernel, &lep->binding, &ops, lep, NULL) == TRUE)
				mp_device_registry_register(&lep->binding, &lep->tempDev, MP_DEVICE_TEMPERATURE, 0, &lep->temperature, "lepton", "fpa", NULL);
			if(!lep->tempDev.driver)
				mp_printk("Lepton: can not register the temperature");
		}
		mp_printk("Lepton ready");
	}

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
	mp_options_t i2cOpen[] = {
		{ "gate", mp_options_get(options, "i2cGate") },
		{ "sda", mp_options_get(options, "sda") },
		{ "clk", mp_options_get(options, "scl") },
		{ NULL, NULL }
	};
	mp_options_t i2cSetup[] = {
		{ "frequency", "100000" },
		{ "role", "master" },
		{ NULL, NULL }
	};

	if(mp_spi_open(kernel, &lep->spi, spiOpen, who) == FALSE)
		return(FALSE);
	if(mp_spi_setup(&lep->spi, spiSetup) == FALSE)
		return(FALSE);

	if(mp_i2c_open(kernel, &lep->i2c, i2cOpen, who) == FALSE)
		return(FALSE);
	if(mp_i2c_setup(&lep->i2c, i2cSetup) == FALSE)
		return(FALSE);
	mp_i2c_setSlaveAddress(&lep->i2c, lep->address);

	if(mp_regMaster_init_i2c(kernel, &lep->regMaster, &lep->i2c, lep, "Lepton I2C") == FALSE) {
		mp_printk("Lepton: can not start regMaster");
		return(FALSE);
	}
	return(TRUE);
}

mp_ret_t mp_drv_lepton_init(mp_kernel_t *kernel, mp_drv_lepton_t *lep, mp_options_t *options, char *who) {
	memset(lep, 0, sizeof(*lep));
	lep->kernel = kernel;
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
	_lepton_dma_stop(lep);
	if(lep->task) {
		mp_task_destroy(lep->task);
		lep->task = NULL;
	}

	lep->busy = NO;
	mp_regMaster_fini(&lep->regMaster);
	mp_i2c_close(&lep->i2c);
	mp_spi_close(&lep->spi);

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
	mp_drv_lepton_fini(self);
}

static void *_lepton_device_face(void *self) {
	mp_drv_lepton_t *lep = self;

	return(&lep->frame);
}

const mp_device_ops_t mp_drv_lepton_device = {
	"LEPTON", MP_DEVICE_CAMERA, _lepton_device_open, _lepton_device_close, _lepton_device_face, NULL
};
#endif

#endif
