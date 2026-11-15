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

#ifdef SUPPORT_DRV_FS_SD

/**
 * Channel 0 is the card. Channels 1..MP_DRV_SD_PARTS are the primaries,
 * which is as many extra channels as a driver remembers.
 */
typedef char _mp_drv_sd_parts_fit[(MP_DRV_SD_PARTS <= MP_DEVICE_DRIVER_EXTRA) ? 1 : -1];

static const char *_sd_part_label[MP_DRV_SD_PARTS] = {
	"SD partition 1",
	"SD partition 2",
	"SD partition 3",
	"SD partition 4"
};

static const char *_sd_model(mp_fs_sd_t *card) {
	const unsigned char *p;

	if(!card->product[0])
		return("sd");
	for(p = (const unsigned char *)card->product; *p; p++)
		if(*p < 0x20 || *p >= 0x80)
			return("sd");
	return(card->product);
}

static void _sd_serial(mp_drv_sd_t *sd) {
	static const char hex[] = "0123456789abcdef";
	unsigned long value = sd->card.serial;
	char tmp[8];
	int n = 0;
	int i;

	do {
		tmp[n++] = hex[value & 0x0f];
		value >>= 4;
	} while(value && n < (int)sizeof(tmp));
	for(i = 0; i < n; i++)
		sd->serial_text[i] = tmp[n - 1 - i];
	sd->serial_text[n] = 0;
}

static void _sd_board(mp_drv_sd_t *sd) {
	const char *model = "sd";
	const char *serial;

	if(sd->card.ready == YES) {
		model = _sd_model(&sd->card);
		_sd_serial(sd);
		serial = sd->serial_text;
	}
	else {
		serial = mp_machine_serial();
		if(!serial || !serial[0])
			serial = "sd";
	}
	mp_device_identify(sd, "SD card", model, serial, NULL);
}

static void _sd_drop(mp_device_registry_t *device) {
	if(device && device->driver)
		mp_device_registry_unregister(device);
}

static void _sd_retract(mp_drv_sd_t *sd) {
	int n;

	for(n = 0; n < MP_DRV_SD_PARTS; n++)
		_sd_drop(&sd->part_dev[n]);
	_sd_drop(&sd->disk);
	sd->parts = 0;
	_sd_board(sd);
}

static void _sd_publish(mp_drv_sd_t *sd) {
	const char *model;
	int found, n;

	if(!sd || sd->card.ready != YES)
		return;
	_sd_retract(sd);
	if(sd->card.ready != YES)
		return;
	model = _sd_model(&sd->card);
	_sd_serial(sd);
	if(!mp_device_registry_register(&sd->binding, &sd->disk, MP_DEVICE_BLOCK, 0, &sd->card.block, model, sd->serial_text, NULL)) {
		mp_printk("SD: can not register the card");
		_sd_board(sd);
		return;
	}
	sd->disk.label = "SD card";
	found = mp_fs_part_scan(&sd->card.block, sd->part, MP_DRV_SD_PARTS);
	if(sd->card.ready != YES || !sd->disk.driver)
		return;
	if(found == MP_FS_PART_GPT) {
		mp_printk("SD: GPT is not supported");
		_sd_board(sd);
		return;
	}
	if(found < 0) {
		_sd_board(sd);
		return;
	}
	if(found > MP_DRV_SD_PARTS)
		found = MP_DRV_SD_PARTS;
	for(n = 0; n < found; n++) {
		if(!mp_device_registry_register(&sd->binding, &sd->part_dev[n], MP_DEVICE_BLOCK, n + 1, &sd->part[n].block, model, sd->serial_text, NULL)) {
			mp_printk("SD: can not register partition %d", n + 1);
			break;
		}
		sd->part_dev[n].label = _sd_part_label[n];
	}
	sd->parts = n;
	_sd_board(sd);
}

static void _sd_on_media(mp_fs_sd_t *card, mp_bool_t present, void *user) {
	mp_drv_sd_t *sd = user;

	(void)card;
	if(!sd)
		return;
	if(present == YES)
		_sd_publish(sd);
	else
		_sd_retract(sd);
}

mp_ret_t mp_drv_sd_init(mp_kernel_t *kernel, mp_drv_sd_t *sd, mp_options_t *options, char *who) {
	static const mp_device_driver_ops_t ops = {
		"fs/sd", "SD card", "1.0.0",
		(unsigned int)(1u << MP_DEVICE_BLOCK),
		NULL, NULL, NULL,
		NULL, NULL
	};

	if(!kernel || !sd)
		return(FALSE);
	memset(sd, 0, sizeof(*sd));
	sd->kernel = kernel;
	if(mp_fs_sd_init(kernel, &sd->card, options, who) != TRUE) {
		memset(sd, 0, sizeof(*sd));
		return(FALSE);
	}
	if(mp_device_driver_register(kernel, &sd->binding, &ops, sd, NULL) != TRUE) {
		mp_fs_sd_fini(&sd->card);
		memset(sd, 0, sizeof(*sd));
		return(FALSE);
	}
	sd->card.on_media = _sd_on_media;
	sd->card.on_media_user = sd;
	if(sd->card.ready == YES)
		_sd_publish(sd);
	else
		_sd_board(sd);
	return(TRUE);
}

void mp_drv_sd_fini(mp_drv_sd_t *sd) {
	if(!sd || !sd->kernel)
		return;
	sd->card.on_media = NULL;
	_sd_retract(sd);
	sd->card.ready = NO;
	_sd_board(sd);
	if(sd->binding.kernel)
		mp_device_driver_unregister(sd->kernel, &sd->binding);
	if(sd->card.kernel)
		mp_fs_sd_fini(&sd->card);
	sd->kernel = NULL;
}

#ifdef SUPPORT_COMMON_DEVICE

static mp_ret_t _sd_open(mp_kernel_t *kernel, void *self, mp_options_t *options, char *who) {
	return(mp_drv_sd_init(kernel, self, options, who));
}

static void _sd_close(void *self) {
	mp_drv_sd_fini(self);
}

static void _sd_check(mp_drv_sd_t *sd) {
	if(mp_fs_sd_probe(&sd->card) == TRUE && sd->card.ready == YES && !sd->disk.driver)
		_sd_publish(sd);
}

static void *_sd_face(void *self) {
	mp_drv_sd_t *sd = self;

	if(!sd)
		return(NULL);
	_sd_check(sd);
	if(sd->card.ready != YES)
		return(NULL);
	return(&sd->card.block);
}

static void _sd_set(void *self, int value) {
	mp_drv_sd_t *sd = self;

	if(sd && value)
		_sd_check(sd);
}

const mp_device_ops_t mp_drv_sd_device = {
	"fs/sd", MP_DEVICE_BLOCK, _sd_open, _sd_close, _sd_face, _sd_set,
	"SD card", "1.0.0"
};

#endif

#endif
