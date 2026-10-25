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

#ifdef SUPPORT_DRV_DS1307

/**
@defgroup mpDrvDs1307 Maxim DS1307

@ingroup mpDriver

@{
*/

/* half a 100 kHz clock. The DS1307 does not answer any faster. */
#define DS1307_I2C_HALF 5000UL

#define DS1307_REG_SECONDS 0x00
#define DS1307_REG_CONTROL 0x07
#define DS1307_CH          0x80
#define DS1307_HOUR12      0x40
#define DS1307_PM          0x20
#define DS1307_SQWE        0x10

static const char *const _ds1307_sqw[] = { "off", "1hz", "4khz", "8khz", "32khz", NULL };

static void _ds1307_kick(mp_drv_ds1307_t *rtc);
static mp_ret_t _ds1307_time_set(mp_device_time_t *face, const mp_device_time_t *time);
static mp_ret_t _ds1307_running_set(mp_device_registry_t *device, const mp_device_value_t *value);
static mp_ret_t _ds1307_running_get(mp_device_registry_t *device, mp_device_value_t *value);
static mp_ret_t _ds1307_sqw_set(mp_device_registry_t *device, const mp_device_value_t *value);
static mp_ret_t _ds1307_sqw_get(mp_device_registry_t *device, mp_device_value_t *value);
static MP_TASK(_ds1307_task);

static const mp_device_command_t _ds1307_commands[] = {
	{ "running", "Running", MP_DEVICE_ARG_BOOL, 0, 0, NULL, NULL, _ds1307_running_set, _ds1307_running_get },
	{ "sqw", "Square wave", MP_DEVICE_ARG_SELECT, 0, 0, _ds1307_sqw, NULL, _ds1307_sqw_set, _ds1307_sqw_get },
	{ NULL, NULL, 0, 0, 0, NULL, NULL, NULL, NULL }
};

static unsigned long _ds1307_ticks(unsigned long ms) {
	unsigned long hz = mp_clock_tick_hz();
	unsigned long ticks = hz ? (ms * hz) / 1000 : ms;

	return(ticks ? ticks : 1);
}

/* "0x68" or "68" */
static unsigned char _ds1307_address(char *text) {
	unsigned int value = 0;

	if(!text || !*text)
		return(0x68);
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
			return(0x68);
	}
	if(value == 0 || value > 0x7F)
		return(0x68);
	return((unsigned char)value);
}

static unsigned char _ds1307_to_bcd(unsigned char value) {
	return((unsigned char)(((value / 10) << 4) | (value % 10)));
}

/* -1 when a nibble is not a decimal digit */
static int _ds1307_from_bcd(unsigned char bcd) {
	if((bcd & 0x0F) > 9 || (bcd >> 4) > 9)
		return(-1);
	return((bcd >> 4) * 10 + (bcd & 0x0F));
}

static int _ds1307_hour(unsigned char raw) {
	int hour;

	if(raw & DS1307_HOUR12) {
		hour = _ds1307_from_bcd((unsigned char)(raw & 0x1F));
		if(hour < 1 || hour > 12)
			return(-1);
		if(hour == 12)
			return((raw & DS1307_PM) ? 12 : 0);
		return((raw & DS1307_PM) ? hour + 12 : hour);
	}
	hour = _ds1307_from_bcd((unsigned char)(raw & 0x3F));
	if(hour < 0 || hour > 23)
		return(-1);
	return(hour);
}

static int _ds1307_decode(const unsigned char regs[8], mp_device_time_t *time) {
	int second, minute, hour, date, month, year;

	second = _ds1307_from_bcd((unsigned char)(regs[0] & 0x7F));
	minute = _ds1307_from_bcd((unsigned char)(regs[1] & 0x7F));
	hour = _ds1307_hour(regs[2]);
	date = _ds1307_from_bcd((unsigned char)(regs[4] & 0x3F));
	month = _ds1307_from_bcd((unsigned char)(regs[5] & 0x1F));
	year = _ds1307_from_bcd(regs[6]);
	if(second < 0 || second > 59 || minute < 0 || minute > 59 || hour < 0)
		return(0);
	if(date < 1 || date > 31 || month < 1 || month > 12 || year < 0)
		return(0);
	time->year = 2000 + year;
	time->month = (unsigned char)month;
	time->day = (unsigned char)date;
	time->wday = (unsigned char)(regs[3] & 0x07);
	time->hour = (unsigned char)hour;
	time->minute = (unsigned char)minute;
	time->second = (unsigned char)second;
	time->running = (regs[0] & DS1307_CH) ? NO : YES;
	return(1);
}

