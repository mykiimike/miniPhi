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

#ifdef SUPPORT_COMMON_USB

/**
@defgroup mpCommonUsbCore USB port list

@ingroup mpCommonUsb

@{
*/

static mp_kernel_t *_mp_usb_kernel;
static mp_task_t *_mp_usb_task;

static int _mp_usb_allowed(mp_usb_port_t *port, mp_usb_role_t role) {
	if(role == MP_USB_ROLE_DEVICE)
		return((port->caps & MP_USB_CAP_DEVICE) != 0);
	if(role == MP_USB_ROLE_HOST)
		return((port->caps & MP_USB_CAP_HOST) != 0);
	return(0);
}

static void _mp_usb_flush(mp_usb_port_t *port) {
	MP_INTERRUPT_SAFE_BEGIN
	port->qCount = 0;
	MP_INTERRUPT_SAFE_END
}

static void _mp_usb_apply(mp_usb_port_t *port, mp_usb_event_t event, mp_usb_speed_t speed) {
	mp_usb_link_t link = port->link;
	mp_usb_speed_t was = port->speed;
	mp_bool_t fault = port->fault;

	/* a role change flushed these; one that raced the mask is ignored */
	if(port->role == MP_USB_ROLE_NONE)
		return;

	switch(event) {
		case MP_USB_EVENT_CONNECT:
			port->link = MP_USB_LINK_CONNECTED;
			port->speed = speed;
			port->fault = NO;
			break;
		case MP_USB_EVENT_DISCONNECT:
			port->link = MP_USB_LINK_DISCONNECTED;
			port->speed = MP_USB_SPEED_NONE;
			port->fault = NO;
			break;
		case MP_USB_EVENT_RESET:
			port->link = MP_USB_LINK_RESET;
			port->speed = speed;
			break;
		case MP_USB_EVENT_SUSPEND:
			if(port->link == MP_USB_LINK_CONNECTED || port->link == MP_USB_LINK_RESET)
				port->link = MP_USB_LINK_SUSPENDED;
			break;
		case MP_USB_EVENT_RESUME:
			if(port->link == MP_USB_LINK_SUSPENDED)
				port->link = MP_USB_LINK_CONNECTED;
			break;
		case MP_USB_EVENT_FAULT:
			port->fault = YES;
			break;
	}

	if(port->link == link && port->speed == was && port->fault == fault)
		return;
	mp_printk("usb %s %s%s%s", port->name,
		port->fault == YES ? "fault" : mp_usb_link_name(port->link),
		port->speed == MP_USB_SPEED_NONE || port->fault == YES ? "" : ", ",
		port->speed == MP_USB_SPEED_NONE || port->fault == YES ? "" : mp_usb_speed_name(port->speed));
	if(port->onLink)
		port->onLink(port);
}

static unsigned long _mp_usb_ms(unsigned long ms) {
	unsigned long hz = mp_clock_tick_hz();
	unsigned long ticks;

	if(hz == 0 || ms == 0)
		return(1);
	ticks = (ms / 1000) * hz + ((ms % 1000) * hz) / 1000;
	return(ticks ? ticks : 1);
}

static void _mp_usb_drain(void) {
	mp_list_item_t *item;

	for(item = _mp_usb_kernel->usbs.first; item != NULL; item = item->next) {
		mp_usb_port_t *port = item->user;

		while(port->qCount) {
			unsigned char at;
			mp_usb_event_t event;
			mp_usb_speed_t speed;

			MP_INTERRUPT_SAFE_BEGIN
			at = port->qHead;
			event = port->queue[at].event;
			speed = port->queue[at].speed;
			port->qHead = (unsigned char)((port->qHead + 1) % MP_USB_EVENTS);
			port->qCount--;
			MP_INTERRUPT_SAFE_END
			_mp_usb_apply(port, event, speed);
		}
	}
}

/* the host is on the line: keep sampling so its departure is seen */
static int _mp_usb_hot(mp_usb_port_t *port) {
	if(port->ops->poll == NULL || port->connect != YES)
		return(0);
	return(port->link == MP_USB_LINK_CONNECTED || port->link == MP_USB_LINK_RESET);
}

static MP_TASK(_mp_usb_task_run) {
	mp_list_item_t *item;
	int watch = 0;

	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	if(_mp_usb_kernel) {
		_mp_usb_drain();
		for(item = _mp_usb_kernel->usbs.first; item != NULL; item = item->next) {
			mp_usb_port_t *port = item->user;

			if(_mp_usb_hot(port))
				port->ops->poll(port);
		}
		_mp_usb_drain();
		for(item = _mp_usb_kernel->usbs.first; item != NULL; item = item->next)
			if(_mp_usb_hot(item->user))
				watch = 1;
	}

	/* 20 ms: the F103 counts several SE0 samples before it calls the cable gone */
	if(watch)
		mp_task_delay(task, _mp_usb_ms(20));
	else
		mp_task_signal(task, MP_TASK_SIG_SLEEP);
}

