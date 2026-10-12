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

#ifdef SUPPORT_DRV_LCD_NOKIA3310

#define __LCD_FREQ  1000000

//LCD definitions +
#define __SEND_CMD   0
#define __SEND_CHR   1
#define __SEND_FLUSH 2
#define __LCD_X_RES                  84
#define __LCD_Y_RES                  48
#define __LCD_START_LINE_ADDR     (66-2)

static const unsigned char FontLookup[][5] = {
	{ 0x00, 0x00, 0x00, 0x00, 0x00 },  // sp
	{ 0x00, 0x00, 0x2f, 0x00, 0x00 },   // !
	{ 0x00, 0x07, 0x00, 0x07, 0x00 },   // "
	{ 0x14, 0x7f, 0x14, 0x7f, 0x14 },   // #
	{ 0x24, 0x2a, 0x7f, 0x2a, 0x12 },   // $
	{ 0xc4, 0xc8, 0x10, 0x26, 0x46 },   // %
	{ 0x36, 0x49, 0x55, 0x22, 0x50 },   // &
	{ 0x00, 0x05, 0x03, 0x00, 0x00 },   // '
	{ 0x00, 0x1c, 0x22, 0x41, 0x00 },   // (
	{ 0x00, 0x41, 0x22, 0x1c, 0x00 },   // )
	{ 0x14, 0x08, 0x3E, 0x08, 0x14 },   // *
	{ 0x08, 0x08, 0x3E, 0x08, 0x08 },   // +
	{ 0x00, 0x00, 0x50, 0x30, 0x00 },   // ,
	{ 0x10, 0x10, 0x10, 0x10, 0x10 },   // -
	{ 0x00, 0x60, 0x60, 0x00, 0x00 },   // .
	{ 0x20, 0x10, 0x08, 0x04, 0x02 },   // /
	{ 0x3E, 0x51, 0x49, 0x45, 0x3E },   // 0
	{ 0x00, 0x42, 0x7F, 0x40, 0x00 },   // 1
	{ 0x42, 0x61, 0x51, 0x49, 0x46 },   // 2
	{ 0x21, 0x41, 0x45, 0x4B, 0x31 },   // 3
	{ 0x18, 0x14, 0x12, 0x7F, 0x10 },   // 4
	{ 0x27, 0x45, 0x45, 0x45, 0x39 },   // 5
	{ 0x3C, 0x4A, 0x49, 0x49, 0x30 },   // 6
	{ 0x01, 0x71, 0x09, 0x05, 0x03 },   // 7
	{ 0x36, 0x49, 0x49, 0x49, 0x36 },   // 8
	{ 0x06, 0x49, 0x49, 0x29, 0x1E },   // 9
	{ 0x00, 0x36, 0x36, 0x00, 0x00 },   // :
	{ 0x00, 0x56, 0x36, 0x00, 0x00 },   // ;
	{ 0x08, 0x14, 0x22, 0x41, 0x00 },   // <
	{ 0x14, 0x14, 0x14, 0x14, 0x14 },   // =
	{ 0x00, 0x41, 0x22, 0x14, 0x08 },   // >
	{ 0x02, 0x01, 0x51, 0x09, 0x06 },   // ?
	{ 0x32, 0x49, 0x59, 0x51, 0x3E },   // @
	{ 0x7E, 0x11, 0x11, 0x11, 0x7E },   // A
	{ 0x7F, 0x49, 0x49, 0x49, 0x36 },   // B
	{ 0x3E, 0x41, 0x41, 0x41, 0x22 },   // C
	{ 0x7F, 0x41, 0x41, 0x22, 0x1C },   // D
	{ 0x7F, 0x49, 0x49, 0x49, 0x41 },   // E
	{ 0x7F, 0x09, 0x09, 0x09, 0x01 },   // F
	{ 0x3E, 0x41, 0x49, 0x49, 0x7A },   // G
	{ 0x7F, 0x08, 0x08, 0x08, 0x7F },   // H
	{ 0x00, 0x41, 0x7F, 0x41, 0x00 },   // I
	{ 0x20, 0x40, 0x41, 0x3F, 0x01 },   // J
	{ 0x7F, 0x08, 0x14, 0x22, 0x41 },   // K
	{ 0x7F, 0x40, 0x40, 0x40, 0x40 },   // L
	{ 0x7F, 0x02, 0x0C, 0x02, 0x7F },   // M
	{ 0x7F, 0x04, 0x08, 0x10, 0x7F },   // N
	{ 0x3E, 0x41, 0x41, 0x41, 0x3E },   // O
	{ 0x7F, 0x09, 0x09, 0x09, 0x06 },   // P
	{ 0x3E, 0x41, 0x51, 0x21, 0x5E },   // Q
	{ 0x7F, 0x09, 0x19, 0x29, 0x46 },   // R
	{ 0x46, 0x49, 0x49, 0x49, 0x31 },   // S
	{ 0x01, 0x01, 0x7F, 0x01, 0x01 },   // T
	{ 0x3F, 0x40, 0x40, 0x40, 0x3F },   // U
	{ 0x1F, 0x20, 0x40, 0x20, 0x1F },   // V
	{ 0x3F, 0x40, 0x38, 0x40, 0x3F },   // W
	{ 0x63, 0x14, 0x08, 0x14, 0x63 },   // X
	{ 0x07, 0x08, 0x70, 0x08, 0x07 },   // Y
	{ 0x61, 0x51, 0x49, 0x45, 0x43 },   // Z
	{ 0x00, 0x7F, 0x41, 0x41, 0x00 },   // [
	{ 0x55, 0x2A, 0x55, 0x2A, 0x55 },   // 55
	{ 0x00, 0x41, 0x41, 0x7F, 0x00 },   // ]
	{ 0x04, 0x02, 0x01, 0x02, 0x04 },   // ^
	{ 0x40, 0x40, 0x40, 0x40, 0x40 },   // _
	{ 0x00, 0x01, 0x02, 0x04, 0x00 },   // '
	{ 0x20, 0x54, 0x54, 0x54, 0x78 },   // a
	{ 0x7F, 0x48, 0x44, 0x44, 0x38 },   // b
	{ 0x38, 0x44, 0x44, 0x44, 0x20 },   // c
	{ 0x38, 0x44, 0x44, 0x48, 0x7F },   // d
	{ 0x38, 0x54, 0x54, 0x54, 0x18 },   // e
	{ 0x08, 0x7E, 0x09, 0x01, 0x02 },   // f
	{ 0x0C, 0x52, 0x52, 0x52, 0x3E },   // g
	{ 0x7F, 0x08, 0x04, 0x04, 0x78 },   // h
	{ 0x00, 0x44, 0x7D, 0x40, 0x00 },   // i
	{ 0x20, 0x40, 0x44, 0x3D, 0x00 },   // j
	{ 0x7F, 0x10, 0x28, 0x44, 0x00 },   // k
	{ 0x00, 0x41, 0x7F, 0x40, 0x00 },   // l
	{ 0x7C, 0x04, 0x18, 0x04, 0x78 },   // m
	{ 0x7C, 0x08, 0x04, 0x04, 0x78 },   // n
	{ 0x38, 0x44, 0x44, 0x44, 0x38 },   // o
	{ 0x7C, 0x14, 0x14, 0x14, 0x08 },   // p
	{ 0x08, 0x14, 0x14, 0x18, 0x7C },   // q
	{ 0x7C, 0x08, 0x04, 0x04, 0x08 },   // r
	{ 0x48, 0x54, 0x54, 0x54, 0x20 },   // s
	{ 0x04, 0x3F, 0x44, 0x40, 0x20 },   // t
	{ 0x3C, 0x40, 0x40, 0x20, 0x7C },   // u
	{ 0x1C, 0x20, 0x40, 0x20, 0x1C },   // v
	{ 0x3C, 0x40, 0x30, 0x40, 0x3C },   // w
	{ 0x44, 0x28, 0x10, 0x28, 0x44 },   // x
	{ 0x0C, 0x50, 0x50, 0x50, 0x3C },   // y
	{ 0x44, 0x64, 0x54, 0x4C, 0x44 },   // z
	{ 0x08, 0x6C, 0x6A, 0x19, 0x08 },   // {
	{ 0x30, 0x4E, 0x61, 0x4E, 0x30 }    // |
};