/* a released line is an input with its pull-up; a low line is open-drain */
static void _ds1307_line(mp_gpio_port_t *pin, int high) {
	if(high) {
		mp_gpio_set(pin);
		mp_gpio_mode(pin, MP_GPIO_MODE_INPUT_PULL);
	}
	else {
		mp_gpio_mode(pin, MP_GPIO_MODE_OUTPUT_OD);
		mp_gpio_unset(pin);
	}
	mp_clock_nanoDelay(DS1307_I2C_HALF);
}

static int _ds1307_scl_high(mp_drv_ds1307_t *rtc) {
	int spin;

	_ds1307_line(rtc->scl, 1);
	for(spin = 0; spin < 1000 && !mp_gpio_raw_read(rtc->scl); spin++)
		mp_clock_nanoDelay(DS1307_I2C_HALF);
	return(mp_gpio_raw_read(rtc->scl) ? 1 : 0);
}

static void _ds1307_start(mp_drv_ds1307_t *rtc) {
	_ds1307_line(rtc->sda, 1);
	_ds1307_scl_high(rtc);
	_ds1307_line(rtc->sda, 0);
	_ds1307_line(rtc->scl, 0);
}

static void _ds1307_stop(mp_drv_ds1307_t *rtc) {
	_ds1307_line(rtc->sda, 0);
	_ds1307_scl_high(rtc);
	_ds1307_line(rtc->sda, 1);
}

/* 1 when the chip acknowledged */
static int _ds1307_put(mp_drv_ds1307_t *rtc, unsigned char byte) {
	int bit;
	int ack;

	for(bit = 7; bit >= 0; bit--) {
		_ds1307_line(rtc->sda, (byte >> bit) & 1);
		if(!_ds1307_scl_high(rtc))
			return(0);
		_ds1307_line(rtc->scl, 0);
	}
	_ds1307_line(rtc->sda, 1);
	if(!_ds1307_scl_high(rtc))
		return(0);
	ack = mp_gpio_raw_read(rtc->sda) ? 0 : 1;
	_ds1307_line(rtc->scl, 0);
	return(ack);
}

static int _ds1307_get(mp_drv_ds1307_t *rtc, unsigned char *byte, int more) {
	int bit;

	*byte = 0;
	_ds1307_line(rtc->sda, 1);
	for(bit = 0; bit < 8; bit++) {
		if(!_ds1307_scl_high(rtc))
			return(0);
		*byte = (unsigned char)((*byte << 1) | (mp_gpio_raw_read(rtc->sda) ? 1 : 0));
		_ds1307_line(rtc->scl, 0);
	}
	_ds1307_line(rtc->sda, more ? 0 : 1);
	if(!_ds1307_scl_high(rtc))
		return(0);
	_ds1307_line(rtc->scl, 0);
	_ds1307_line(rtc->sda, 1);
	return(1);
}

static int _ds1307_read(mp_drv_ds1307_t *rtc, unsigned char *dst) {
	int n;

	_ds1307_start(rtc);
	if(!_ds1307_put(rtc, (unsigned char)(rtc->address << 1)) || !_ds1307_put(rtc, DS1307_REG_SECONDS)) {
		_ds1307_stop(rtc);
		return(0);
	}
	_ds1307_start(rtc);
	if(!_ds1307_put(rtc, (unsigned char)((rtc->address << 1) | 1))) {
		_ds1307_stop(rtc);
		return(0);
	}
	for(n = 0; n < 8; n++) {
		if(!_ds1307_get(rtc, &dst[n], n < 7)) {
			_ds1307_stop(rtc);
			return(0);
		}
	}
	_ds1307_stop(rtc);
	return(1);
}

