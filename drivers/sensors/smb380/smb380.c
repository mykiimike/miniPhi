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

#ifdef SUPPORT_DRV_SMB380

/**
@defgroup mpDrvSmb380 Bosch SMB380

@ingroup mpDriver

@{
*/

/* chip id lives in bits 2:0 of register 0x00 and is fixed to 010b */
#define SMB380_REG_CHIP_ID   0x00
#define SMB380_CHIP_ID       0x02

/* 0x02 to 0x07 are x, y, z (LSB then MSB); the next byte is the temperature */
#define SMB380_REG_ACC_X_LSB 0x02

/* bit 0 sleeps the sensor; writing 0 wakes it without a soft reset */
#define SMB380_REG_CTRL      0x0A

/*
 * Register 0x14: bits 4:3 are the range, bits 2:0 the bandwidth.
 * Bits 7:5 are factory calibration and must be written back unchanged.
 */
#define SMB380_REG_RANGE     0x14
#define SMB380_CAL_MASK      0xE0

static const unsigned short _smb380_hz[] = { 25, 50, 100, 190, 375, 750, 1500 };
static const char *_smb380_bw_name[] = { "25", "50", "100", "190", "375", "750", "1500" };

static void _smb380_onChipId(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _smb380_onWake(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _smb380_onRange(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _smb380_onRangeWritten(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _smb380_onSample(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _smb380_onCtrl(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _smb380_ctrl(mp_drv_smb380_t *smb, unsigned char value);
static MP_TASK(_smb380_task);

static unsigned long _smb380_ticks(unsigned long ms) {
	unsigned long hz = mp_clock_tick_hz();
	unsigned long ticks = hz ? (ms * hz) / 1000 : ms;

	return(ticks ? ticks : 1);
}

static int _smb380_decimal(char *text, int fallback) {
	int value = 0;

	if(!text || *text < '0' || *text > '9')
		return(fallback);
	while(*text >= '0' && *text <= '9') {
		value = value * 10 + (*text - '0');
		text++;
	}
	return(value);
}

/* "0x38" or "38"; the datasheet fixes the metal option at 0x38 */
static unsigned char _smb380_address(char *text) {
	unsigned int value = 0;

	if(!text || !*text)
		return(0x38);
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
			return(0x38);
	}
	if(value == 0 || value > 0x7F)
		return(0x38);
	return((unsigned char)value);
}

static unsigned char _smb380_range(char *text) {
	if(text && text[0] == '4' && text[1] == '\0')
		return(1);
	if(text && text[0] == '8' && text[1] == '\0')
		return(2);
	return(0);
}

static unsigned char _smb380_bandwidth(char *text) {
	unsigned int index;

	for(index = 0; text && index < sizeof(_smb380_bw_name) / sizeof(_smb380_bw_name[0]); index++)
		if(mp_options_cmp(text, (char *)_smb380_bw_name[index]) == TRUE)
			return((unsigned char)index);
	return(0);
}

/* 10-bit two's complement: the two low bits sit in bits 7:6 of the LSB */
static int _smb380_axis(unsigned char lsb, unsigned char msb) {
	int raw = ((int)msb << 2) | (lsb >> 6);

	if(raw & 0x200)
		raw -= 0x400;
	return(raw);
}

static mp_ret_t _smb380_write(mp_drv_smb380_t *smb, unsigned char reg, unsigned char value, mp_regMaster_cb_t done) {
	unsigned char *buf = mp_mem_alloc(smb->kernel, 2);

	if(!buf)
		return(FALSE);
	buf[0] = reg;
	buf[1] = value;
	return(mp_regMaster_write(&smb->regMaster, buf, 2, done, smb));
}


mp_ret_t mp_drv_smb380_init(mp_kernel_t *kernel, mp_drv_smb380_t *smb, mp_options_t *options, char *who) {
	char *value;
	int period;
	mp_ret_t ret;

	memset(smb, 0, sizeof(*smb));
	smb->kernel = kernel;
	smb->address = _smb380_address(mp_options_get(options, "address"));
	smb->range = _smb380_range(mp_options_get(options, "range"));
	smb->bandwidth = _smb380_bandwidth(mp_options_get(options, "bandwidth"));
	smb->countsPerG = 256 >> smb->range;

	period = _smb380_decimal(mp_options_get(options, "period"), 50);
	if(period < 10)
		period = 10;
	if(period > 1000)
		period = 1000;
	smb->period = _smb380_ticks((unsigned long)period);

	value = mp_options_get(options, "int");
	if(value && *value) {
		smb->irq = mp_gpio_text_handle(value, "SMB380 INT");
		if(!smb->irq) {
			mp_printk("SMB380: need a valid interrupt pin");
			return(FALSE);
		}
		/* the pad idles low; the pull holds it until the chip drives it */
		mp_gpio_unset(smb->irq);
		mp_gpio_mode(smb->irq, MP_GPIO_MODE_INPUT_PULL);
	}

	ret = mp_i2c_open(kernel, &smb->i2c, options, "SMB380");
	if(ret == FALSE) {
		if(smb->irq)
			mp_gpio_release(smb->irq);
		return(FALSE);
	}

	{
		mp_options_t setup[] = {
			{ "frequency", "400000" },
			{ "role", "master" },
			{ NULL, NULL }
		};

		ret = mp_i2c_setup(&smb->i2c, setup);
	}
	if(ret == FALSE) {
		mp_drv_smb380_fini(smb);
		return(FALSE);
	}

	mp_i2c_setSlaveAddress(&smb->i2c, smb->address);

	ret = mp_regMaster_init_i2c(kernel, &smb->regMaster, &smb->i2c, smb, "SMB380 I2C");
	if(ret == FALSE) {
		mp_printk("SMB380: can not start regMaster");
		mp_i2c_close(&smb->i2c);
		if(smb->irq)
			mp_gpio_release(smb->irq);
		smb->irq = NULL;
		return(FALSE);
	}

	/* the register address is one byte; its high bit stays 0 */
	mp_regMaster_read(
		&smb->regMaster,
		mp_regMaster_register(SMB380_REG_CHIP_ID), 1,
		&smb->chipId, 1,
		_smb380_onChipId, smb
	);

	(void)who;
	return(TRUE);
}

void mp_drv_smb380_fini(mp_drv_smb380_t *smb) {
	if(smb->task) {
		mp_task_destroy(smb->task);
		smb->task = NULL;
	}

	smb->busy = NO;
	mp_regMaster_fini(&smb->regMaster);
	mp_i2c_close(&smb->i2c);

	if(smb->irq) {
		mp_gpio_release(smb->irq);
		smb->irq = NULL;
	}

	mp_sensor_unregister(smb->kernel, &smb->accel);
	mp_sensor_unregister(smb->kernel, &smb->temperature);
	smb->accel = NULL;
	smb->temperature = NULL;
}

static void _smb380_onChipId(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_smb380_t *smb = operand->user;

	if(terminate == YES)
		return;
	if((smb->chipId & 0x07) != SMB380_CHIP_ID) {
		mp_printk("SMB380: chip id 0x%02x, expected 0x02", smb->chipId);
		return;
	}
	if(_smb380_write(smb, SMB380_REG_CTRL, 0, _smb380_onWake) == FALSE)
		mp_printk("SMB380: can not wake the sensor");
}

static void _smb380_onWake(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_smb380_t *smb = operand->user;

	mp_mem_free(smb->kernel, operand->reg);
	if(terminate == YES)
		return;

	mp_regMaster_read(
		&smb->regMaster,
		mp_regMaster_register(SMB380_REG_RANGE), 1,
		&smb->setup, 1,
		_smb380_onRange, smb
	);
}

static void _smb380_onRange(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_smb380_t *smb = operand->user;
	unsigned char value;

	if(terminate == YES)
		return;

	value = (unsigned char)((smb->setup & SMB380_CAL_MASK) | (smb->range << 3) | smb->bandwidth);
	if(_smb380_write(smb, SMB380_REG_RANGE, value, _smb380_onRangeWritten) == FALSE)
		mp_printk("SMB380: can not set the range");
}

static void _smb380_onRangeWritten(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_smb380_t *smb = operand->user;

	mp_mem_free(smb->kernel, operand->reg);
	if(terminate == YES)
		return;

	smb->accel = mp_sensor_register(smb->kernel, MP_SENSOR_ACCELEROMETER, "SMB380");
	smb->temperature = mp_sensor_register(smb->kernel, MP_SENSOR_TEMPERATURE, "SMB380 Temp");
	if(!smb->accel || !smb->temperature) {
		mp_sensor_unregister(smb->kernel, &smb->accel);
		mp_sensor_unregister(smb->kernel, &smb->temperature);
		mp_printk("SMB380: can not register the sensors");
		return;
	}

	smb->task = mp_task_create(&smb->kernel->tasks, "SMB380", _smb380_task, smb, smb->period);
	if(!smb->task) {
		mp_sensor_unregister(smb->kernel, &smb->accel);
		mp_sensor_unregister(smb->kernel, &smb->temperature);
		mp_printk("SMB380: can not create the sample task");
		return;
	}

	mp_printk("SMB380 at 0x%02x, +/- %u g, %u Hz",
		smb->address,
		(unsigned int)(2 << smb->range),
		(unsigned int)_smb380_hz[smb->bandwidth]);
}

static void _smb380_ctrl(mp_drv_smb380_t *smb, unsigned char value) {
	if(smb->busy == YES)
		return;
	smb->ctrl = value;
	if(_smb380_write(smb, SMB380_REG_CTRL, value, _smb380_onCtrl) == TRUE)
		smb->busy = YES;
}

static void _smb380_onCtrl(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_smb380_t *smb = operand->user;
	unsigned char want = smb->asleep == YES ? 1 : 0;

	mp_mem_free(smb->kernel, operand->reg);
	smb->busy = NO;
	if(terminate == YES || !smb->task)
		return;
	if(smb->ctrl != want) {
		_smb380_ctrl(smb, want);
		return;
	}
	mp_task_delay(smb->task, want ? MP_TASK_FOREVER : smb->period);
}

static void _smb380_onSample(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_smb380_t *smb = operand->user;
	int x, y, z;

	smb->busy = NO;
	if(terminate == YES)
		return;
	if(smb->asleep == YES) {
		_smb380_ctrl(smb, 1);
		return;
	}

	x = _smb380_axis(smb->raw[0], smb->raw[1]);
	y = _smb380_axis(smb->raw[2], smb->raw[3]);
	z = _smb380_axis(smb->raw[4], smb->raw[5]);

	mp_sensor_publish3(smb->accel,
		(float)x / (float)smb->countsPerG,
		(float)y / (float)smb->countsPerG,
		(float)z / (float)smb->countsPerG);
	/* code 0x00 is -30 °C, and each step is 0.5 °C */
	mp_sensor_publish(smb->temperature, (float)smb->raw[6] * 0.5f - 30.0f);
}

static MP_TASK(_smb380_task) {
	mp_drv_smb380_t *smb = task->user;

	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}
	if(smb->asleep == YES) {
		mp_task_delay(task, MP_TASK_FOREVER);
		_smb380_ctrl(smb, 1);
		return;
	}
	if(smb->busy == YES)
		return;

	smb->busy = YES;
	if(mp_regMaster_read(
		&smb->regMaster,
		mp_regMaster_register(SMB380_REG_ACC_X_LSB), 1,
		smb->raw, 7,
		_smb380_onSample, smb
	) == FALSE)
		smb->busy = NO;
}

#ifdef SUPPORT_COMMON_DEVICE
static mp_ret_t _smb380_device_open(mp_kernel_t *kernel, void *self, mp_options_t *options, char *who) {
	return(mp_drv_smb380_init(kernel, self, options, who));
}

static void _smb380_device_close(void *self) {
	mp_drv_smb380_fini(self);
}

/* 0 sleeps the chip until the screen comes back; the sensors stay registered */
static void _smb380_device_set(void *self, int value) {
	mp_drv_smb380_t *smb = self;
	mp_bool_t asleep = value ? NO : YES;

	if(!smb->task || smb->asleep == asleep)
		return;
	smb->asleep = asleep;
	mp_task_delay(smb->task, asleep == YES ? MP_TASK_FOREVER : smb->period);
	_smb380_ctrl(smb, asleep == YES ? 1 : 0);
}

const mp_device_driver_t mp_drv_smb380_device = {
	"SMB380", MP_DEVICE_ACCELEROMETER, _smb380_device_open, _smb380_device_close, NULL, _smb380_device_set
};
#endif

#endif
