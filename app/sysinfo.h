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

/*
 * Shared by main.c (boot and the console report), screen.c (the display),
 * pages/ (one directory per home page, its index.c) and morse.c (the LEDs).
 */

#ifndef _MINIPHI_APP_SYSINFO_H
#define _MINIPHI_APP_SYSINFO_H

#include <mp.h>
#include <pinout.h>
#include <gen/crypto.h>

#define SYSINFO_OP MP_KERNEL_RES01

/* kept in flash when KFLASH is on; a bool, YES by default */
#define SYSINFO_KEY_ACCEL "sysinfo.accelerometers"
/* kept in flash when KFLASH is on; a bool, NO by default: the screen may go dark */
#define SYSINFO_KEY_HIBERNATE "sysinfo.hibernate"

/* Power page figures table: the governor rows plus the supply sensors */
#define SYSINFO_POWER_ROWS 12

/* the devices come from common/device: sysinfo never names a driver */
#ifdef SUPPORT_COMMON_TUI
#define SYSINFO_SCREEN
#endif

#if defined(SYSINFO_SCREEN) && defined(SUPPORT_COMMON_DEVICE)
/** bubble level: mx, my, mz are centi-g, and ±100 is the edge of the glass */
typedef struct {
	mp_tui_widget_t widget;
	int mx, my, mz;
} sysinfo_level_t;
#endif

typedef struct sysinfo_s sysinfo_t;

#ifdef SYSINFO_SCREEN
/** the picture of the camera that is open; owner is the app */
typedef struct {
	mp_tui_widget_t widget;
	sysinfo_t *owner;
} sysinfo_camera_view_t;
#endif

struct sysinfo_s {
	mp_kernel_t kernel;

	mp_serial_t serial;
	mp_bool_t console;

	mp_task_t *task;
	int line;

	/** SYSINFO_KEY_ACCEL: the accelerometers may run while it is YES */
	mp_bool_t accelEnabled;
	/** they are open: enabled, and on screen when there is one */
	mp_bool_t accelRunning;
#ifdef SUPPORT_COMMON_KFLASH
	mp_kflash_var_t accelVar;
	mp_bool_t accelListened;
	mp_kflash_var_t hibernateVar;
	mp_bool_t hibernateListened;
#endif
	/** SYSINFO_KEY_HIBERNATE: the screen saver may turn the backlight fully off */
	mp_bool_t hibernate;