static int _ds1307_write_at(mp_drv_ds1307_t *rtc, unsigned char at, const unsigned char *bytes, unsigned char len) {
	unsigned char n;

	_ds1307_start(rtc);
	if(!_ds1307_put(rtc, (unsigned char)(rtc->address << 1)) || !_ds1307_put(rtc, at)) {
		_ds1307_stop(rtc);
		return(0);
	}
	for(n = 0; n < len; n++) {
		if(!_ds1307_put(rtc, bytes[n])) {
			_ds1307_stop(rtc);
			return(0);
		}
	}
	_ds1307_stop(rtc);
	return(1);
}

static void _ds1307_queue(mp_drv_ds1307_t *rtc, unsigned char at, const unsigned char *bytes, unsigned char len) {
	unsigned char n;

	if(len > sizeof(rtc->wr))
		return;
	MP_INTERRUPT_SAFE_BEGIN
	rtc->wrAt = at;
	rtc->wrLen = len;
	for(n = 0; n < len; n++)
		rtc->wr[n] = bytes[n];
	rtc->pending = YES;
	MP_INTERRUPT_SAFE_END
	_ds1307_kick(rtc);
}

static void _ds1307_flush(mp_drv_ds1307_t *rtc) {
	unsigned char at;
	unsigned char len;
	unsigned char buf[8];
	unsigned char n;
	mp_bool_t go = NO;

	MP_INTERRUPT_SAFE_BEGIN
	if(rtc->pending == YES) {
		go = YES;
		at = rtc->wrAt;
		len = rtc->wrLen;
		for(n = 0; n < len && n < sizeof(buf); n++)
			buf[n] = rtc->wr[n];
		rtc->pending = NO;
	}
	MP_INTERRUPT_SAFE_END
	if(go == NO)
		return;
	if(!_ds1307_write_at(rtc, at, buf, len)) {
		mp_printk("DS1307: write refused");
		return;
	}
	if(at < sizeof(rtc->regs)) {
		for(n = 0; n < len && (unsigned int)at + n < sizeof(rtc->regs); n++)
			rtc->regs[at + n] = buf[n];
		rtc->haveRegs = YES;
	}
}

static void _ds1307_publish(mp_drv_ds1307_t *rtc) {
	mp_device_time_t time;

	memset(&time, 0, sizeof(time));
	if(!_ds1307_decode(rtc->regs, &time)) {
		MP_INTERRUPT_SAFE_BEGIN
		rtc->time.valid = NO;
		MP_INTERRUPT_SAFE_END
		return;
	}
	mp_device_clock_publish(&rtc->time, &time);
}

static void _ds1307_kick(mp_drv_ds1307_t *rtc) {
	if(rtc->task)
		mp_task_delay(rtc->task, 1);
}

static int _ds1307_span(int value, int lo, int hi) {
	return(value >= lo && value <= hi);
}

static mp_ret_t _ds1307_time_set(mp_device_time_t *face, const mp_device_time_t *time) {
	mp_drv_ds1307_t *rtc = face->user;
	unsigned char raw[8];

	if(!rtc || !time)
		return(FALSE);
	if(!_ds1307_span(time->year, 2000, 2099) || !_ds1307_span(time->month, 1, 12))
		return(FALSE);
	if(!_ds1307_span(time->day, 1, 31) || !_ds1307_span(time->wday, 1, 7))
		return(FALSE);
	if(!_ds1307_span(time->hour, 0, 23) || !_ds1307_span(time->minute, 0, 59) || !_ds1307_span(time->second, 0, 59))
		return(FALSE);
	raw[0] = (unsigned char)(_ds1307_to_bcd(time->second) | (time->running == YES ? 0 : DS1307_CH));
	raw[1] = _ds1307_to_bcd(time->minute);
	raw[2] = _ds1307_to_bcd(time->hour);
	raw[3] = time->wday;
	raw[4] = _ds1307_to_bcd(time->day);
	raw[5] = _ds1307_to_bcd(time->month);
	raw[6] = _ds1307_to_bcd((unsigned char)(time->year - 2000));
	raw[7] = rtc->haveRegs == YES ? rtc->regs[7] : 0;
	_ds1307_queue(rtc, DS1307_REG_SECONDS, raw, 8);
	return(TRUE);
}

