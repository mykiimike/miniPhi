/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2026  Michael VERGOZ                                      *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify    *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation; either version 3 of the License, or       *
 * (at your option) any later version.                                   *
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

#ifdef SUPPORT_DRV_RTC_PCF8563

/* the PCF8563 shares its bus with the OLED, so borrow that display's pins */
#ifdef SUPPORT_DRV_LCD_SSD1306
	#include <ssd1306/ssd1306.h>
#endif

/**
@defgroup mpDrvPcf8563 NXP PCF8563

@ingroup mpDriver

@{
*/

#define PCF8563_REG_SECONDS 0x02
#define PCF8563_VL         0x80

static void _pcf8563_kick(mp_drv_pcf8563_t *rtc);
static MP_TASK(_pcf8563_task);

static unsigned long _pcf8563_ticks(unsigned long ms) {
	unsigned long hz = mp_clock_tick_hz();
	unsigned long ticks = hz ? (ms * hz) / 1000 : ms;

	return(ticks ? ticks : 1);
}

/* "0x51" or "51" */
static unsigned char _pcf8563_address(char *text) {
	unsigned int value = 0;

	if(!text || !*text)
		return(0x51);
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
			return(0x51);
	}
	if(value == 0 || value > 0x7F)
		return(0x51);
	return((unsigned char)value);
}

static int _pcf8563_from_bcd(unsigned char bcd) {
	if((bcd & 0x0F) > 9 || (bcd >> 4) > 9)
		return(-1);
	return((bcd >> 4) * 10 + (bcd & 0x0F));
}

/* borrow the OLED's I2C bus through the display face */
static int _pcf8563_bus(mp_drv_pcf8563_t *rtc) {
#ifdef SUPPORT_DRV_LCD_SSD1306
	mp_device_t *display = mp_device_find(MP_DEVICE_DISPLAY, 0);
	mp_pui_display_t *face = display ? mp_device_face(display) : NULL;
	mp_drv_ssd1306_t *lcd = face ? face->user : NULL;
	mp_i2c_bus_t *bus = lcd ? mp_drv_ssd1306_i2c(lcd) : NULL;

	if(!bus || !bus->sda || !bus->scl)
		return(FALSE);
	rtc->bus = bus;
	return(TRUE);
#else
	(void)rtc;
	return(FALSE);
#endif
}

static int _pcf8563_read(mp_drv_pcf8563_t *rtc, unsigned char *dst) {
	return(mp_i2c_read_reg(rtc->bus, rtc->address, PCF8563_REG_SECONDS, dst, 7));
}

static int _pcf8563_decode(const unsigned char regs[7], mp_device_time_t *time) {
	int second, minute, hour, day, month, year;

	second = _pcf8563_from_bcd((unsigned char)(regs[0] & 0x7F));
	minute = _pcf8563_from_bcd((unsigned char)(regs[1] & 0x7F));
	hour = _pcf8563_from_bcd((unsigned char)(regs[2] & 0x3F));
	day = _pcf8563_from_bcd((unsigned char)(regs[3] & 0x3F));
	month = _pcf8563_from_bcd((unsigned char)(regs[5] & 0x1F));
	year = _pcf8563_from_bcd(regs[6]);
	if(second < 0 || second > 59 || minute < 0 || minute > 59 || hour < 0 || hour > 23)
		return(0);
	if(day < 1 || day > 31 || month < 1 || month > 12 || year < 0 || year > 99)
		return(0);
	time->year = 2000 + year;
	time->month = (unsigned char)month;
	time->day = (unsigned char)day;
	time->wday = (unsigned char)(regs[4] & 0x07);
	time->hour = (unsigned char)hour;
	time->minute = (unsigned char)minute;
	time->second = (unsigned char)second;
	time->running = (regs[0] & PCF8563_VL) ? NO : YES;
	return(1);
}

static void _pcf8563_publish(mp_drv_pcf8563_t *rtc) {
	mp_device_time_t time;

	memset(&time, 0, sizeof(time));
	if(!_pcf8563_decode(rtc->regs, &time)) {
		MP_INTERRUPT_SAFE_BEGIN
		rtc->time.valid = NO;
		MP_INTERRUPT_SAFE_END
		return;
	}
	mp_device_clock_publish(&rtc->time, &time);
}

static void _pcf8563_kick(mp_drv_pcf8563_t *rtc) {
	if(rtc->task)
		mp_task_delay(rtc->task, 1);
}

static void _pcf8563_start_driver(void *self) {
	mp_drv_pcf8563_t *rtc = self;

	rtc->watched = YES;
	_pcf8563_kick(rtc);
}

