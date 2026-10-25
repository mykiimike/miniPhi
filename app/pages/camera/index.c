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
 * Home page. One button per camera. Opening one runs that camera and draws
 * its frame while the picture page stays on screen. Each sample is drawn
 * as a 2x2 block, centered under the header. The model, the serial number,
 * the software version and the temperature are drawn over it. A band is
 * filled even while the next frame is being read, or the clear behind it
 * leaves a line across the picture.
 */

#include "../index.h"

#ifdef SYSINFO_SCREEN

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

static void _sysinfo_camera_hold(sysinfo_t *sysinfo, mp_bool_t on) {
	mp_device_t *want = NULL;
	mp_device_t *device;
	int rank;

	if(on == YES)
		want = mp_device_find(MP_DEVICE_CAMERA, sysinfo->screen.cameraPick);
	for(rank = 0; (device = mp_device_find(MP_DEVICE_CAMERA, rank)) != NULL; rank++) {
		if(device == want) {
			if(device->opened == NO && mp_device_open(&sysinfo->kernel, device) == TRUE)
				sysinfo->screen.cameraSequence = 0;
		}
		else if(device->opened == YES)
			mp_device_close(device);
	}
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
	int width, height, left, top, row, col, row0, row1;

	device = sysinfo ? mp_device_find(MP_DEVICE_CAMERA, sysinfo->screen.cameraPick) : NULL;
	frame = device && device->opened == YES ? mp_device_face(device) : NULL;
	sequence = frame ? frame->sequence : 0;
	if(!frame || sequence == 0 || !frame->pixels || !frame->width || !frame->height)
		return;

	pixels = frame->pixels;
	width = frame->width;
	height = frame->height;
	left = widget->screen.x + (widget->screen.w - width * 2) / 2;
	top = widget->screen.y + (widget->screen.h - height * 2) / 2;
	row0 = (draw->clip.y - top) / 2;
	row1 = (draw->clip.y + draw->clip.h - 1 - top) / 2 + 1;
	if(row0 < 0)
		row0 = 0;
	if(row1 > height)
		row1 = height;

	for(row = row0; row < row1; row++) {
		const unsigned char *line = pixels + row * width;

		for(col = 0; col < width; col++) {
			unsigned char sample = line[col];

			mp_tui_draw_fill(draw, left + col * 2, top + row * 2, 2, 2, MP_TUI_RGB(sample, sample, sample));
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

	if(sysinfo->screen.cameraView < 0 || sysinfo->screen.tui.opened != sysinfo->screen.cameraView)
		return;
	_sysinfo_camera_hold(sysinfo, YES);
	device = mp_device_find(MP_DEVICE_CAMERA, sysinfo->screen.cameraPick);
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
				snprintf(degrees, sizeof(degrees), "%.1f °C", (double)value[0]);
				n = _sysinfo_camera_line(text, n, degrees);
			}
		}
#endif
		mp_tui_label_set(&sysinfo->screen.cameraState, "%s", n > 0 ? text : "");
	}
}

static void _sysinfo_camera_view_page(mp_tui_widget_t *page, void *user) {
	sysinfo_t *sysinfo = user;
	const mp_tui_font_t *font = page->tui->theme.font;
	int line = font->lineHeight + 1;
	int width = page->rect.w - 16;
	int y = 8;
	int imgW = 160;
	int imgH = 120;
	int x;
	sysinfo_camera_view_t *picture = &sysinfo->screen.cameraPicture;
	mp_device_t *device;
	mp_camera_frame_t *frame;

	_sysinfo_camera_hold(sysinfo, YES);
	device = mp_device_find(MP_DEVICE_CAMERA, sysinfo->screen.cameraPick);
	frame = device && device->opened == YES ? mp_device_face(device) : NULL;
	if(frame && frame->width && frame->height) {
		imgW = (int)frame->width * 2;
		imgH = (int)frame->height * 2;
	}
	if(imgW > page->rect.w)
		imgW = page->rect.w;
	if(imgH > page->rect.h)
		imgH = page->rect.h;
	x = (page->rect.w - imgW) / 2;
	y = (page->rect.h - imgH) / 2;
	if(y < 8)
		y = 8;

	picture->owner = sysinfo;
	mp_tui_widget_init(&picture->widget, &_sysinfo_camera_class, page, x, y, imgW, imgH);
	mp_tui_label(&sysinfo->screen.cameraState, page, 8, 8, width, line * 4, "");
	mp_tui_label_color(&sysinfo->screen.cameraState, page->tui->theme.muted);
	_sysinfo_camera_values(sysinfo);
}

