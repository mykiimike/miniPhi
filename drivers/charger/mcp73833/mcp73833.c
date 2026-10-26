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

#ifdef SUPPORT_DRV_MCP73833

/**
 * MCP73833 status lines. Open drain, active low. A subscriber samples them.
 */

static unsigned long _mcp73833_ticks(unsigned long ms) {
	unsigned long hz = mp_clock_tick_hz();
	unsigned long ticks = hz ? (ms * hz) / 1000 : ms;

	return(ticks ? ticks : 1);
}

static void _mcp73833_sample(mp_drv_mcp73833_t *chg) {
	int stat1 = mp_gpio_read(chg->stat1) ? 1 : 0;
	int stat2 = mp_gpio_read(chg->stat2) ? 1 : 0;

	if(stat1 == 0 && stat2 != 0)
		mp_device_charge_sample(&chg->charge, MP_DEVICE_CHARGE_CHARGING);
	else if(stat2 == 0 && stat1 != 0)
		mp_device_charge_sample(&chg->charge, MP_DEVICE_CHARGE_DONE);
	else if(stat1 != 0 && stat2 != 0)
		mp_device_charge_sample(&chg->charge, MP_DEVICE_CHARGE_WAITING);
}

static MP_TASK(_mcp73833_task) {
	mp_drv_mcp73833_t *chg = task->user;

	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}
	_mcp73833_sample(chg);
	mp_task_delay(task, _mcp73833_ticks(500));
}

static void _mcp73833_start(void *self) {
	mp_drv_mcp73833_t *chg = self;

	_mcp73833_sample(chg);
	chg->task = mp_task_create(&chg->kernel->tasks, "MCP73833", _mcp73833_task, chg, _mcp73833_ticks(500));
	if(!chg->task)
		mp_printk("MCP73833: no task");
}

static void _mcp73833_stop(void *self) {
	mp_drv_mcp73833_t *chg = self;

	if(!chg->task)
		return;
	mp_task_destroy(chg->task);
	chg->task = NULL;
}

static void _mcp73833_bind(mp_drv_mcp73833_t *chg) {
	static const mp_device_driver_ops_t ops = {
		"charger/mcp73833", "Microchip MCP73833", "1.0.0",
		(unsigned int)(1u << MP_DEVICE_CHARGER),
		NULL, NULL, NULL, _mcp73833_start, _mcp73833_stop
	};

	if(chg->binding.kernel == chg->kernel)
		return;
	if(mp_device_driver_register(chg->kernel, &chg->binding, &ops, chg, NULL) != TRUE)
		mp_printk("MCP73833: can not register the driver");
}

mp_ret_t mp_drv_mcp73833_init(mp_kernel_t *kernel, mp_drv_mcp73833_t *chg, mp_options_t *options, char *who) {
	const char *model;
	const char *serial;

	memset(chg, 0, sizeof(*chg));
	chg->kernel = kernel;
	if(mp_device_options_identity(options, &model, &serial) != TRUE) {
		mp_printk("MCP73833: model and serial are required");
		return(FALSE);
	}
	chg->model = model;
	chg->serial = serial;

	chg->stat1 = mp_gpio_text_handle(mp_options_get(options, "stat1"), "MCP73833 STAT1");
	chg->stat2 = mp_gpio_text_handle(mp_options_get(options, "stat2"), "MCP73833 STAT2");
	if(!chg->stat1 || !chg->stat2) {
		mp_printk("MCP73833: need stat1 and stat2");
		mp_drv_mcp73833_fini(chg);
		return(FALSE);
	}
	mp_gpio_direction(chg->stat1, MP_GPIO_INPUT);
	mp_gpio_direction(chg->stat2, MP_GPIO_INPUT);

	_mcp73833_bind(chg);
	if(chg->binding.kernel != chg->kernel) {
		mp_drv_mcp73833_fini(chg);
		return(FALSE);
	}
	memset(&chg->charge, 0, sizeof(chg->charge));
	if(!mp_device_registry_register(&chg->binding, &chg->dev, MP_DEVICE_CHARGER, 0, &chg->charge, chg->model, chg->serial, NULL)) {
		mp_printk("MCP73833: can not register the charger");
		mp_drv_mcp73833_fini(chg);
		return(FALSE);
	}
	chg->dev.label = "Microchip MCP73833";
	mp_device_identify(chg, "Microchip MCP73833", chg->model, chg->serial, NULL);
	mp_printk("MCP73833: charger ready");
	(void)who;
	return(TRUE);
}

void mp_drv_mcp73833_fini(mp_drv_mcp73833_t *chg) {
	if(!chg || !chg->kernel)
		return;
	if(chg->task) {
		mp_task_destroy(chg->task);
		chg->task = NULL;
	}
	if(chg->dev.driver)
		mp_device_registry_unregister(&chg->dev);
	if(chg->binding.kernel)
		mp_device_driver_unregister(chg->kernel, &chg->binding);
	if(chg->stat1) {
		mp_gpio_release(chg->stat1);
		chg->stat1 = NULL;
	}
	if(chg->stat2) {
		mp_gpio_release(chg->stat2);
		chg->stat2 = NULL;
	}
}

#ifdef SUPPORT_COMMON_DEVICE
static mp_ret_t _mcp73833_device_open(mp_kernel_t *kernel, void *self, mp_options_t *options, char *who) {
	return(mp_drv_mcp73833_init(kernel, self, options, who));
}

static void _mcp73833_device_close(void *self) {
	mp_drv_mcp73833_fini(self);
}

const mp_device_ops_t mp_drv_mcp73833_device = {
	"MCP73833", MP_DEVICE_CHARGER, _mcp73833_device_open, _mcp73833_device_close, NULL, NULL,
	"Microchip MCP73833", "1.0.0"
};
#endif

#endif