static mp_ret_t _ds1307_running_set(mp_device_registry_t *device, const mp_device_value_t *value) {
	mp_drv_ds1307_t *rtc;
	unsigned char second;

	if(!device || !device->driver || !value)
		return(FALSE);
	rtc = device->driver->self;
	if(rtc->haveRegs != YES)
		return(FALSE);
	second = (unsigned char)(rtc->regs[0] & 0x7F);
	if(value->as.on)
		second = (unsigned char)(second & ~DS1307_CH);
	else
		second = (unsigned char)(second | DS1307_CH);
	_ds1307_queue(rtc, DS1307_REG_SECONDS, &second, 1);
	return(TRUE);
}

static mp_ret_t _ds1307_running_get(mp_device_registry_t *device, mp_device_value_t *value) {
	mp_drv_ds1307_t *rtc;

	if(!device || !device->driver || !value)
		return(FALSE);
	rtc = device->driver->self;
	if(rtc->haveRegs != YES)
		return(FALSE);
	value->as.on = (rtc->regs[0] & DS1307_CH) ? 0 : 1;
	return(TRUE);
}

static mp_ret_t _ds1307_sqw_set(mp_device_registry_t *device, const mp_device_value_t *value) {
	mp_drv_ds1307_t *rtc;
	unsigned char control;
	static const unsigned char byte[] = { 0x00, 0x10, 0x11, 0x12, 0x13 };

	if(!device || !device->driver || !value)
		return(FALSE);
	if(value->as.index < 0 || value->as.index > 4)
		return(FALSE);
	rtc = device->driver->self;
	control = byte[value->as.index];
	_ds1307_queue(rtc, DS1307_REG_CONTROL, &control, 1);
	return(TRUE);
}

static mp_ret_t _ds1307_sqw_get(mp_device_registry_t *device, mp_device_value_t *value) {
	mp_drv_ds1307_t *rtc;
	unsigned char control;

	if(!device || !device->driver || !value)
		return(FALSE);
	rtc = device->driver->self;
	if(rtc->haveRegs != YES)
		return(FALSE);
	control = rtc->regs[7];
	if((control & DS1307_SQWE) == 0)
		value->as.index = 0;
	else
		value->as.index = (control & 0x03) + 1;
	return(TRUE);
}

static void _ds1307_start_driver(void *self) {
	mp_drv_ds1307_t *rtc = self;

	rtc->watched = YES;
	_ds1307_kick(rtc);
}

static void _ds1307_stop_driver(void *self) {
	mp_drv_ds1307_t *rtc = self;

	rtc->watched = NO;
	if(rtc->task)
		mp_task_delay(rtc->task, MP_TASK_FOREVER);
}

static MP_TASK(_ds1307_task) {
	mp_drv_ds1307_t *rtc = task->user;

	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}
	_ds1307_flush(rtc);
	if(rtc->watched == NO) {
		mp_task_delay(task, MP_TASK_FOREVER);
		return;
	}
	if(_ds1307_read(rtc, rtc->regs)) {
		rtc->haveRegs = YES;
		_ds1307_publish(rtc);
	}
	else {
		MP_INTERRUPT_SAFE_BEGIN
		rtc->time.valid = NO;
		MP_INTERRUPT_SAFE_END
	}
	if(rtc->watched == YES)
		mp_task_delay(task, _ds1307_ticks(1000));
	else
		mp_task_delay(task, MP_TASK_FOREVER);
}

