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

#ifdef SUPPORT_DRV_INA3221

/**
 * @defgroup mpDrvIna3221 TI INA3221
 * @ingroup mpDrv
 *
 * Three shunt and bus channels. The chip does not compute current; the
 * driver divides the shunt voltage by the board shunt.
 * @{
 */

#define _INA3221_REG_CONFIG 0x00
#define _INA3221_REG_MANUF  0xFE
#define _INA3221_REG_DIE    0xFF
#define _INA3221_MANUF      0x5449
#define _INA3221_DIE        0x3220

static const char *_ina3221_serial[MP_DRV_INA3221_CHANNELS] = { "ch1", "ch2", "ch3" };
static const char *_ina3221_labelKey[MP_DRV_INA3221_CHANNELS] = { "label1", "label2", "label3" };
static const char *_ina3221_labelFallback[MP_DRV_INA3221_CHANNELS] = { "CH1", "CH2", "CH3" };

static void _ina3221_sample(mp_drv_ina3221_t *meter);
static MP_TASK(_ina3221_task);

static unsigned long _ina3221_ticks(unsigned long ms) {
	unsigned long hz = mp_clock_tick_hz();
	unsigned long ticks;

	if(hz == 0 || ms == 0)
		return(1);
	ticks = (ms / 1000) * hz + ((ms % 1000) * hz) / 1000;
	return(ticks ? ticks : 1);
}

static void _ina3221_copy(char *dst, int cap, const char *src) {
	int n = 0;

	if(cap < 1)
		return;
	if(!src)
		src = "";
	while(src[n] && n < cap - 1) {
		dst[n] = src[n];
		n++;
	}
	dst[n] = 0;
}

/* "0x40" or "40". Out of 0x40..0x43 stays at 0x40. */
static unsigned char _ina3221_address(char *text) {
	unsigned int value = 0;

	if(!text || !*text)
		return(0x40);
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
			return(0x40);
	}
	if(value < 0x40 || value > 0x43)
		return(0x40);
	return((unsigned char)value);
}

/* digits 1, 2 and 3. Missing or empty enables every channel. */
static unsigned char _ina3221_channels(char *text) {
	unsigned char mask = 0;

	if(!text || !*text)
		return(0x07);
	while(*text) {
		if(*text >= '1' && *text <= '3')
			mask |= (unsigned char)(1u << (*text - '1'));
		text++;
	}
	return(mask ? mask : 0x07);
}

/* milliohms. Missing, zero or junk is 50. */
static unsigned short _ina3221_shunt(char *text) {
	unsigned int value = 0;

	if(!text || !*text)
		return(50);
	while(*text >= '0' && *text <= '9') {
		value = value * 10u + (unsigned int)(*text - '0');
		if(value > 10000u)
			return(50);
		text++;
	}
	if(*text || value == 0)
		return(50);
	return((unsigned short)value);
}

static int _ina3221_read(mp_drv_ina3221_t *meter, unsigned char reg, unsigned short *out) {
	unsigned char buf[2];

	if(!mp_i2c_read_reg(&meter->bus, meter->address, reg, buf, 2))
		return(0);
	*out = (unsigned short)(((unsigned short)buf[0] << 8) | buf[1]);
	return(1);
}

static int _ina3221_write(mp_drv_ina3221_t *meter, unsigned char reg, unsigned short value) {
	unsigned char buf[2];

	buf[0] = (unsigned char)(value >> 8);
	buf[1] = (unsigned char)(value & 0xFF);
	return(mp_i2c_write_reg(&meter->bus, meter->address, reg, buf, 2));
}

/* 16 averages, 1.1 ms shunt and bus. run keeps continuous shunt-and-bus mode. */
static unsigned short _ina3221_config(mp_drv_ina3221_t *meter, int run) {
	unsigned short cfg = 0;

	cfg |= (unsigned short)(2u << 9);
	cfg |= (unsigned short)(4u << 6);
	cfg |= (unsigned short)(4u << 3);
	if(meter->mask & 0x01)
		cfg |= (unsigned short)(1u << 14);
	if(meter->mask & 0x02)
		cfg |= (unsigned short)(1u << 13);
	if(meter->mask & 0x04)
		cfg |= (unsigned short)(1u << 12);
	if(run)
		cfg |= 7u;
	return(cfg);
}

/* bits 15:3, 13-bit signed, 40 µV per step */
static float _ina3221_shunt_v(unsigned short raw) {
	return((float)((int16_t)(raw & 0xFFF8) >> 3) * 0.00004f);
}