static void _mp_usb_idle(mp_usb_port_t *port) {
	port->connect = NO;
	port->fault = NO;
	port->speed = MP_USB_SPEED_NONE;
	port->link = port->role == MP_USB_ROLE_NONE ? MP_USB_LINK_OFF : MP_USB_LINK_DISCONNECTED;
}

/* a lock is the role; the pull-up lets a host reset show up on the link */
static void _mp_usb_arm_locked(void) {
	mp_list_item_t *item;

	for(item = _mp_usb_kernel->usbs.first; item != NULL; item = item->next) {
		mp_usb_port_t *port = item->user;

		if(port->fixed == MP_USB_ROLE_NONE)
			continue;
		if(mp_usb_set_role(port, port->fixed) != TRUE)
			continue;
		if(port->fixed == MP_USB_ROLE_DEVICE)
			mp_usb_connect(port, YES);
	}
}

void mp_usb_start(mp_kernel_t *kernel) {
	if(!kernel || _mp_usb_task)
		return;

	_mp_usb_kernel = kernel;
	mp_list_init(&kernel->usbs);
	mp_usb_arch_register(kernel);
	if(mp_usb_count() == 0)
		return;

	_mp_usb_task = mp_task_create(&kernel->tasks, "USB", _mp_usb_task_run, NULL, 1);
	if(!_mp_usb_task) {
		mp_printk("USB: no task slot");
		return;
	}
	mp_task_signal(_mp_usb_task, MP_TASK_SIG_SLEEP);
	_mp_usb_arm_locked();
}

void mp_usb_stop(void) {
	mp_list_item_t *item;

	if(!_mp_usb_kernel)
		return;

	for(item = _mp_usb_kernel->usbs.first; item != NULL; item = item->next)
		mp_usb_set_role(item->user, MP_USB_ROLE_NONE);

	if(_mp_usb_task) {
		mp_task_destroy(_mp_usb_task);
		_mp_usb_task = NULL;
	}
	_mp_usb_kernel = NULL;
}

mp_ret_t mp_usb_port_add(mp_kernel_t *kernel, mp_usb_port_t *port, char *name,
	mp_usb_caps_t caps, mp_usb_role_t fixed, const mp_usb_ops_t *ops, void *hw) {

	if(!kernel || !port || !name || !ops)
		return(FALSE);
	if(port->kernel)
		return(TRUE);
	if((caps & (MP_USB_CAP_DEVICE | MP_USB_CAP_HOST)) == 0)
		return(FALSE);
	if(fixed == MP_USB_ROLE_DEVICE && (caps & MP_USB_CAP_DEVICE) == 0)
		return(FALSE);
	if(fixed == MP_USB_ROLE_HOST && (caps & MP_USB_CAP_HOST) == 0)
		return(FALSE);
	if(fixed != MP_USB_ROLE_NONE && fixed != MP_USB_ROLE_DEVICE && fixed != MP_USB_ROLE_HOST)
		return(FALSE);
	if(mp_usb_count() >= MP_USB_PORT_MAX)
		return(FALSE);

	memset(port, 0, sizeof(*port));
	port->kernel = kernel;
	port->name = name;
	port->ops = ops;
	port->hw = hw;
	port->caps = caps;
	port->fixed = fixed;
	port->role = MP_USB_ROLE_NONE;
	port->link = MP_USB_LINK_OFF;
	mp_list_add_last(&kernel->usbs, &port->item, port);
	return(TRUE);
}

int mp_usb_count(void) {
	mp_list_item_t *item;
	int n = 0;

	if(!_mp_usb_kernel)
		return(0);
	for(item = _mp_usb_kernel->usbs.first; item != NULL; item = item->next)
		n++;
	return(n);
}

mp_usb_port_t *mp_usb_at(int index) {
	mp_list_item_t *item;
	int n = 0;

	if(!_mp_usb_kernel || index < 0)
		return(NULL);
	for(item = _mp_usb_kernel->usbs.first; item != NULL; item = item->next, n++)
		if(n == index)
			return(item->user);
	return(NULL);
}

mp_usb_port_t *mp_usb_named(const char *name) {
	mp_list_item_t *item;

	if(!_mp_usb_kernel || !name)
		return(NULL);
	for(item = _mp_usb_kernel->usbs.first; item != NULL; item = item->next) {
		mp_usb_port_t *port = item->user;

		if(port->name && strcmp(port->name, name) == 0)
			return(port);
	}
	return(NULL);
}

const char *mp_usb_role_name(mp_usb_role_t role) {
	switch(role) {
		case MP_USB_ROLE_DEVICE: return("device");
		case MP_USB_ROLE_HOST: return("host");
		default: return("off");
	}
}

const char *mp_usb_lock_name(mp_usb_role_t fixed) {
	switch(fixed) {
		case MP_USB_ROLE_DEVICE: return("device");
		case MP_USB_ROLE_HOST: return("host");
		default: return("free");
	}
}

