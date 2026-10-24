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

#ifdef SUPPORT_COMMON_DEVICE
#ifndef _HAVE_MP_COMMON_DEVICE_H
	#define _HAVE_MP_COMMON_DEVICE_H

	/**
	 * The board devices behind one interface, so an app never names a driver.
	 * A driver takes part with an mp_device_driver_t and a device entry in its
	 * _phi.js. The configurator lists the board devices of those drivers, with
	 * their storage and their options, in include/gen/devices.h. An app finds
	 * one by class and rank, opens it, and talks to its face: the common
	 * interface of the class.
	 */

	/**
	 * One frame from an MP_DEVICE_CAMERA.
	 * pixels stays put for the life of the device. Each byte is the 14-bit sample
	 * stretched across the range of the previous frame.
	 * sequence is even when pixels is a complete frame and odd while the
	 * driver is writing it. Read sequence, then pixels, then sequence again,
	 * and keep the copy only when both reads match and are even.
	 */
	typedef struct {
		const unsigned char *pixels;
		unsigned short width;
		unsigned short height;
		unsigned long sequence;
	} mp_camera_frame_t;

	typedef enum {
		/** face: mp_tui_display_t */
		MP_DEVICE_DISPLAY,
		/** face: mp_input_t, already listed on the kernel while open */
		MP_DEVICE_TOUCH,
		/** publishes MP_SENSOR_ACCELEROMETER sensors while open; set(0) sleeps the chip */
		MP_DEVICE_ACCELEROMETER,
		/** an output: mp_device_set() */
		MP_DEVICE_LED,
		/** face: mp_camera_frame_t */
		MP_DEVICE_CAMERA,
	} mp_device_class_t;

	typedef struct mp_device_driver_s mp_device_driver_t;
	typedef struct mp_device_s mp_device_t;

	struct mp_device_driver_s {
		const char *name;
		mp_device_class_t type;
		mp_ret_t (*open)(mp_kernel_t *kernel, void *self, mp_options_t *options, char *who);
		void (*close)(void *self);
		/** optional: the class interface, see mp_device_class_t */
		void *(*face)(void *self);
		/** optional: 0 is off, anything else on */
		void (*set)(void *self, int value);
	};

	struct mp_device_s {
		/** the name on the board, also the owner of its pins */
		char *name;
		const mp_device_driver_t *driver;
		void *self;
		mp_options_t *options;
		mp_bool_t opened;
		/**
		 * Identity read from the chip. NULL until the driver registers it.
		 * The driver keeps the text; these point at it.
		 */
		const char *model;
		const char *serial;
		const char *version;
	};

	#ifndef MP_DEVICE_MAX
		/** devices one board can list; the table in include/gen/devices.h is checked against it */
		#define MP_DEVICE_MAX 8
	#endif

	int mp_device_count(void);
	mp_device_t *mp_device_at(int index);
	/** rank counts the devices of that class in board order, from 0 */
	mp_device_t *mp_device_find(mp_device_class_t type, int rank);
	mp_device_t *mp_device_named(const char *name);

	/** TRUE when it is open, also when it already was */
	mp_ret_t mp_device_open(mp_kernel_t *kernel, mp_device_t *device);
	void mp_device_close(mp_device_t *device);
	/** NULL while closed or when the class has no face */
	void *mp_device_face(mp_device_t *device);
	void mp_device_set(mp_device_t *device, int value);

	/**
	 * Register a model, a serial number and a software version read from the chip.
	 * self is the driver object passed to open. The strings are not copied, so
	 * they must stay valid after this returns. NULL or "" leaves that field unknown.
	 * A driver calls this again as it learns each one.
	 */
	void mp_device_identify(void *self, const char *model, const char *serial, const char *version);

#endif
#endif