static void _ds1307_bind(mp_drv_ds1307_t *rtc) {
	static const mp_device_driver_ops_t ops = {
		"rtc/ds1307", "Maxim DS1307", "1.0.0",
		(unsigned int)(1u << MP_DEVICE_CLOCK),
		NULL, NULL, NULL, _ds1307_start_driver, _ds1307_stop_driver
	};

	if(rtc->binding.kernel == rtc->kernel)
		return;
	if(mp_device_driver_register(rtc->kernel, &rtc->binding, &ops, rtc, NULL) != TRUE)
		mp_printk("DS1307: can not register the driver");
}

static void _ds1307_device(mp_drv_ds1307_t *rtc) {
	if(rtc->binding.kernel != rtc->kernel)
		return;
	memset(&rtc->time, 0, sizeof(rtc->time));
	rtc->time.set = _ds1307_time_set;
	rtc->time.user = rtc;
	if(!mp_device_registry_register(&rtc->binding, &rtc->dev, MP_DEVICE_CLOCK, 0, &rtc->time, rtc->model, rtc->serial, NULL)) {
		mp_printk("DS1307: can not register the clock");
		return;
	}
	rtc->dev.label = "Maxim DS1307";
	rtc->dev.commands = _ds1307_commands;
	mp_device_identify(rtc, "Maxim DS1307", rtc->model, rtc->serial, NULL);
	_ds1307_publish(rtc);
	rtc->task = mp_task_create(&rtc->kernel->tasks, "DS1307", _ds1307_task, rtc, MP_TASK_FOREVER);
	if(!rtc->task)
		mp_printk("DS1307: no task");
}

mp_ret_t mp_drv_ds1307_init(mp_kernel_t *kernel, mp_drv_ds1307_t *rtc, mp_options_t *options, char *who) {
	const char *model;
	const char *serial;
	char *sda;
	char *clk;

	memset(rtc, 0, sizeof(*rtc));
	rtc->kernel = kernel;
	if(mp_device_options_identity(options, &model, &serial) != TRUE) {
		mp_printk("DS1307: model and serial are required");
		return(FALSE);
	}
	rtc->model = model;
	rtc->serial = serial;
	rtc->address = _ds1307_address(mp_options_get(options, "address"));

	sda = mp_options_get(options, "sda");
	clk = mp_options_get(options, "clk");
	rtc->sda = mp_gpio_text_handle(sda, "DS1307 SDA");
	rtc->scl = mp_gpio_text_handle(clk, "DS1307 SCL");
	if(!rtc->sda || !rtc->scl) {
		mp_printk("DS1307: need sda and clk");
		mp_drv_ds1307_fini(rtc);
		return(FALSE);
	}
	_ds1307_line(rtc->sda, 1);
	_ds1307_line(rtc->scl, 1);

	_ds1307_bind(rtc);
	if(_ds1307_read(rtc, rtc->regs)) {
		rtc->haveRegs = YES;
		_ds1307_device(rtc);
		mp_printk("DS1307: clock on the bus");
	}
	else
		mp_printk("DS1307: no answer at 0x%02x", rtc->address);

	(void)who;
	return(TRUE);
}

void mp_drv_ds1307_fini(mp_drv_ds1307_t *rtc) {
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
	if(rtc->sda) {
		mp_gpio_release(rtc->sda);
		rtc->sda = NULL;
	}
	if(rtc->scl) {
		mp_gpio_release(rtc->scl);
		rtc->scl = NULL;
	}
}

#ifdef SUPPORT_COMMON_DEVICE
static mp_ret_t _ds1307_device_open(mp_kernel_t *kernel, void *self, mp_options_t *options, char *who) {
	return(mp_drv_ds1307_init(kernel, self, options, who));
}

static void _ds1307_device_close(void *self) {
	mp_drv_ds1307_fini(self);
}

const mp_device_ops_t mp_drv_ds1307_device = {
	"DS1307", MP_DEVICE_CLOCK, _ds1307_device_open, _ds1307_device_close, NULL, NULL,
	"Maxim DS1307", "1.0.0"
};
#endif

/**@}*/

#endif