const char *mp_usb_speed_name(mp_usb_speed_t speed) {
	switch(speed) {
		case MP_USB_SPEED_LOW: return("low");
		case MP_USB_SPEED_FULL: return("full");
		case MP_USB_SPEED_HIGH: return("high");
		default: return("none");
	}
}

const char *mp_usb_link_name(mp_usb_link_t link) {
	switch(link) {
		case MP_USB_LINK_DISCONNECTED: return("idle");
		case MP_USB_LINK_CONNECTED: return("connected");
		case MP_USB_LINK_RESET: return("reset");
		case MP_USB_LINK_SUSPENDED: return("suspended");
		default: return("off");
	}
}

mp_ret_t mp_usb_lock(mp_usb_port_t *port, mp_usb_role_t role) {
	if(!port || _mp_usb_allowed(port, role) == 0)
		return(FALSE);
	if(port->fixed != MP_USB_ROLE_NONE && port->fixed != role)
		return(FALSE);
	if(port->role != MP_USB_ROLE_NONE && port->role != role)
		return(FALSE);
	port->fixed = role;
	return(TRUE);
}

mp_ret_t mp_usb_set_role(mp_usb_port_t *port, mp_usb_role_t role) {
	if(!port || !port->ops || !port->ops->set_role)
		return(FALSE);
	if(role != MP_USB_ROLE_NONE && role != MP_USB_ROLE_DEVICE && role != MP_USB_ROLE_HOST)
		return(FALSE);
	if(port->fixed != MP_USB_ROLE_NONE && role != MP_USB_ROLE_NONE && role != port->fixed)
		return(FALSE);
	if(role != MP_USB_ROLE_NONE && _mp_usb_allowed(port, role) == 0)
		return(FALSE);
	if(role == port->role)
		return(TRUE);

	_mp_usb_flush(port);
	if(port->ops->set_role(port, role) != TRUE)
		return(FALSE);

	port->role = role;
	_mp_usb_idle(port);
	return(TRUE);
}

mp_ret_t mp_usb_connect(mp_usb_port_t *port, mp_bool_t on) {
	if(!port || port->role == MP_USB_ROLE_NONE || !port->ops || !port->ops->connect)
		return(FALSE);

	on = on ? YES : NO;
	if(port->connect == on)
		return(TRUE);
	if(port->ops->connect(port, on) != TRUE)
		return(FALSE);

	port->connect = on;
	if(on == NO) {
		port->fault = NO;
		port->speed = MP_USB_SPEED_NONE;
		port->link = MP_USB_LINK_DISCONNECTED;
	}
	return(TRUE);
}

void mp_usb_on_link(mp_usb_port_t *port, mp_usb_on_link_t onLink, void *user) {
	if(!port)
		return;
	port->onLink = onLink;
	port->user = user;
}

void mp_usb_event(mp_usb_port_t *port, mp_usb_event_t event, mp_usb_speed_t speed) {
	unsigned char at;

	if(!port || !_mp_usb_task)
		return;

	MP_INTERRUPT_SAFE_BEGIN
	if(port->qCount == MP_USB_EVENTS) {
		port->qHead = (unsigned char)((port->qHead + 1) % MP_USB_EVENTS);
		port->qCount--;
	}
	at = (unsigned char)((port->qHead + port->qCount) % MP_USB_EVENTS);
	port->queue[at].event = event;
	port->queue[at].speed = speed;
	port->qCount++;
	MP_INTERRUPT_SAFE_END

	mp_task_wake(_mp_usb_task);
}

static void _mp_usb_park(mp_usb_port_t *port) {
	if(port->ops->clock_stop)
		port->ops->clock_stop(port);
	/* a stale reset must not mark the link up after the PHY was stopped */
	_mp_usb_flush(port);
	port->speed = MP_USB_SPEED_NONE;
	port->fault = NO;
	if(port->link != MP_USB_LINK_OFF)
		port->link = MP_USB_LINK_DISCONNECTED;
}

static void _mp_usb_unpark(mp_usb_port_t *port) {
	if(port->ops->clock_start)
		port->ops->clock_start(port);
}

void mp_usb_clock_stop(void) {
	mp_list_item_t *item;

	if(!_mp_usb_kernel)
		return;
	for(item = _mp_usb_kernel->usbs.first; item != NULL; item = item->next) {
		mp_usb_port_t *port = item->user;

		if(port->role != MP_USB_ROLE_NONE)
			_mp_usb_park(port);
	}
}

void mp_usb_clock_start(void) {
	mp_list_item_t *item;

	if(!_mp_usb_kernel)
		return;
	for(item = _mp_usb_kernel->usbs.first; item != NULL; item = item->next) {
		mp_usb_port_t *port = item->user;

		if(port->role != MP_USB_ROLE_NONE)
			_mp_usb_unpark(port);
	}
}

/**@}*/

#endif