static void _sysinfo_on_camera(mp_tui_button_t *button) {
	sysinfo_t *sysinfo = button->user;
	mp_device_t *device;
	int pick = button->id;

	if(!sysinfo || sysinfo->screen.cameraView < 0)
		return;
	device = mp_device_find(MP_DEVICE_CAMERA, pick);
	if(!device)
		return;
	sysinfo->screen.cameraPick = pick;
	sysinfo->screen.cameraSequence = 0;
	sysinfo->screen.tui.app[sysinfo->screen.cameraView].name = device->name;
	mp_tui_enter(&sysinfo->screen.tui, sysinfo->screen.cameraView);
}

static void _sysinfo_camera_page(mp_tui_widget_t *page, void *user) {
	sysinfo_t *sysinfo = user;
	const mp_tui_font_t *font = page->tui->theme.font;
	int line = font->lineHeight + 1;
	int width = page->rect.w - 16;
	int h = line + 18;
	int y = 8;
	int count = 0;
	int rank;
	mp_device_t *device;

	for(rank = 0; count < SYSINFO_CAMERA_MAX && (device = mp_device_find(MP_DEVICE_CAMERA, rank)) != NULL; rank++) {
		mp_tui_button(&sysinfo->screen.cameraButton[count], page, 8, y, width, h, device->name);
		sysinfo->screen.cameraButton[count].chevron = 1;
		sysinfo->screen.cameraButton[count].onPress = _sysinfo_on_camera;
		sysinfo->screen.cameraButton[count].user = sysinfo;
		sysinfo->screen.cameraButton[count].id = rank;
		y += h + 6;
		count++;
	}
	if(count == 0) {
		mp_tui_label(&sysinfo->screen.cameraState, page, 8, 12, width, line, "No camera");
		mp_tui_label_color(&sysinfo->screen.cameraState, page->tui->theme.muted);
	}
}

void _sysinfo_camera_open(sysinfo_t *sysinfo) {
	mp_tui_t *tui = &sysinfo->screen.tui;

	sysinfo->screen.cameraIndex = -1;
	sysinfo->screen.cameraView = -1;
	sysinfo->screen.cameraPick = 0;
	sysinfo->screen.cameraSequence = 0;
	if(mp_tui_app(tui, "Camera", _sysinfo_camera_page, sysinfo))
		sysinfo->screen.cameraIndex = tui->appCount - 1;
	if(mp_tui_child(tui, "Camera", _sysinfo_camera_view_page, sysinfo))
		sysinfo->screen.cameraView = tui->appCount - 1;
}

/* the Devices page opens each camera so the chip can register its identity */
static void _sysinfo_camera_identify(sysinfo_t *sysinfo) {
	mp_device_t *device;
	int rank;

	for(rank = 0; (device = mp_device_find(MP_DEVICE_CAMERA, rank)) != NULL; rank++) {
		if(device->opened == NO)
			mp_device_open(&sysinfo->kernel, device);
	}
}

void _sysinfo_camera_refresh(sysinfo_t *sysinfo) {
	int devices = sysinfo->screen.devicesReady && sysinfo->screen.tui.opened == sysinfo->screen.devicesIndex;

	if(sysinfo->screen.cameraView >= 0 && sysinfo->screen.tui.opened == sysinfo->screen.cameraView)
		_sysinfo_camera_values(sysinfo);
	else if(devices)
		_sysinfo_camera_identify(sysinfo);
	else
		_sysinfo_camera_hold(sysinfo, NO);
}

#endif
