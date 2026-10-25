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
 * Camera view of Devices. Taking the frame while the view is on screen is
 * what runs the video. Each sample is a 2x2 block when the page has room,
 * otherwise one pixel per sample. The part number, serial, software version
 * and temperature the driver published sit under the picture; the title is
 * the device name. A band is filled even while the next frame is being
 * read, or the clear behind it leaves a line across the picture.
 */

#include "../index.h"

#ifdef SYSINFO_SCREEN
#ifdef SUPPORT_COMMON_DEVICE

static int _sysinfo_camera_name(const char *a, const char *b) {
	if(!a || !b)
		return(0);
	while(*a && *b) {
		char ca = *a++;
		char cb = *b++;

		if(ca >= 'A' && ca <= 'Z')
			ca = (char)(ca - 'A' + 'a');
		if(cb >= 'A' && cb <= 'Z')
			cb = (char)(cb - 'A' + 'a');
		if(ca != cb)
			return(0);
	}
	return(*a == *b);
}

static mp_device_t *_sysinfo_camera_device(sysinfo_t *sysinfo) {
	mp_device_registry_t *reg = NULL;
	mp_device_t *legacy = NULL;

	_sysinfo_dev_current(sysinfo, &reg, &legacy);
	return(legacy);
}

/* the camera stays open from boot; taking its frame is what runs the video */
static void _sysinfo_camera_hold(sysinfo_t *sysinfo) {
	mp_device_t *device = _sysinfo_camera_device(sysinfo);

	if(device && device->opened == NO && mp_device_open(&sysinfo->kernel, device) == TRUE)
		sysinfo->screen.cameraSequence = 0;
}

#ifdef SUPPORT_COMMON_DEVICE
static mp_device_registry_t *_sysinfo_camera_temperature(sysinfo_t *sysinfo, mp_device_t *device) {
	mp_device_registry_t *sensor = NULL;

	if(!device)
		return(NULL);
	while((sensor = mp_device_measure_next(&sysinfo->kernel, sensor)) != NULL) {
		const char *name;

		if(sensor->class != MP_DEVICE_TEMPERATURE || !sensor->driver || !sensor->driver->name)
			continue;
		name = sensor->driver->name;
		if(strrchr(name, '/'))
			name = strrchr(name, '/') + 1;
		if(_sysinfo_camera_name(name, device->name))
			return(sensor);
	}
	return(NULL);
}
#endif

static void _sysinfo_camera_draw(mp_tui_widget_t *widget, mp_tui_draw_t *draw) {
	sysinfo_camera_view_t *picture = (sysinfo_camera_view_t *)widget;
	sysinfo_t *sysinfo = picture->owner;
	mp_device_t *device;
	mp_camera_frame_t *frame;
	const unsigned char *pixels;
	unsigned long sequence;
	int width, height, scale, left, top, row, col, row0, row1;

	device = sysinfo ? _sysinfo_camera_device(sysinfo) : NULL;
	frame = device && device->opened == YES ? mp_device_face(device) : NULL;
	sequence = frame ? frame->sequence : 0;
	if(!frame || sequence == 0 || !frame->pixels || !frame->width || !frame->height)
		return;

	pixels = frame->pixels;
	width = frame->width;
	height = frame->height;
	scale = widget->screen.w >= width * 2 && widget->screen.h >= height * 2 ? 2 : 1;
	left = widget->screen.x + (widget->screen.w - width * scale) / 2;
	top = widget->screen.y + (widget->screen.h - height * scale) / 2;
	row0 = (draw->clip.y - top) / scale;
	row1 = (draw->clip.y + draw->clip.h - 1 - top) / scale + 1;
	if(row0 < 0)
		row0 = 0;
	if(row1 > height)
		row1 = height;

	for(row = row0; row < row1; row++) {
		const unsigned char *line = pixels + row * width;

		for(col = 0; col < width; col++) {
			unsigned char sample = line[col];

			mp_tui_draw_fill(draw, left + col * scale, top + row * scale, scale, scale, MP_TUI_RGB(sample, sample, sample));
		}
	}
}

