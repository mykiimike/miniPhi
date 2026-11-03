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
 * Camera view of Devices. At boot the camera is opened at High so the
 * driver can read the module, then closed; that name stays on the device
 * and the list shows it. Selecting the view holds the clock at High and
 * only then opens the camera again. Leaving closes it, then the governor
 * takes the clock back. Taking the frame while the view is on screen is
 * what runs the video. Each sample is a 2x2 block when the page has room,
 * otherwise one pixel per sample. The part number, serial, software version
 * and temperature the driver published sit under the picture; the title is
 * the device name. A band is filled even while the next frame is being
 * read, or the clear behind it leaves a line across the picture.
 * A tap on the picture shows it full screen until the cross is tapped.
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
	void *self;
	int n;
	int count;

	_sysinfo_dev_current(sysinfo, &reg, &legacy);
	if(legacy)
		return(legacy);
	/* the registered camera hides the board device; the frame is still its face */
	if(!reg || reg->class != MP_DEVICE_CAMERA || !reg->driver || !reg->driver->self)
		return(NULL);
	self = reg->driver->self;
	count = mp_device_count();
	for(n = 0; n < count; n++) {
		mp_device_t *device = mp_device_at(n);

		if(device && device->self == self)
			return(device);
	}
	return(NULL);
}

/* High first, and the clock has moved, before the driver's init runs */
static void _sysinfo_camera_arm(sysinfo_t *sysinfo) {
	mp_device_t *device = _sysinfo_camera_device(sysinfo);

	if(!device)
		return;
	mp_power_hold(&sysinfo->kernel, &sysinfo->screen.cameraPower, MP_POWER_HIGH, "Camera");
	mp_power_apply(&sysinfo->kernel);
	if(mp_power_level(&sysinfo->kernel) != MP_POWER_HIGH)
		return;
	if(device->opened == NO && mp_device_open(&sysinfo->kernel, device) == TRUE)
		sysinfo->screen.cameraSequence = 0;
}

void _sysinfo_dev_camera_release(sysinfo_t *sysinfo) {
	int n, count;

	if(!sysinfo)
		return;
	count = mp_device_count();
	for(n = 0; n < count; n++) {
		mp_device_t *device = mp_device_at(n);

		if(!device || device->opened == NO || !device->driver || device->driver->type != MP_DEVICE_CAMERA)
			continue;
		mp_device_close(device);
	}
	if(sysinfo->screen.cameraPower.held == NO)
		return;
	mp_power_release(&sysinfo->kernel, &sysinfo->screen.cameraPower);
	mp_power_apply(&sysinfo->kernel);
}

/* part and version are in, or the driver has had long enough and published a name */
static int _sysinfo_camera_described(sysinfo_t *sysinfo, mp_device_t *device) {
	unsigned long now;

	if(!device->label)
		return(0);
	if(device->version)
		return(1);
	if(!sysinfo->screen.cameraProbeAt)
		return(0);
	now = mp_clock_ticks();
	return((long)(now - sysinfo->screen.cameraProbeAt) > (long)_sysinfo_ticks(8000));
}

static int _sysinfo_camera_busy(sysinfo_t *sysinfo) {
	int n, count;

	count = mp_device_count();
	for(n = 0; n < count; n++) {
		mp_device_t *device = mp_device_at(n);

		if(!device || device->opened == NO || !device->driver || device->driver->type != MP_DEVICE_CAMERA)
			continue;
		if(_sysinfo_camera_described(sysinfo, device) == 0)
			return(1);
	}
	return(0);
}

static int _sysinfo_camera_opened(void) {
	int n, count, opened = 0;

	count = mp_device_count();
	for(n = 0; n < count; n++) {
		mp_device_t *device = mp_device_at(n);

		if(device && device->opened == YES && device->driver && device->driver->type == MP_DEVICE_CAMERA)
			opened++;
	}
	return(opened);
}

/* a camera that has never published its name */
static int _sysinfo_camera_unknown(void) {
	int n, count;

	count = mp_device_count();
	for(n = 0; n < count; n++) {
		mp_device_t *device = mp_device_at(n);

		if(!device || device->opened == YES || !device->driver || device->driver->type != MP_DEVICE_CAMERA)
			continue;
		if(!device->label)
			return(1);
	}
	return(0);
}

