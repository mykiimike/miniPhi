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

#ifndef _HAVE_MP_DRV_VENUS638_H
	#define _HAVE_MP_DRV_VENUS638_H

	/**
	 * SkyTraq Venus638FLPx GPS receiver on a UART.
	 *
	 * The module streams NMEA. GGA and RMC build the fix, GSA names the
	 * satellites it uses, GSV the ones in view. Commands are SkyTraq
	 * binary frames: A0 A1, length, payload, XOR, 0D 0A. The module answers
	 * each one with an ACK (0x83) or a NACK (0x84).
	 *
	 * It registers one MP_DEVICE_GNSS and one MP_DEVICE_CLOCK once a valid
	 * sentence was heard. The receive interrupt fills a ring and wakes the
	 * task on each line feed. While the link listens, deep sleep is vetoed:
	 * Stop gates the UART clock and the bytes are lost.
	 *
	 * options: model, serial, gate, txd, rxd, baudRate (power-up rate).
	 */

	#ifndef MP_DRV_VENUS638_RING
		#define MP_DRV_VENUS638_RING 256
	#endif

	/* a GSV sentence with four satellites is the longest line kept */
	#define MP_DRV_VENUS638_LINE 100
	/* restart is 21 bytes; version and the rest stay under that */
	#define MP_DRV_VENUS638_FRAME 24

	typedef struct mp_drv_venus638_s mp_drv_venus638_t;

	struct mp_drv_venus638_s {
		mp_kernel_t *kernel;
		mp_uart_t uart;
		mp_bool_t uartOpen;
		mp_task_t *task;
		mp_power_hold_t power;

		mp_device_driver_t binding;
		mp_device_registry_t gnssDev;
		mp_device_registry_t clockDev;
		mp_device_gnss_t gnss;
		mp_device_time_t clock;

		const char *model;
		const char *serial;
		char version[32];

		/* filled by the receive interrupt */
		volatile unsigned char head;
		volatile unsigned char tail;
		volatile unsigned short dropped;
		unsigned char ring[MP_DRV_VENUS638_RING];

		/* sent by the transmit interrupt */
		unsigned char tx[MP_DRV_VENUS638_FRAME + 7];
		volatile unsigned char txAt;
		unsigned char txLen;

		/* parser */
		unsigned char state;
		unsigned short frameLen;
		unsigned short frameAt;
		unsigned char frameCs;
		unsigned char frame[MP_DRV_VENUS638_FRAME];
		unsigned char lineLen;
		char line[MP_DRV_VENUS638_LINE];

		/* the fix being assembled from GGA and RMC */
		mp_device_gnss_t fix;
		unsigned long ggaTime;

		/* GSV arrives in several sentences; committed into fix when the set ends */
		unsigned char skyCount;
		unsigned char skyGot;
		unsigned char skyTotal;
		unsigned char usedCount;
		unsigned char usedId[12];
		mp_device_gnss_sat_t sky[MP_DEVICE_GNSS_SAT_MAX];

		/* link */
		unsigned long baud;
		unsigned long boardBaud;
		unsigned char probe;
		mp_bool_t heard;
		mp_bool_t listening;
		mp_bool_t running;
		unsigned long heardAt;
		unsigned long probeUntil;

		/* one command in flight */
		unsigned char boot;
		unsigned long sendAt;
		mp_bool_t waiting;
		unsigned char waitId;
		unsigned long waitUntil;
		unsigned char kind;
		int arg;
		unsigned char step;
		mp_bool_t cmdPending;
		unsigned char cmdKind;
		int cmdArg;

		/* last confirmed values; *Set is YES once known */
		unsigned char rate, pps, nav;
		mp_bool_t sbas, pinning, powerSave;
		mp_bool_t rateSet, ppsSet, navSet, sbasSet, pinningSet, powerSaveSet;
	};

	mp_ret_t mp_drv_venus638_init(mp_kernel_t *kernel, mp_drv_venus638_t *gps, mp_options_t *options, char *who);
	void mp_drv_venus638_fini(mp_drv_venus638_t *gps);

#ifdef SUPPORT_COMMON_DEVICE
	/** an MP_DEVICE_GNSS and an MP_DEVICE_CLOCK */
	extern const mp_device_ops_t mp_drv_venus638_device;
#endif

#endif