static void _pcf8563_stop_driver(void *self) {
	mp_drv_pcf8563_t *rtc = self;

	rtc->watched = NO;
	if(rtc->task)
		mp_task_delay(rtc->task, MP_TASK_FOREVER);
}

static MP_TASK(_pcf8563_task) {
	mp_drv_pcf8563_t *rtc = task->user;

	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}
	if(rtc->watched == NO) {
		mp_task_delay(task, MP_TASK_FOREVER);
		return;
	}
	if(_pcf8563_read(rtc, rtc->regs)) {
		rtc->haveRegs = YES;
		_pcf8563_publish(rtc);
	}
	else {
		MP_INTERRUPT_SAFE_BEGIN
		rtc->time.valid = NO;
		MP_INTERRUPT_SAFE_END
	}
	if(rtc->watched == YES)
		mp_task_delay(task, _pcf8563_ticks(1000));
	else
		mp_task_delay(task, MP_TASK_FOREVER);
}

static void _pcf8563_bind(mp_drv_pcf8563_t *rtc) {
	static const mp_device_driver_ops_t ops = {
		"rtc/pcf8563", "NXP PCF8563", "1.0.0",
		(unsigned int)(1u << MP_DEVICE_CLOCK),
		NULL, NULL, NULL, _pcf8563_start_driver, _pcf8563_stop_driver
	};

	if(rtc->binding.kernel == rtc->kernel)
		return;
	if(mp_device_driver_register(rtc->kernel, &rtc->binding, &ops, rtc, NULL) != TRUE)
		mp_printk("PCF8563: can not register the driver");
}

static void _pcf8563_device(mp_drv_pcf8563_t *rtc) {
	if(rtc->binding.kernel != rtc->kernel)
		return;
	memset(&rtc->time, 0, sizeof(rtc->time));
	rtc->time.user = rtc;
	if(!mp_device_registry_register(&rtc->binding, &rtc->dev, MP_DEVICE_CLOCK, 0, &rtc->time, "pcf8563", "rtc", NULL)) {
		mp_printk("PCF8563: can not register the clock");
		return;
	}
	rtc->dev.label = "NXP PCF8563";
	_pcf8563_publish(rtc);
	rtc->task = mp_task_create(&rtc->kernel->tasks, "PCF8563", _pcf8563_task, rtc, MP_TASK_FOREVER);
	if(!rtc->task)
		mp_printk("PCF8563: no task");
}

mp_ret_t mp_drv_pcf8563_init(mp_kernel_t *kernel, mp_drv_pcf8563_t *rtc, mp_options_t *options, char *who) {
	memset(rtc, 0, sizeof(*rtc));
	rtc->kernel = kernel;
	rtc->address = _pcf8563_address(mp_options_get(options, "address"));

	if(!_pcf8563_bus(rtc)) {
		mp_printk("PCF8563: no display bus to borrow");
		return(FALSE);
	}
	_pcf8563_bind(rtc);
	if(_pcf8563_read(rtc, rtc->regs)) {
		rtc->haveRegs = YES;
		_pcf8563_device(rtc);
		mp_printk("PCF8563: clock on the bus at 0x%02x", rtc->address);
	}
	else
		mp_printk("PCF8563: no answer at 0x%02x", rtc->address);

	(void)who;
	return(TRUE);
}

void mp_drv_pcf8563_fini(mp_drv_pcf8563_t *rtc) {
	if(!rtc || !rtc->kernel)
		return;
	rtc->watched = NO;
	if(rtc->task) {
		mp_task_destroy(rtc->task);
		rtc->task = NULL;
	}
	if(rtc->dev.driver)
		mp_device_registry_unregister(&rtc->dev);
	if(rtc->binding.kernel)
		mp_device_driver_unregister(rtc->kernel, &rtc->binding);
	rtc->kernel = NULL;
}

#ifdef SUPPORT_COMMON_DEVICE
static mp_ret_t _pcf8563_device_open(mp_kernel_t *kernel, void *self, mp_options_t *options, char *who) {
	return(mp_drv_pcf8563_init(kernel, self, options, who));
}

static void _pcf8563_device_close(void *self) {
	mp_drv_pcf8563_fini(self);
}

const mp_device_ops_t mp_drv_pcf8563_device = {
	"PCF8563", MP_DEVICE_CLOCK, _pcf8563_device_open, _pcf8563_device_close, NULL, NULL,
	"NXP PCF8563", "1.0.0"
};
#endif

/**@}*/

#endif