/* bits 15:3, unsigned, 8 mV per step */
static float _ina3221_bus_v(unsigned short raw) {
	return((float)((raw >> 3) & 0x1FFF) * 0.008f);
}

#ifdef SUPPORT_COMMON_DEVICE

static int _ina3221_reg(mp_drv_ina3221_t *meter, mp_device_registry_t *dev, mp_device_measure_t *measure,
	mp_device_class_t class, int channel, const char *serial, const char *label) {
	memset(measure, 0, sizeof(*measure));
	if(!mp_device_registry_register(&meter->binding, dev, class, channel, measure, "ina3221", serial, NULL))
		return(0);
	dev->label = label;
	return(1);
}

static void _ina3221_start(void *self) {
	mp_drv_ina3221_t *meter = self;

	meter->watched = YES;
	if(meter->task)
		mp_task_delay(meter->task, 1);
}

static void _ina3221_stop(void *self) {
	mp_drv_ina3221_t *meter = self;

	meter->watched = NO;
	if(meter->task)
		mp_task_delay(meter->task, MP_TASK_FOREVER);
}

static void _ina3221_devices(mp_drv_ina3221_t *meter) {
	int hw, slot;

	if(meter->binding.kernel != meter->kernel)
		return;
	slot = 0;
	for(hw = 0; hw < MP_DRV_INA3221_CHANNELS; hw++) {
		if((meter->mask & (unsigned char)(1u << hw)) == 0)
			continue;
		if(slot >= MP_DRV_INA3221_CHANNELS)
			break;
		meter->hw[slot] = (unsigned char)hw;
		if(!_ina3221_reg(meter, &meter->volt[slot], &meter->voltM[slot], MP_DEVICE_VOLTAGE, slot, _ina3221_serial[hw], meter->label[hw])) {
			mp_printk("INA3221: voltage %s did not register", _ina3221_serial[hw]);
			continue;
		}
		if(!_ina3221_reg(meter, &meter->curr[slot], &meter->currM[slot], MP_DEVICE_CURRENT, slot, _ina3221_serial[hw], meter->label[hw]))
			mp_printk("INA3221: current %s did not register", _ina3221_serial[hw]);
		if(!_ina3221_reg(meter, &meter->power[slot], &meter->powerM[slot], MP_DEVICE_POWER, slot, _ina3221_serial[hw], meter->label[hw]))
			mp_printk("INA3221: power %s did not register", _ina3221_serial[hw]);
		slot++;
	}
	meter->count = (unsigned char)slot;
}

#endif

static void _ina3221_sample(mp_drv_ina3221_t *meter) {
	int slot;

	for(slot = 0; slot < (int)meter->count; slot++) {
		unsigned char hw = meter->hw[slot];
		unsigned short rawShunt, rawBus;
		float shunt, bus, amps;

		if(!_ina3221_read(meter, (unsigned char)(1 + hw * 2), &rawShunt))
			continue;
		if(!_ina3221_read(meter, (unsigned char)(2 + hw * 2), &rawBus))
			continue;
		shunt = _ina3221_shunt_v(rawShunt);
		bus = _ina3221_bus_v(rawBus);
		amps = shunt * meter->ampsPerVolt;
#ifdef SUPPORT_COMMON_DEVICE
		if(meter->volt[slot].driver)
			mp_device_measure_publish(&meter->voltM[slot], bus);
		if(meter->curr[slot].driver)
			mp_device_measure_publish(&meter->currM[slot], amps);
		if(meter->power[slot].driver)
			mp_device_measure_publish(&meter->powerM[slot], bus * amps);
#else
		(void)bus;
		(void)amps;
#endif
	}
}

static MP_TASK(_ina3221_task) {
	mp_drv_ina3221_t *meter = task->user;

	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}
	if(meter->watched == NO) {
		if(meter->running == YES) {
			_ina3221_write(meter, _INA3221_REG_CONFIG, _ina3221_config(meter, 0));
			meter->running = NO;
		}
		mp_task_delay(task, MP_TASK_FOREVER);
		return;
	}
	if(meter->running == NO) {
		if(!_ina3221_write(meter, _INA3221_REG_CONFIG, _ina3221_config(meter, 1)))
			mp_printk("INA3221: configuration was not accepted");
		meter->running = YES;
	}
	_ina3221_sample(meter);
	if(meter->watched == YES)
		mp_task_delay(task, _ina3221_ticks(200));
	else
		mp_task_delay(task, MP_TASK_FOREVER);
}

