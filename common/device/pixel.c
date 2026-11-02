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
 * Pixel contract. The driver owns an mp_device_pixel_t and the channel array,
 * and registers the struct as the instance. One subscriber owns every channel.
 * A channel write changes only that byte. commit sends the one frame.
 */

static int _mp_device_pixel_shade(int value) {
	if(value <= 0)
		return(0);
	if(value > 255)
		return(255);
	return(value);
}

static mp_device_pixel_channel_t *_mp_device_pixel_at(mp_device_pixel_t *pixel, int index, int channel) {
	if(!pixel || !pixel->channel || index < 0 || index >= pixel->count)
		return(NULL);
	if(channel < 0 || channel >= MP_DEVICE_PIXEL_CHANNELS)
		return(NULL);
	return(&pixel->channel[index * MP_DEVICE_PIXEL_CHANNELS + channel]);
}

static void _mp_device_pixel_commit(mp_device_pixel_t *pixel) {
	if(pixel && pixel->commit)
		pixel->commit(pixel);
}

static void _mp_device_pixel_write(mp_device_pixel_channel_t *die, int value) {
	value = _mp_device_pixel_shade(value);
	if(value) {
		die->level = (unsigned char)value;
		die->on = 1;
	}
	else
		die->on = 0;
}

static int _mp_device_pixel_remember(mp_device_pixel_channel_t *die, int level) {
	level = _mp_device_pixel_shade(level);
	if(!die || !level)
		return(0);
	die->level = (unsigned char)level;
	return(die->on ? 1 : 0);
}

static void _mp_device_pixel_lamp(mp_device_pixel_channel_t *die, int on) {
	if(on) {
		if(!die->level)
			die->level = 255;
		die->on = 1;
	}
	else
		die->on = 0;
}

static unsigned char _mp_device_pixel_byte(mp_device_pixel_channel_t *die) {
	if(!die->on || !die->level)
		return(0);
	return(die->level);
}

mp_device_pixel_t *mp_device_pixel(mp_device_registry_t *device, mp_device_subscribe_t *sub) {
	return((mp_device_pixel_t *)mp_device_subscribe(device, sub));
}

void mp_device_pixel_set(mp_device_pixel_t *pixel, int index, int red, int green, int blue) {
	mp_device_pixel_channel_t *die;
	int color[MP_DEVICE_PIXEL_CHANNELS];
	int channel;

	color[MP_DEVICE_PIXEL_RED] = red;
	color[MP_DEVICE_PIXEL_GREEN] = green;
	color[MP_DEVICE_PIXEL_BLUE] = blue;
	if(!_mp_device_pixel_at(pixel, index, MP_DEVICE_PIXEL_RED))
		return;
	for(channel = 0; channel < MP_DEVICE_PIXEL_CHANNELS; channel++) {
		die = _mp_device_pixel_at(pixel, index, channel);
		_mp_device_pixel_write(die, color[channel]);
	}
	_mp_device_pixel_commit(pixel);
}

void mp_device_pixel_channel_set(mp_device_pixel_t *pixel, int index, int channel, int on) {
	mp_device_pixel_channel_t *die;

	die = _mp_device_pixel_at(pixel, index, channel);
	if(!die)
		return;
	_mp_device_pixel_lamp(die, on);
	_mp_device_pixel_commit(pixel);
}

void mp_device_pixel_level(mp_device_pixel_t *pixel, int index, int level) {
	int channel;
	int lit = 0;

	if(!_mp_device_pixel_at(pixel, index, MP_DEVICE_PIXEL_RED))
		return;
	if(!_mp_device_pixel_shade(level))
		return;
	for(channel = 0; channel < MP_DEVICE_PIXEL_CHANNELS; channel++)
		if(_mp_device_pixel_remember(_mp_device_pixel_at(pixel, index, channel), level))
			lit = 1;
	if(lit)
		_mp_device_pixel_commit(pixel);
}

void mp_device_pixel_channel_level(mp_device_pixel_t *pixel, int index, int channel, int level) {
	if(!_mp_device_pixel_remember(_mp_device_pixel_at(pixel, index, channel), level))
		return;
	_mp_device_pixel_commit(pixel);
}

void mp_device_pixel_channel_turn(mp_device_pixel_t *pixel, int index, int channel) {
	mp_device_pixel_channel_t *die;

	die = _mp_device_pixel_at(pixel, index, channel);
	if(!die)
		return;
	_mp_device_pixel_lamp(die, die->on ? 0 : 1);
	_mp_device_pixel_commit(pixel);
}

int mp_device_pixel_channel_state(mp_device_pixel_t *pixel, int index, int channel) {
	mp_device_pixel_channel_t *die;

	die = _mp_device_pixel_at(pixel, index, channel);
	return(die && die->on ? 1 : 0);
}

mp_ret_t mp_device_pixel_color(mp_device_pixel_t *pixel, int index, unsigned char *red, unsigned char *green, unsigned char *blue) {
	mp_device_pixel_channel_t *die;
	unsigned char *out[MP_DEVICE_PIXEL_CHANNELS];
	int channel;

	out[MP_DEVICE_PIXEL_RED] = red;
	out[MP_DEVICE_PIXEL_GREEN] = green;
	out[MP_DEVICE_PIXEL_BLUE] = blue;
	die = _mp_device_pixel_at(pixel, index, MP_DEVICE_PIXEL_RED);
	if(!die) {
		for(channel = 0; channel < MP_DEVICE_PIXEL_CHANNELS; channel++)
			if(out[channel])
				*out[channel] = 0;
		return(FALSE);
	}
	for(channel = 0; channel < MP_DEVICE_PIXEL_CHANNELS; channel++)
		if(out[channel])
			*out[channel] = _mp_device_pixel_byte(die + channel);
	return(TRUE);
}

#endif