static void _set_position(mp_drv_nokia3310_t *drv, unsigned char x, unsigned int y);
static void _lcd_send(mp_drv_nokia3310_t *drv, unsigned char data, unsigned char cd);

/*
 * gate=<gate name>
 * somi=<port>
 * simo=<port>
 * ste=<port>
 * clk=<port>
 * dc=<port>
 * contrast=<port>
 */
mp_ret_t mp_drv_nokia3310_init(mp_kernel_t *kernel, mp_drv_nokia3310_t *drv, mp_options_t *options, char *who) {
	mp_ret_t ret;
	char *value;

	memset(drv, 0, sizeof(*drv));

	/* dc */
	value = mp_options_get(options, "dc");
	if(!value) {
		mp_drv_nokia3310_fini(drv);
		return(FALSE);
	}
	drv->dc = mp_gpio_text_handle(value, "LCD DC");
    if(!drv->dc) {
		mp_drv_nokia3310_fini(drv);
		return(FALSE);
	}

	/* contrast */
	value = mp_options_get(options, "contrast");
	if(!value) {
		mp_drv_nokia3310_fini(drv);
		return(FALSE);
	}
	drv->res = mp_gpio_text_handle(value, "LCD CONSTRAT");
    if(!drv->res) {
		mp_drv_nokia3310_fini(drv);
		return(FALSE);
	}

    /* initialize SPI interface */
	ret = mp_spi_open(
		kernel, &drv->spi, options,
		MP_SPI_CLK_PHASE_CHANGE,
		MP_SPI_CLK_POLARITY_HIGH,
		MP_SPI_MSB,
		MP_SPI_MASTER,
		MP_SPI_MODE_3PIN,
		MP_SPI_BIT_8,
		MP_SPI_SYNC,
		__LCD_FREQ,
		"Nokia 3310 LCD"
	);
    if(ret == FALSE) {
		mp_drv_nokia3310_fini(drv);
		return(FALSE);
	}

    /* set gpio direction */
    mp_gpio_direction(drv->res, MP_GPIO_OUTPUT);
    mp_gpio_unset(drv->res);
    mp_clock_delay(1000);
	mp_gpio_set(drv->res);
	mp_clock_delay(1000);

	_lcd_send(drv, 0x21, __SEND_CMD);  // LCD Extended Commands.
	//Extended commands +
		_lcd_send(drv, 0xC8, __SEND_CMD);  // Set Contrast.
		_lcd_send(drv, 0x04 | !!(__LCD_START_LINE_ADDR&(0x40)), __SEND_CMD);      // Set Temp S6 for start line
		_lcd_send(drv, 0x40 | (__LCD_START_LINE_ADDR & ((0x40)-1)), __SEND_CMD);  // Set Temp S[5:0] for start line
		_lcd_send(drv, 0x12, __SEND_CMD);  // LCD bias mode 1:68.
	//Extended commands -
		_lcd_send(drv, 0x20, __SEND_CMD);  // LCD Standard Commands, Horizontal addressing mode.
		_lcd_send(drv, 0x08, __SEND_CMD);  // LCD blank
		_lcd_send(drv, 0x0C, __SEND_CMD);  // LCD in normal mode.

	//Optional commands +
	mp_drv_nokia3310_contrast(drv, 0xf0);       //Increase contrast
	mp_drv_nokia3310_clear(drv);              //Clear Screen
	_set_position(drv, 0, 0);   //0, 0 coordinates of the display


	return(TRUE);
}