/* High, and the clock has moved, before the driver's init reads the module */
static void _sysinfo_camera_probe(sysinfo_t *sysinfo) {
	int n, count, opened;

	if(_sysinfo_camera_unknown() == 0)
		return;
	mp_power_hold(&sysinfo->kernel, &sysinfo->screen.cameraPower, MP_POWER_HIGH, "Camera");
	mp_power_apply(&sysinfo->kernel);
	if(mp_power_level(&sysinfo->kernel) != MP_POWER_HIGH)
		return;
	opened = 0;
	count = mp_device_count();
	for(n = 0; n < count; n++) {
		mp_device_t *device = mp_device_at(n);

		if(!device || device->opened == YES || device->label || !device->driver || device->driver->type != MP_DEVICE_CAMERA)
			continue;
		if(mp_device_open(&sysinfo->kernel, device) == TRUE)
			opened = 1;
	}
	if(opened == 0 || sysinfo->screen.cameraProbeAt)
		return;
	sysinfo->screen.cameraProbeAt = mp_clock_ticks();
	if(!sysinfo->screen.cameraProbeAt)
		sysinfo->screen.cameraProbeAt = 1;
}

void _sysinfo_dev_camera_settle(sysinfo_t *sysinfo) {
	if(!sysinfo)
		return;
	if(_sysinfo_dev_camera_shown(sysinfo) == YES) {
		_sysinfo_dev_camera_refresh(sysinfo);
		return;
	}
	/* the driver is still reading the module; the view is not up */
	if(_sysinfo_camera_busy(sysinfo))
		return;
	if(_sysinfo_camera_opened()) {
		_sysinfo_dev_camera_release(sysinfo);
		return;
	}
	_sysinfo_camera_probe(sysinfo);
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

static void _sysinfo_camera_closed(mp_tui_t *ui, void *user) {
	sysinfo_t *sysinfo = user;

	(void)ui;
	if(!sysinfo)
		return;
	mp_tui_invalidate(&sysinfo->screen.cameraPicture.widget);
	mp_tui_invalidate(&sysinfo->screen.cameraState.widget);
}

static int _sysinfo_camera_event(mp_tui_widget_t *widget, const mp_tui_event_t *event) {
	sysinfo_camera_view_t *picture = (sysinfo_camera_view_t *)widget;
	sysinfo_t *sysinfo = picture->owner;
	mp_device_t *device;
	mp_camera_frame_t *frame;
	mp_tui_image_t image;

	if(event->type != MP_TUI_EVENT_CLICK)
		return(0);
	device = sysinfo ? _sysinfo_camera_device(sysinfo) : NULL;
	frame = device && device->opened == YES ? mp_device_face(device) : NULL;
	if(!frame || !frame->pixels || !frame->width || !frame->height || frame->sequence == 0)
		return(1);
	image.pixels = frame->pixels;
	image.w = frame->width;
	image.h = frame->height;
	image.format = MP_TUI_IMAGE_GRAY8;
	mp_tui_fullscreen(widget->tui, &image, _sysinfo_camera_closed, sysinfo);
	return(1);
}

static const mp_tui_class_t _sysinfo_camera_class = { "camera", _sysinfo_camera_draw, NULL, _sysinfo_camera_event };

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
	_sysinfo_camera_arm(sysinfo);
	device = _sysinfo_camera_device(sysinfo);
	if(!device || device->opened == NO) {
		mp_tui_label_set(&sysinfo->screen.cameraState, "");
		return;
	}

	frame = mp_device_face(device);
	sequence = frame ? frame->sequence : 0;
	if(frame && (sequence & 1UL) == 0 && sequence != 0 && sequence != sysinfo->screen.cameraSequence) {
		sysinfo->screen.cameraSequence = sequence;
		if(mp_tui_fullscreen_shown(&sysinfo->screen.tui))
			mp_tui_fullscreen_update(&sysinfo->screen.tui);
		else
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
	_sysinfo_camera_arm(sysinfo);
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