mp_ret_t mp_drv_ina3221_init(mp_kernel_t *kernel, mp_drv_ina3221_t *meter, mp_options_t *options, char *who) {
	unsigned short manuf, die;
	unsigned short shunt;
	int hw;

	memset(meter, 0, sizeof(*meter));
	meter->kernel = kernel;
	if(!who || !who[0])
		who = "INA3221";
	meter->address = _ina3221_address(mp_options_get(options, "address"));
	meter->mask = _ina3221_channels(mp_options_get(options, "channels"));
	shunt = _ina3221_shunt(mp_options_get(options, "shunt"));
	meter->ampsPerVolt = 1000.0f / (float)shunt;
	for(hw = 0; hw < MP_DRV_INA3221_CHANNELS; hw++) {
		char *text = mp_options_get(options, (char *)_ina3221_labelKey[hw]);

		_ina3221_copy(meter->label[hw], (int)sizeof(meter->label[hw]),
			(text && text[0]) ? text : _ina3221_labelFallback[hw]);
	}

	if(!mp_i2c_open(kernel, &meter->bus, options, who)) {
		mp_printk("INA3221: SDA and SCL are required");
		meter->kernel = NULL;
		return(FALSE);
	}
	if(!_ina3221_read(meter, _INA3221_REG_MANUF, &manuf) || manuf != _INA3221_MANUF) {
		mp_printk("INA3221: no TI chip at 0x%02x", meter->address);
		mp_i2c_close(&meter->bus);
		meter->kernel = NULL;
		return(FALSE);
	}
	if(!_ina3221_read(meter, _INA3221_REG_DIE, &die) || die != _INA3221_DIE)
		mp_printk("INA3221: die id 0x%04x", die);

	meter->task = mp_task_create(&kernel->tasks, who, _ina3221_task, meter, MP_TASK_FOREVER);
	if(!meter->task) {
		mp_printk("INA3221: no task");
		mp_i2c_close(&meter->bus);
		meter->kernel = NULL;
		return(FALSE);
	}

#ifdef SUPPORT_COMMON_DEVICE
	{
		static const mp_device_driver_ops_t ops = {
			"sensor/ina3221", "TI INA3221", "1.0.0",
			(unsigned int)((1u << MP_DEVICE_VOLTAGE) | (1u << MP_DEVICE_CURRENT) | (1u << MP_DEVICE_POWER)),
			NULL, NULL, NULL, _ina3221_start, _ina3221_stop
		};

		if(mp_device_driver_register(kernel, &meter->binding, &ops, meter, NULL) != TRUE) {
			mp_printk("INA3221: can not register the driver");
			mp_drv_ina3221_fini(meter);
			return(FALSE);
		}
		_ina3221_devices(meter);
	}
#endif
	mp_printk("INA3221: %d channel%s, %u mOhm, 0x%02x",
		meter->count, meter->count == 1 ? "" : "s", shunt, meter->address);
	return(TRUE);
}

void mp_drv_ina3221_fini(mp_drv_ina3221_t *meter) {
	int slot;

	if(!meter || !meter->kernel)
		return;
	meter->watched = NO;
	if(meter->task) {
		mp_task_destroy(meter->task);
		meter->task = NULL;
	}
#ifdef SUPPORT_COMMON_DEVICE
	for(slot = 0; slot < MP_DRV_INA3221_CHANNELS; slot++) {
		mp_device_registry_unregister(&meter->power[slot]);
		mp_device_registry_unregister(&meter->curr[slot]);
		mp_device_registry_unregister(&meter->volt[slot]);
	}
	if(meter->binding.kernel)
		mp_device_driver_unregister(meter->kernel, &meter->binding);
#endif
	mp_i2c_close(&meter->bus);
	meter->kernel = NULL;
	(void)slot;
}

#ifdef SUPPORT_COMMON_DEVICE

static mp_ret_t _ina3221_open(mp_kernel_t *kernel, void *self, mp_options_t *options, char *who) {
	return(mp_drv_ina3221_init(kernel, self, options, who));
}

static void _ina3221_close(void *self) {
	mp_drv_ina3221_fini(self);
}

const mp_device_ops_t mp_drv_ina3221_device = {
	"sensor/ina3221", MP_DEVICE_VOLTAGE, _ina3221_open, _ina3221_close, NULL, NULL,
	"TI INA3221", "1.0.0"
};

#endif

/** @} */

#endif