mp_ret_t mp_drv_nokia3310_fini(mp_drv_nokia3310_t *drv) {

	if(drv->res)
		mp_gpio_release(drv->res);
	if(drv->dc)
		mp_gpio_release(drv->dc);

	mp_spi_close(&drv->spi);
	return(TRUE);
}


void mp_drv_nokia3310_update(mp_drv_nokia3310_t *drv) {
	char x, y;
	for(y=0; y<__LCD_Y_RES/8; y++) {
		_lcd_send(drv, 0x80, __SEND_CMD);     //x - home
		_lcd_send(drv, 0x40|y, __SEND_CMD);   //y - next line

		for(x=0; x<__LCD_X_RES; x++)
			_lcd_send(drv, drv->LCDMemory[y*__LCD_X_RES+x], __SEND_CHR);
	}
}

void mp_drv_nokia3310_clear(mp_drv_nokia3310_t *drv) {
	mp_drv_nokia3310_clearMemory(drv);
	mp_drv_nokia3310_update(drv);
}

void mp_drv_nokia3310_contrast(mp_drv_nokia3310_t *drv, unsigned char contrast) {
	_lcd_send(drv, 0x21, __SEND_CMD);
	//LCD Extended Commands +
		_lcd_send(drv, 0x80 | contrast, __SEND_CMD);  // Set LCD Vop (Contrast).
	//LCD Extended Commands -
	_lcd_send(drv, 0x20, __SEND_CMD);         //LCD Standard Commands, horizontal addressing mode
}

void mp_drv_nokia3310_write(mp_drv_nokia3310_t *drv, unsigned char *a, int l) {
	unsigned char i;
	while (*a && l) {
		   l--;
		   for (i=0; i<5; i++)
			   	   drv->LCDMemory[drv->index++] = FontLookup[*a-32][i];
		drv->LCDMemory[drv->index++] = 0x00;    //space between signs
		a++;
	}
}

void mp_drv_nokia3310_writePos(mp_drv_nokia3310_t *drv, unsigned char *str, int l, int pos) {
	int a, b;
	for(a=0; a<l; a++) {
		for (b=0; b<5; b++)
			drv->LCDMemory[pos+b] = FontLookup[*str-32][b];
		drv->LCDMemory[pos+b] = 0x00;
		pos += 6;
		str++;
	}
}

unsigned char *mp_drv_nokia3310_getMemory(mp_drv_nokia3310_t *drv) {
	return(drv->LCDMemory);
}

void mp_drv_nokia3310_clearMemory(mp_drv_nokia3310_t *drv) {
	for (drv->index=0; drv->index<504; drv->index++)
		drv->LCDMemory[drv->index] = 0;
	drv->index = 0;
}

static void _lcd_send(mp_drv_nokia3310_t *drv, unsigned char data, unsigned char cd) {
	if(cd == __SEND_CHR)
		mp_gpio_set(drv->dc); /* sending char */
	else if(cd == __SEND_CMD)
		mp_gpio_unset(drv->dc); /* sending command */
	mp_spi_tx(&drv->spi, data);
}

static void _set_position(mp_drv_nokia3310_t *drv, unsigned char x, unsigned int y) {
	y = y + x/84;
	y = y % 6;
	x = x % 84;
	drv->index = 84*y + x;
	_lcd_send(drv, 0x80 | x, __SEND_CMD);
	_lcd_send(drv, 0x40 | y, __SEND_CMD);
}

#endif
