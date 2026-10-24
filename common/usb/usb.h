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

#ifndef _HAVE_MP_COMMON_USB_H
	#define _HAVE_MP_COMMON_USB_H

	#ifdef SUPPORT_COMMON_USB

	/**
	 * @defgroup mpCommonUsb USB ports
	 * @ingroup mpCommon
	 *
	 * PHY ports of the chip, listed at boot before any role is chosen.
	 * caps is what the silicon can do. fixed, when it is not
	 * MP_USB_ROLE_NONE, is the only role the port accepts: the block or
	 * the connector forces device or host. role is what it runs now, and
	 * it stays MP_USB_ROLE_NONE until mp_usb_set_role(). A locked port
	 * can still be off. No endpoints and no classes here.
	 * @{
	 */

	#ifndef MP_USB_PORT_MAX
		#define MP_USB_PORT_MAX 4
	#endif

	#ifndef MP_USB_EVENTS
		#define MP_USB_EVENTS 8
	#endif

	enum {
		MP_USB_CAP_DEVICE = 0x01,
		MP_USB_CAP_HOST   = 0x02,
	};
	typedef unsigned char mp_usb_caps_t;

	typedef enum {
		/** PHY off, or a port that is not locked to one role */
		MP_USB_ROLE_NONE = 0,
		MP_USB_ROLE_DEVICE,
		MP_USB_ROLE_HOST,
	} mp_usb_role_t;

	typedef enum {
		MP_USB_SPEED_NONE = 0,
		MP_USB_SPEED_LOW,
		MP_USB_SPEED_FULL,
		MP_USB_SPEED_HIGH,
	} mp_usb_speed_t;

	/** electrical state; addressed and configured belong to the stack */
	typedef enum {
		MP_USB_LINK_OFF = 0,
		MP_USB_LINK_DISCONNECTED,
		MP_USB_LINK_CONNECTED,
		MP_USB_LINK_RESET,
		MP_USB_LINK_SUSPENDED,
	} mp_usb_link_t;

	typedef enum {
		MP_USB_EVENT_CONNECT,
		MP_USB_EVENT_DISCONNECT,
		MP_USB_EVENT_RESET,
		MP_USB_EVENT_SUSPEND,
		MP_USB_EVENT_RESUME,
		MP_USB_EVENT_FAULT,
	} mp_usb_event_t;

	typedef struct mp_usb_port_s mp_usb_port_t;
	typedef struct mp_usb_ops_s mp_usb_ops_t;

	typedef void (*mp_usb_on_link_t)(mp_usb_port_t *port);

	struct mp_usb_ops_s {
		/** MP_USB_ROLE_NONE turns the PHY off and releases its pins */
		mp_ret_t (*set_role)(mp_usb_port_t *port, mp_usb_role_t role);
		/**
		 * Device: pull-up, once VBUS is assumed present.
		 * Host: port power. FALSE while the PHY is off.
		 */
		mp_ret_t (*connect)(mp_usb_port_t *port, mp_bool_t on);
		/**
		 * Called from the USB task while the link is up.
		 * NULL when removal is reported from the interrupt alone.
		 */
		void (*poll)(mp_usb_port_t *port);
	};

	struct mp_usb_port_s {
		mp_list_item_t item;
		mp_kernel_t *kernel;
		char *name;
		const mp_usb_ops_t *ops;
		void *hw;

		mp_usb_caps_t caps;
		/** MP_USB_ROLE_NONE when device and host are both still allowed */
		mp_usb_role_t fixed;
		mp_usb_role_t role;
		mp_usb_speed_t speed;
		mp_usb_link_t link;
		/** YES after a successful mp_usb_connect() */
		mp_bool_t connect;
		mp_bool_t fault;

		mp_usb_on_link_t onLink;
		void *user;

		struct {
			mp_usb_event_t event;
			mp_usb_speed_t speed;
		} queue[MP_USB_EVENTS];
		volatile unsigned char qHead;
		volatile unsigned char qCount;
	};

	/** @} */

	void mp_usb_start(mp_kernel_t *kernel);
	void mp_usb_stop(void);

	/**
	 * Published by the arch for each PHY the chip has. The port starts
	 * off. fixed is DEVICE or HOST when that is the only legal role.
	 * mp_usb_start() then arms a lock, and raises the pull-up of a device lock.
	 */
	mp_ret_t mp_usb_port_add(mp_kernel_t *kernel, mp_usb_port_t *port, char *name,
		mp_usb_caps_t caps, mp_usb_role_t fixed, const mp_usb_ops_t *ops, void *hw);

	int mp_usb_count(void);
	mp_usb_port_t *mp_usb_at(int index);
	mp_usb_port_t *mp_usb_named(const char *name);

	const char *mp_usb_role_name(mp_usb_role_t role);
	/** "free" when the port is not locked, otherwise the forced role */
	const char *mp_usb_lock_name(mp_usb_role_t fixed);
	const char *mp_usb_speed_name(mp_usb_speed_t speed);
	const char *mp_usb_link_name(mp_usb_link_t link);

	/**
	 * Lock a free port to one role. A port already locked the other way
	 * refuses, and so does a port currently running the other role.
	 * There is no unlock: a silicon lock stays.
	 */
	mp_ret_t mp_usb_lock(mp_usb_port_t *port, mp_usb_role_t role);

	/**
	 * Arm the PHY in that role, or MP_USB_ROLE_NONE to turn it off.
	 * FALSE when the role is not in caps, when it breaks a lock, or
	 * when the arch refuses (no 48 MHz clock, pins taken). While any
	 * port is armed the clock tree does not change.
	 */
	mp_ret_t mp_usb_set_role(mp_usb_port_t *port, mp_usb_role_t role);
	mp_ret_t mp_usb_connect(mp_usb_port_t *port, mp_bool_t on);

	void mp_usb_on_link(mp_usb_port_t *port, mp_usb_on_link_t onLink, void *user);

	/**
	 * Queue a link event from the controller interrupt. The USB task
	 * applies it. Interrupt safe. Oldest event is dropped when the
	 * queue is full.
	 */
	void mp_usb_event(mp_usb_port_t *port, mp_usb_event_t event, mp_usb_speed_t speed);

	/** YES while a port is armed: the arch keeps the current clock */
	mp_bool_t mp_usb_needs_clock(void);

	/** the selected arch lists its PHYs, or none */
	void mp_usb_arch_register(mp_kernel_t *kernel);

	#endif

#endif