	/** the first two LEDs of the board; NULL when it has fewer */
	struct {
		mp_device_t *device;
		const char *pattern;
		int step;
	} leds[2];
	mp_task_t *morse;

#ifdef SYSINFO_SCREEN
	struct {
		mp_device_t *display;
		mp_tui_t tui;
		mp_bool_t ready;
		mp_task_t *task;
#ifdef SUPPORT_COMMON_DEVICE
		mp_device_input_t keys;
		/** subscribers of the registered touch panels; sampling starts with the first */
		mp_device_subscribe_t touchSub[2];
		int touchCount;
#endif
		int systemReady;
		/** home entry System; -1 until it is registered */
		int systemIndex;
		/** sub-pages of System; -1 until they are registered */
		int tasksIndex;
		int tasksReady;
		/** per kernel task slot: runs at taskAt, and wake ups per second since the one before */
		unsigned long taskRuns[MP_TASK_MAX];
		unsigned long taskRate[MP_TASK_MAX];
		unsigned long taskAt;
		int devicesIndex;
		int devicesReady;
#ifdef SUPPORT_COMMON_USB
		int usbIndex;
		int usbReady;
#endif
		/** class page, then the algorithm; the detail says if that one is accelerated */
		int cryptoClass;
		int cryptoDetail;
		int cryptoPick;
		const char *cryptoFamily;
#ifdef SUPPORT_COMMON_CRYPTO_RNG
		/** RNG class page: the generator, a fresh number every 500 ms, then the algorithms */
		int rngShown;
		unsigned long rngLast;
#endif
		/** home entry "Power"; -1 until it is registered */
		int powerIndex;
		int powerReady;
		int powerSensorRows;
#ifdef SUPPORT_COMMON_DEVICE
		/** home entry Devices, then the category, then the device; -1 until registered */
		int devIndex;
		int devList;
		int devView;
		int devClass;
		int devRank;
		char devTitle[32];
		/** held while the clock view is on screen; that subscription runs the read */
		mp_device_subscribe_t clockSub;
		/** accelerometer view: the bubble, the axes, the companion temperature, the switch */
		int accelReady;
		/** camera view */
		unsigned long cameraSequence;
#endif
		/**
		 * One page is built at a time, and Back builds the previous one again.
		 * The arms share their memory. A refresh runs only while its page is the
		 * one on screen, so it never writes an arm that a later page has reused.
		 */
		union {
			/** System */
			struct {
				mp_tui_widget_t logo;
				mp_tui_label_t heading;
				mp_tui_label_t copyright;
				mp_tui_label_t license;
				mp_tui_window_t system;
				mp_tui_window_t status;
				mp_tui_label_t board;
				mp_tui_label_t cpu;
				mp_tui_label_t clock;
				mp_tui_label_t memory;
				mp_tui_window_t flash;
				mp_tui_label_t flashSize;
				mp_tui_label_t flashUsed;
				mp_tui_bar_t flashBar;
				mp_tui_label_t flashFree;
				mp_tui_label_t flashState;
#ifdef SUPPORT_COMMON_KFLASH
				mp_tui_label_t flashStore;
#endif
				mp_tui_label_t build;
				mp_tui_label_t target;
				mp_tui_label_t uptime;
				mp_tui_label_t tasks;
				mp_tui_bar_t taskBar;
				mp_tui_label_t ram;
				mp_tui_bar_t ramBar;
#ifdef SUPPORT_COMMON_MEM
				mp_tui_label_t pool;
				mp_tui_bar_t poolBar;
#endif
				mp_tui_button_t taskManager;
				mp_tui_button_t devicesButton;
#ifdef SUPPORT_COMMON_USB
				mp_tui_button_t usbButton;
#endif
				/** die temperature and supply */
				mp_tui_gauge_t sensorGauge[2];
			};
			/** Task manager */
			struct {
				mp_tui_label_t taskCount;
				mp_tui_table_t taskTable;
				mp_tui_cell_t taskCell[MP_TASK_MAX][4];
			};
			/** Drivers */
			struct {
				mp_tui_label_t deviceCount;
				mp_tui_table_t deviceTable;
				mp_tui_cell_t deviceCell[MP_DEVICE_MAX][3];
			};
#ifdef SUPPORT_COMMON_USB
			/** USB */
			struct {
				mp_tui_label_t usbCount;
				mp_tui_table_t usbTable;
				mp_tui_cell_t usbCell[MP_USB_PORT_MAX][4];
			};
#endif
			/**
			 * Buttons of whichever list is on screen. Cryptography and Devices
			 * are never built together: opening one clears the page and fills these again.
			 * The RNG lines sit on the cryptography class page, with the buttons.
			 */
			struct {
				union {
					mp_tui_button_t crypto[MP_CRYPTO_MAX];
					mp_tui_button_t device[MP_CRYPTO_MAX];
				} menu;
#ifdef SUPPORT_COMMON_CRYPTO_RNG
				mp_tui_label_t rngGenerator;
				mp_tui_label_t rngValue;
				mp_tui_label_t rngHeading;
#endif
			};
			/** one algorithm */
			struct {
				mp_tui_label_t cryptoState;
#ifdef SUPPORT_COMMON_CRYPTO_MAC_HMAC
				/** HMAC and HMAC-DRBG: the hashes they run over, and whether the chip accelerates them */
				mp_tui_table_t cryptoTable;
				mp_tui_cell_t cryptoCell[MP_CRYPTO_MAX][3];
#endif
#ifdef SUPPORT_COMMON_CRYPTO_SIGN
				/** a signature: key and signature sizes, then the self-test */
				mp_tui_label_t signInfo;
				mp_tui_label_t signResult;
#endif
			};
			/** Power */
			struct {
				mp_tui_switch_t powerSwitch;
				mp_tui_switch_t powerHibernate;
				mp_tui_label_t powerLevel;
				mp_tui_label_t powerGovernor;
				/** figures, then the supply sensors found when the page was built */
				mp_tui_table_t powerTable;
				mp_tui_cell_t powerCell[SYSINFO_POWER_ROWS][2];
				mp_tui_label_t powerHolds;
				mp_tui_table_t powerLevels;
				mp_tui_cell_t powerLevelCell[MP_POWER_LEVELS][4];
			};
#ifdef SUPPORT_COMMON_DEVICE
			/** one device: bubble, dial, or the camera picture */
			struct {
				mp_tui_columns_t accelLayout;
				sysinfo_level_t level;
				mp_tui_label_t accelName;
				mp_tui_label_t accelX;
				mp_tui_label_t accelY;
				mp_tui_label_t accelZ;
				mp_tui_label_t accelTemperature;
				mp_tui_switch_t accelSwitch;
				/** temperature or voltage dial, or the accelerometer thermometer */
				mp_tui_gauge_t measureGauge;
				mp_tui_label_t cameraState;
				sysinfo_camera_view_t cameraPicture;
			};
#endif
		};
	} screen;
#endif
};

/* delays are milliseconds and follow mp_clock_tick_hz() */
static inline unsigned long _sysinfo_ticks(unsigned long ms) {
	unsigned long hz = mp_clock_tick_hz();
	unsigned long ticks;

	if(hz == 0 || ms == 0)
		return(1);

	ticks = (ms / 1000) * hz + ((ms % 1000) * hz) / 1000;
	return(ticks ? ticks : 1);
}

void _sysinfo_morse_start(sysinfo_t *sysinfo);
void _sysinfo_morse_stop(sysinfo_t *sysinfo);
void _sysinfo_screen_open(sysinfo_t *sysinfo);
void _sysinfo_screen_close(sysinfo_t *sysinfo);

/** stores SYSINFO_KEY_ACCEL; the accelerometers follow when the store tells sysinfo */
void _sysinfo_accel_enable(sysinfo_t *sysinfo, mp_bool_t on);
/** stores SYSINFO_KEY_HIBERNATE; the saver follows when the store tells sysinfo */
void _sysinfo_hibernate_enable(sysinfo_t *sysinfo, mp_bool_t on);
/** sleeps or wakes the accelerometers after accelEnabled or the screen changed */
void _sysinfo_accel_apply(sysinfo_t *sysinfo);
/** YES without a screen; with one, while the Accelerometer page is on and lit */
mp_bool_t _sysinfo_accel_shown(sysinfo_t *sysinfo);

#endif
