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

#ifdef SUPPORT_COMMON_BUTTON

/**
 * A push button on one GPIO pin. The board names the pin and the polarity.
 * The interrupt follows the pin: after a level is seen, the next edge is the
 * other one. The task reports that level through the button contract.
 */

static MP_TASK(_mp_button_task);

static void _mp_button_pin_release(mp_button_t *button) {
	if(!button->pin)
		return;
	mp_gpio_release(button->pin);
	button->pin = NULL;
}

/* the next edge is the one that leaves the level just read */
static void _mp_button_arm(mp_button_t *button, int level) {
	if(level)
		mp_gpio_interrupt_hi2lo(button->pin);
	else
		mp_gpio_interrupt_lo2hi(button->pin);
}

static void _mp_button_irq(void *user) {
	mp_button_t *button = user;
	int level = mp_gpio_read(button->pin) ? 1 : 0;

	button->level = level;
	_mp_button_arm(button, level);
	if(button->task)
		mp_task_wake(button->task);
}

static MP_TASK(_mp_button_task) {
	mp_button_t *button = task->user;

	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	mp_device_button_level(&button->face, button->level);
	mp_task_signal(task, MP_TASK_SIG_SLEEP);
}

static mp_ret_t _mp_button_load(mp_kernel_t *kernel, mp_device_driver_t *driver, void *self, mp_options_t *options) {
	(void)kernel;
	(void)driver;
	(void)self;
	(void)options;
	return(TRUE);
}

static void _mp_button_unload(void *self) {
	_mp_button_pin_release(self);
}

static void _mp_button_disarm(mp_button_t *button) {
	if(button->irq == NO)
		return;
	mp_gpio_interrupt_disable(button->pin);
	mp_gpio_interrupt_unset(button->pin);
	button->irq = NO;
}

static void _mp_button_start(void *self) {
	mp_button_t *button = self;
	int level;

	mp_gpio_direction(button->pin, MP_GPIO_INPUT);
	/* Active low idles high. The direction call leaves the pad floating. */
	if(button->face.active == 0)
		mp_gpio_pull_up(button->pin);
	if(mp_gpio_interrupt_set(button->pin, _mp_button_irq, button, "Button") == FALSE)
		return;

	level = mp_gpio_read(button->pin) ? 1 : 0;
	button->level = level;
	mp_device_button_level(&button->face, level);
	_mp_button_arm(button, level);

	button->task = mp_task_create(&button->kernel->tasks, "Button", _mp_button_task, button, 1000);
	if(!button->task) {
		mp_gpio_interrupt_unset(button->pin);
		return;
	}
	mp_task_signal(button->task, MP_TASK_SIG_SLEEP);
	mp_gpio_interrupt_enable(button->pin);
	button->irq = YES;
}

static void _mp_button_stop(void *self) {
	mp_button_t *button = self;

	_mp_button_disarm(button);
	if(button->task) {
		mp_task_destroy(button->task);
		button->task = NULL;
	}
}

mp_ret_t mp_button_init(mp_kernel_t *kernel, mp_button_t *button) {
	static const mp_device_driver_ops_t ops = {
		"button/gpio", "GPIO button", "1.0.0",
		(unsigned int)(1u << MP_DEVICE_BUTTON),
		_mp_button_load, _mp_button_unload, NULL,
		_mp_button_start, _mp_button_stop
	};

	if(!kernel || !button)
		return(FALSE);
	button->kernel = kernel;
	if(!button->pin)
		goto fail;
	if(!button->model || !button->model[0] || !button->serial || !button->serial[0])
		goto fail;

	if(mp_device_driver_register(kernel, &button->binding, &ops, button, NULL) == FALSE)
		goto fail;
	if(!mp_device_registry_register(&button->binding, &button->device, MP_DEVICE_BUTTON, 0, &button->face, button->model, button->serial, NULL)) {
		mp_device_driver_unregister(kernel, &button->binding);
		return(FALSE);
	}
	button->device.label = "GPIO button";

	mp_printk("Button %s.%s on port %u pin %u, active %s",
		button->model, button->serial,
		button->pin->port, button->pin->pin,
		button->face.active ? "high" : "low");
	return(TRUE);

fail:
	_mp_button_pin_release(button);
	return(FALSE);
}

void mp_button_fini(mp_button_t *button) {
	if(!button || !button->kernel)
		return;
	mp_device_registry_unregister(&button->device);
	mp_device_driver_unregister(button->kernel, &button->binding);
	button->kernel = NULL;
}

#endif