static const mp_tui_class_t _sysinfo_camera_class = { "camera", _sysinfo_camera_draw, NULL, NULL };

static int _sysinfo_camera_line(char *text, int n, const char *line) {
	int wrote;

	if(!line || !line[0] || n >= MP_TUI_TEXT_MAX - 1)
		return(n);
	wrote = snprintf(text + n, (size_t)(MP_TUI_TEXT_MAX - n), "%s%s", n ? "\n" : "", line);
	if(wrote < 0 || n + wrote >= MP_TUI_TEXT_MAX)
		return(MP_TUI_TEXT_MAX - 1);
	return(n + wrote);
}

static void _sysinfo_camera_values(sysinfo_t *sysinfo) {
	mp_device_t *device;
	mp_camera_frame_t *frame;
	unsigned long sequence;

	if(_sysinfo_dev_camera_shown(sysinfo) == NO)
		return;
	_sysinfo_camera_hold(sysinfo);
	device = _sysinfo_camera_device(sysinfo);
	if(!device || device->opened == NO) {
		mp_tui_label_set(&sysinfo->screen.cameraState, "");
		return;
	}

	frame = mp_device_face(device);
	sequence = frame ? frame->sequence : 0;
	if(frame && (sequence & 1UL) == 0 && sequence != 0 && sequence != sysinfo->screen.cameraSequence) {
		sysinfo->screen.cameraSequence = sequence;
		mp_tui_invalidate(&sysinfo->screen.cameraPicture.widget);
	}

	{
		char text[MP_TUI_TEXT_MAX];
		int n = 0;

		text[0] = 0;
		n = _sysinfo_camera_line(text, n, device->model);
		n = _sysinfo_camera_line(text, n, device->serial);
		n = _sysinfo_camera_line(text, n, device->version);
#ifdef SUPPORT_COMMON_DEVICE
		{
			mp_device_registry_t *temperature = _sysinfo_camera_temperature(sysinfo, device);
			float value[3];
			char degrees[16];

			if(temperature && mp_device_measure_read(temperature->instance, value, NULL) == TRUE) {
				char num[8];

				_sysinfo_format_fixed(num, (int)sizeof(num), value[0], 1, NO);
				snprintf(degrees, sizeof(degrees), "%s °C", num);
				n = _sysinfo_camera_line(text, n, degrees);
			}
		}
#endif
		mp_tui_label_set(&sysinfo->screen.cameraState, "%s", n > 0 ? text : "");
	}
}

void _sysinfo_dev_camera_build(mp_tui_widget_t *page, sysinfo_t *sysinfo) {
	const mp_tui_font_t *font = page->tui->theme.font;
	int line = font->lineHeight + 1;
	int caption = line * 4;
	int gap = 6;
	int avail = page->rect.h - caption - gap;
	int scale = 2;
	int imgW;
	int imgH;
	int x;
	int y = 0;
	sysinfo_camera_view_t *picture = &sysinfo->screen.cameraPicture;

	if(avail < 60)
		avail = page->rect.h > 60 ? page->rect.h - gap : page->rect.h;
	if(60 * scale > avail || 80 * scale > page->rect.w)
		scale = 1;
	imgW = 80 * scale;
	imgH = 60 * scale;
	if(imgW > page->rect.w)
		imgW = page->rect.w;
	x = (page->rect.w - imgW) / 2;
	if(page->rect.h > imgH + gap + caption)
		y = (page->rect.h - imgH - gap - caption) / 2;

	picture->owner = sysinfo;
	mp_tui_widget_init(&picture->widget, &_sysinfo_camera_class, page, x, y, imgW, imgH);
	mp_tui_label(&sysinfo->screen.cameraState, page, 4, y + imgH + gap, page->rect.w - 8, caption, "");
	mp_tui_label_align(&sysinfo->screen.cameraState, MP_TUI_ALIGN_CENTER);
	_sysinfo_camera_values(sysinfo);
}

void _sysinfo_dev_camera_refresh(sysinfo_t *sysinfo) {
	if(_sysinfo_dev_camera_shown(sysinfo) == YES)
		_sysinfo_camera_values(sysinfo);
}

#endif
#endif
