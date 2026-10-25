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

#ifndef _HAVE_MP_DRV_LEPTON_H
	#define _HAVE_MP_DRV_LEPTON_H

	/**
	 * FLIR Lepton 2 on the v1.3 breakout.
	 * VoSPI on SPI mode 3 fills one 80x60 frame of bytes; the face is that frame.
	 * When the SPI has DMA engines, each packet is moved there and chip select
	 * rises in that completion, so the CPU can draw meanwhile. Otherwise the
	 * task clocks the packet itself.
	 * CCI on I2C reads the FLIR serial, the part number and the software version
	 * as soon as the driver is open, then publishes the FPA temperature in °C.
	 * The device name comes from that part number ("FLIR Lepton 2"), never
	 * from the board option. VoSPI runs only while the frame is being taken.
	 * The frame lives in the device, not on the heap.
	 *
	 * options: model (required, not published). The serial shown is the
	 * module's own once CCI returns it. spiGate, simo, somi, sck, cs,
	 * sda, scl, address (0x2A).
	 */

	#define MP_DRV_LEPTON_WIDTH 80
	#define MP_DRV_LEPTON_HEIGHT 60

	typedef struct mp_drv_lepton_s mp_drv_lepton_t;

	struct mp_drv_lepton_s {
		mp_kernel_t *kernel;
		mp_spi_t spi;
		mp_gpio_port_t *cs;
		/** CCI, driven by hand: the FSMC holds PB7 while its clock runs */
		mp_gpio_port_t *sda;
		mp_gpio_port_t *scl;
		mp_task_t *task;
		mp_device_driver_t binding;
		mp_device_registry_t tempDev;
		mp_device_measure_t temperature;

		mp_camera_frame_t frame;
		unsigned char pixels[MP_DRV_LEPTON_WIDTH * MP_DRV_LEPTON_HEIGHT];
		unsigned char packet[164];

		unsigned char address;
		/** boot, idle while nobody takes the frame, sync, video */
		unsigned char phase;
		mp_bool_t booted;
		/** YES once a failed command was reported */
		mp_bool_t gaveUp;
		/** data words of the last CCI answer */
		unsigned char cciRx[32];
		/** model from the board, and the MCU unique id, until the module answers */
		const char *askedModel;
		const char *askedSerial;
		char model[33];
		char serial[17];
		char version[40];
		/** product name from the part number ("FLIR Lepton 2"); NULL until known */
		const char *label;
		/** last CCI round: identity until known, then the temperature */
		unsigned long tempAt;
		unsigned long bootAt;
		/** last time the frame was taken; video stops half a second after */
		unsigned long watchAt;
		/** floor at HIGH while frames stream; DMA leaves the CPU idle so the load alone would drop the clock under VoSPI */
		mp_power_hold_t power;
		/** raw ends of the frame being read, and the stretch used to store it */
		unsigned short seenMin;
		unsigned short seenMax;
		unsigned short bias;
		unsigned int scale;

		#ifdef SUPPORT_COMMON_DMA
			mp_dma_t dmaRx;
			mp_dma_t dmaTx;
			mp_dma_seg_t rxSeg;
			mp_dma_seg_t txSeg;
			/** the byte the TX engine repeats for the whole packet */
			unsigned char dmaZero;
			/** YES once both SPI engines are mapped */
			mp_bool_t dmaUse;
			/** -1 seeks line 0, otherwise the next row */
			short dmaRow;
			unsigned short dmaSkips;
			mp_bool_t dmaOn;
			/** 0 idle, 1 frame, 2 lost */
			signed char dmaResult;
		#endif
	};

	mp_ret_t mp_drv_lepton_init(mp_kernel_t *kernel, mp_drv_lepton_t *lep, mp_options_t *options, char *who);
	void mp_drv_lepton_fini(mp_drv_lepton_t *lep);

#ifdef SUPPORT_COMMON_DEVICE
	/** an MP_DEVICE_CAMERA */
	extern const mp_device_ops_t mp_drv_lepton_device;
#endif

#endif
