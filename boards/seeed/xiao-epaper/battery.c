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

#ifdef SUPPORT_COMMON_DEVICE

/**
 * TRMNL 7.5" (OG) DIY kit, EE04.
 * GPIO6 powers the divider. GPIO1 is ADC1 channel 0, 12 bit, 11 dB.
 * Seeed scale: (count / 4095) * 3.6 * 2 * 0.968.
 * The divider stays off between samples.
 */

static MP_TASK(_mp_board_battery_task);

static struct {
	mp_kernel_t *kernel;
	mp_gpio_port_t *sense;
	mp_gpio_port_t *enable;
	mp_task_t *task;
	mp_device_measure_t face;
	mp_device_driver_t binding;
	mp_device_registry_t device;
} _mp_board_battery;

static void _mp_board_battery_sample(void) {
	int raw;
	float volts;

	mp_gpio_set(_mp_board_battery.enable);
	mp_clock_delay(10);
	raw = mp_adc_convert(1);
	mp_gpio_unset(_mp_board_battery.enable);
	if(raw < 0)
		return;
	volts = (float)raw / 4095.0f * 3.6f * 2.0f * 0.968f;
	mp_device_measure_publish(&_mp_board_battery.face, volts);
}

static MP_TASK(_mp_board_battery_task) {
	if(task->signal == MP_TASK_SIG_STOP) {
		mp_gpio_unset(_mp_board_battery.enable);
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}
	_mp_board_battery_sample();
}

mp_ret_t mp_board_battery(mp_kernel_t *kernel) {
	static const mp_device_driver_ops_t ops = {
		"board/battery", "Cell voltage", "1.0.0",
		(unsigned int)(1u << MP_DEVICE_VOLTAGE),
		NULL, NULL, NULL, NULL, NULL
	};

	memset(&_mp_board_battery, 0, sizeof(_mp_board_battery));
	_mp_board_battery.kernel = kernel;
	_mp_board_battery.sense = mp_gpio_text_handle("pa1", "Battery");
	_mp_board_battery.enable = mp_gpio_text_handle("pa6", "Battery enable");
	if(!_mp_board_battery.sense || !_mp_board_battery.enable) {
		mp_printk("TRMNL battery: sense or enable is not free");
		if(_mp_board_battery.sense)
			mp_gpio_release(_mp_board_battery.sense);
		if(_mp_board_battery.enable)
			mp_gpio_release(_mp_board_battery.enable);
		return(FALSE);
	}

	mp_gpio_direction(_mp_board_battery.enable, MP_GPIO_OUTPUT);
	mp_gpio_unset(_mp_board_battery.enable);

	if(mp_device_driver_register(kernel, &_mp_board_battery.binding, &ops, &_mp_board_battery, NULL) == FALSE)
		return(FALSE);
	if(!mp_device_registry_register(&_mp_board_battery.binding, &_mp_board_battery.device, MP_DEVICE_VOLTAGE, 0, &_mp_board_battery.face, "gpio", "battery", NULL)) {
		mp_device_driver_unregister(kernel, &_mp_board_battery.binding);
		return(FALSE);
	}
	_mp_board_battery.device.label = "Cell voltage";

	_mp_board_battery.task = mp_task_create(&kernel->tasks, "Battery", _mp_board_battery_task, &_mp_board_battery, 5000);
	if(!_mp_board_battery.task) {
		mp_printk("TRMNL battery: no task");
		return(FALSE);
	}
	mp_printk("Battery gpio.battery on pa1, enable pa6");
	return(TRUE);
}

#else

mp_ret_t mp_board_battery(mp_kernel_t *kernel) {
	(void)kernel;
	return(TRUE);
}

#endif
