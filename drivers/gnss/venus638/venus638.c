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

#ifdef SUPPORT_DRV_VENUS638

#define VENUS_LABEL "SkyTraq Venus638"

/* time without a valid sentence before the next baud rate is tried */
#define VENUS_PROBE_MS 2500
/* listen window after init and after a scan, with nobody subscribed */
#define VENUS_WINDOW_MS 10000
#define VENUS_SCAN_MS 3000
/* a running receiver silent for longer is gone */
#define VENUS_GONE_MS 5000
#define VENUS_ACK_MS 1000

enum {
	VENUS_PARSE_IDLE = 0,
	VENUS_PARSE_LINE,
	VENUS_PARSE_A1,
	VENUS_PARSE_LENH,
	VENUS_PARSE_LENL,
	VENUS_PARSE_BODY,
	VENUS_PARSE_CS,
};

/* what the frame in flight was for */
enum {
	VENUS_K_NONE = 0,
	VENUS_K_NMEA,
	VENUS_K_VERSION,
	VENUS_K_QRATE,
	VENUS_K_QSBAS,
	VENUS_K_QPIN,
	VENUS_K_QNAV,
	VENUS_K_QPPS,
	VENUS_K_BAUD,
	VENUS_K_RATE,
	VENUS_K_PPS,
	VENUS_K_SBAS,
	VENUS_K_NAV,
	VENUS_K_PIN,
	VENUS_K_PSAVE,
	VENUS_K_RESTART,
	VENUS_K_FACTORY,
};

/* sent once the module is heard: GGA and RMC only, then what it is set to */
static const unsigned char _venus_boot[] = {
	VENUS_K_NMEA, VENUS_K_VERSION, VENUS_K_QRATE, VENUS_K_QSBAS, VENUS_K_QPIN, VENUS_K_QNAV, VENUS_K_QPPS
};
#define VENUS_BOOT_STEPS (sizeof(_venus_boot) / sizeof(_venus_boot[0]))

static const unsigned long _venus_bauds[] = { 4800, 9600, 19200, 38400, 57600, 115200 };
static const unsigned long _venus_probe[] = { 9600, 38400, 115200, 4800 };
#define VENUS_PROBE_COUNT (sizeof(_venus_probe) / sizeof(_venus_probe[0]))

static const unsigned char _venus_rates[] = { 1, 2, 4, 5, 8, 10, 20 };
static const char *const _venus_rate_choice[] = { "1", "2", "4", "5", "8", "10", "20", NULL };
static const char *const _venus_pps_choice[] = { "off", "3d", "1sv", NULL };
static const char *const _venus_nav_choice[] = { "car", "pedestrian", NULL };
static const char *const _venus_restart_choice[] = { "hot", "warm", "cold", NULL };

#define VENUS_COMMANDS 9
static const mp_device_command_t _venus_commands[VENUS_COMMANDS];

static unsigned long _venus_ticks(unsigned long ms) {
	unsigned long hz = mp_clock_tick_hz();
	unsigned long ticks = hz ? (ms * hz) / 1000 : ms;

	return(ticks ? ticks : 1);
}

static mp_bool_t _venus_after(unsigned long now, unsigned long at) {
	return((long)(now - at) >= 0 ? YES : NO);
}

/*
 * Receive interrupt. One byte per call; the task is woken on a line feed,
 * which ends both an NMEA sentence and a binary frame.
 */
static void _venus_rx(mp_uart_t *uart) {
	mp_drv_venus638_t *gps = uart->user;
	unsigned char c = mp_uart_rx(uart);
	unsigned char next = (unsigned char)((gps->head + 1) % MP_DRV_VENUS638_RING);

	if(next == gps->tail)
		gps->dropped++;
	else {
		gps->ring[gps->head] = c;
		gps->head = next;
	}
	if(c == '\n' && gps->task)
		mp_task_wake(gps->task);
}

static void _venus_tx(mp_uart_t *uart) {
	mp_drv_venus638_t *gps = uart->user;

	if(gps->txAt < gps->txLen)
		mp_uart_tx(uart, gps->tx[gps->txAt++]);
	if(gps->txAt >= gps->txLen)
		mp_uart_disable_tx_int(uart);
}

static void _venus_baud(mp_drv_venus638_t *gps, unsigned long baud) {
	char text[8];
	mp_options_t options[] = { { "baudRate", text }, { NULL, NULL } };

	snprintf(text, sizeof(text), "%lu", baud);
	mp_uart_disable_rx_int(&gps->uart);
	if(mp_uart_setup(&gps->uart, options) == TRUE)
		gps->baud = baud;
	gps->state = VENUS_PARSE_IDLE;
	if(gps->listening == YES)
		mp_uart_enable_rx_int(&gps->uart);
}

/*
 * One binary frame. The XOR covers the payload only. The answer is
 * awaited before the next frame goes out.
 */
static void _venus_send(mp_drv_venus638_t *gps, const unsigned char *body, int n, unsigned char kind) {
	unsigned char cs = 0;
	int i;

	if(n > MP_DRV_VENUS638_FRAME)
		return;
	gps->tx[0] = 0xA0;
	gps->tx[1] = 0xA1;
	gps->tx[2] = 0;
	gps->tx[3] = (unsigned char)n;
	for(i = 0; i < n; i++) {
		gps->tx[4 + i] = body[i];
		cs ^= body[i];
	}
	gps->tx[4 + n] = cs;
	gps->tx[5 + n] = 0x0D;
	gps->tx[6 + n] = 0x0A;
	gps->txAt = 0;
	gps->txLen = (unsigned char)(n + 7);
	gps->kind = kind;
	gps->waitId = body[0];
	gps->waiting = YES;
	gps->waitUntil = mp_clock_ticks() + _venus_ticks(VENUS_ACK_MS);
	mp_uart_enable_tx_int(&gps->uart);
}

static unsigned char _venus_baud_code(unsigned long baud) {
	unsigned char i;

	for(i = 0; i < sizeof(_venus_bauds) / sizeof(_venus_bauds[0]); i++)
		if(_venus_bauds[i] == baud)
			return(i);
	return(1);
}

/* 4 to 10 Hz needs 38400, 20 Hz needs 115200 */
static unsigned long _venus_rate_baud(int rate) {
	if(rate >= 20)
		return(115200);
	if(rate >= 4)
		return(38400);
	return(9600);
}

static void _venus_send_boot(mp_drv_venus638_t *gps, unsigned char kind) {
	unsigned char b[9];

	switch(kind) {
		case VENUS_K_NMEA:
			/* GGA GSA GSV GLL RMC VTG ZDA, SRAM */
			b[0] = 0x08; b[1] = 1; b[2] = 1; b[3] = 1; b[4] = 0; b[5] = 1; b[6] = 0; b[7] = 0; b[8] = 0;
			_venus_send(gps, b, 9, kind);
			break;
		case VENUS_K_VERSION:
			b[0] = 0x02; b[1] = 1;
			_venus_send(gps, b, 2, kind);
			break;
		case VENUS_K_QRATE: b[0] = 0x10; _venus_send(gps, b, 1, kind); break;
		case VENUS_K_QSBAS: b[0] = 0x38; _venus_send(gps, b, 1, kind); break;
		case VENUS_K_QPIN: b[0] = 0x3A; _venus_send(gps, b, 1, kind); break;
		case VENUS_K_QNAV: b[0] = 0x3D; _venus_send(gps, b, 1, kind); break;
		case VENUS_K_QPPS: b[0] = 0x3F; _venus_send(gps, b, 1, kind); break;
	}
}

static void _venus_send_cmd(mp_drv_venus638_t *gps) {
	unsigned char b[MP_DRV_VENUS638_FRAME];
	unsigned long baud;
	int year = 2026;

	memset(b, 0, sizeof(b));
	switch(gps->cmdKind) {
		case VENUS_K_RATE:
			baud = _venus_rate_baud(gps->cmdArg);
			if(gps->step == 0 && gps->baud < baud) {
				b[0] = 0x05; b[1] = 0; b[2] = _venus_baud_code(baud); b[3] = 0;
				gps->arg = (int)baud;
				_venus_send(gps, b, 4, VENUS_K_BAUD);
				return;
			}
			b[0] = 0x0E; b[1] = (unsigned char)gps->cmdArg; b[2] = 0;
			_venus_send(gps, b, 3, VENUS_K_RATE);
			return;
		case VENUS_K_PPS:
			b[0] = 0x3E; b[1] = (unsigned char)gps->cmdArg; b[2] = 0;
			_venus_send(gps, b, 3, gps->cmdKind);
			return;
		case VENUS_K_SBAS:
			b[0] = 0x37; b[1] = (unsigned char)gps->cmdArg; b[2] = 0;
			_venus_send(gps, b, 3, gps->cmdKind);
			return;
		case VENUS_K_NAV:
			b[0] = 0x3C; b[1] = (unsigned char)gps->cmdArg; b[2] = 0;
			_venus_send(gps, b, 3, gps->cmdKind);
			return;
		case VENUS_K_PIN:
			b[0] = 0x39; b[1] = (unsigned char)gps->cmdArg;
			_venus_send(gps, b, 2, gps->cmdKind);
			return;
		case VENUS_K_PSAVE:
			b[0] = 0x0C; b[1] = (unsigned char)gps->cmdArg; b[2] = 0;
			_venus_send(gps, b, 3, gps->cmdKind);
			return;
		case VENUS_K_RESTART:
			/* mode, UTC, then lat/lon/alt as zero: the module needs the full body */
			if(gps->clock.valid == YES)
				year = gps->clock.year;
			b[0] = 0x01;
			b[1] = (unsigned char)gps->cmdArg;
			b[2] = (unsigned char)(year >> 8);
			b[3] = (unsigned char)year;
			b[4] = gps->clock.valid == YES ? gps->clock.month : 1;
			b[5] = gps->clock.valid == YES ? gps->clock.day : 1;
			b[6] = gps->clock.hour;
			b[7] = gps->clock.minute;
			b[8] = gps->clock.second;
			_venus_send(gps, b, 21, gps->cmdKind);
			return;
		case VENUS_K_FACTORY:
			b[0] = 0x04; b[1] = 1;
			_venus_send(gps, b, 2, gps->cmdKind);
			return;
	}
	gps->cmdPending = NO;
}

/* the module comes back on its own defaults: listen again from the start */
static void _venus_rebooted(mp_drv_venus638_t *gps) {
	gps->boot = 0;
	gps->sendAt = mp_clock_ticks() + _venus_ticks(1000);
	gps->heardAt = mp_clock_ticks();
	gps->rateSet = gps->ppsSet = gps->navSet = NO;
	gps->sbasSet = gps->pinningSet = gps->powerSaveSet = NO;
}

/* the frame in flight was answered; ok is NO for a NACK or a timeout */
static void _venus_done(mp_drv_venus638_t *gps, mp_bool_t ok) {
	gps->waiting = NO;
	if(gps->kind <= VENUS_K_QPPS) {
		if(gps->boot < VENUS_BOOT_STEPS)
			gps->boot++;
		return;
	}
	if(gps->kind == VENUS_K_BAUD) {
		if(ok == YES) {
			_venus_baud(gps, (unsigned long)gps->arg);
			gps->step = 1;
			gps->sendAt = mp_clock_ticks() + _venus_ticks(100);
			return;
		}
		mp_printk("Venus638: baud %d refused", gps->arg);
		gps->cmdPending = NO;
		return;
	}
	gps->cmdPending = NO;
	if(ok != YES) {
		mp_printk("Venus638: command 0x%02x refused", gps->waitId);
		return;
	}
	switch(gps->kind) {
		case VENUS_K_RATE: gps->rate = (unsigned char)gps->cmdArg; gps->rateSet = YES; break;
		case VENUS_K_PPS: gps->pps = (unsigned char)gps->cmdArg; gps->ppsSet = YES; break;
		case VENUS_K_SBAS: gps->sbas = gps->cmdArg ? YES : NO; gps->sbasSet = YES; break;
		case VENUS_K_NAV: gps->nav = (unsigned char)gps->cmdArg; gps->navSet = YES; break;
		case VENUS_K_PIN: gps->pinning = gps->cmdArg ? YES : NO; gps->pinningSet = YES; break;
		case VENUS_K_PSAVE: gps->powerSave = gps->cmdArg ? YES : NO; gps->powerSaveSet = YES; break;
		case VENUS_K_RESTART:
		case VENUS_K_FACTORY:
			_venus_rebooted(gps);
			break;
	}
}

static void _venus_register(mp_drv_venus638_t *gps) {
	const char *version = gps->version[0] ? gps->version : NULL;

	if(gps->binding.kernel != gps->kernel)
		return;
	mp_device_identify(gps, VENUS_LABEL, gps->model, gps->serial, version);
	if(mp_device_registry_register(&gps->binding, &gps->gnssDev, MP_DEVICE_GNSS, 0, &gps->gnss, gps->model, gps->serial, version)) {
		gps->gnssDev.label = VENUS_LABEL;
		gps->gnssDev.commands = _venus_commands;
	}
	if(mp_device_registry_register(&gps->binding, &gps->clockDev, MP_DEVICE_CLOCK, 0, &gps->clock, gps->model, gps->serial, version))
		gps->clockDev.label = VENUS_LABEL;
}

static void _venus_frame(mp_drv_venus638_t *gps) {
	unsigned char *f = gps->frame;
	int n = gps->frameLen;

	if(n < 1 || n > MP_DRV_VENUS638_FRAME)
		return;
	switch(f[0]) {
		case 0x83:
		case 0x84:
			if(n >= 2 && gps->waiting == YES && f[1] == gps->waitId)
				_venus_done(gps, f[0] == 0x83 ? YES : NO);
			break;
		case 0x80:
			/* type, kernel, ODM and revision; each a u32 whose low 3 bytes count */
			if(n >= 14) {
				snprintf(gps->version, sizeof(gps->version), "%u.%u.%u/%u.%u.%u/%02u%02u%02u",
					f[3], f[4], f[5], f[7], f[8], f[9], f[11], f[12], f[13]);
				if(gps->heard == YES)
					_venus_register(gps);
			}
			break;
		case 0x86:
			if(n >= 2) { gps->rate = f[1]; gps->rateSet = YES; }
			break;
		case 0xB3:
			if(n >= 2) { gps->sbas = f[1] ? YES : NO; gps->sbasSet = YES; }
			break;
		case 0xB4:
			if(n >= 2) { gps->pinning = f[1] == 1 ? YES : NO; gps->pinningSet = YES; }
			break;
		case 0xB5:
			if(n >= 2) { gps->nav = f[1]; gps->navSet = YES; }
			break;
		case 0xB6:
			if(n >= 2) { gps->pps = f[1]; gps->ppsSet = YES; }
			break;
	}
}

/*
 * A decimal text as value * 10^places, extra digits dropped.
 * NO when it has no digit.
 */
static mp_bool_t _venus_dec(const char *s, int places, long *out) {
	long v = 0;
	int neg = 0, digits = 0, frac = -1;

	if(*s == '-') { neg = 1; s++; }
	for(; *s; s++) {
		if(*s == '.') {
			if(frac >= 0)
				break;
			frac = 0;
			continue;
		}
		if(*s < '0' || *s > '9')
			break;
		if(frac >= 0) {
			if(frac >= places)
				continue;
			frac++;
		}
		v = v * 10 + (*s - '0');
		digits++;
	}
	if(!digits)
		return(NO);
	if(frac < 0)
		frac = 0;
	for(; frac < places; frac++)
		v *= 10;
	*out = neg ? -v : v;
	return(YES);
}

/* ddmm.mmmmm and a hemisphere, as degrees x 10^7 */
static mp_bool_t _venus_coord(const char *s, const char *hemi, long *out) {
	long v, deg, min;

	if(_venus_dec(s, 5, &v) != YES || v < 0)
		return(NO);
	deg = v / 10000000L;
	min = v % 10000000L;
	v = deg * 10000000L + (min * 5) / 3;
	if(*hemi == 'S' || *hemi == 'W')
		v = -v;
	*out = v;
	return(YES);
}

/* hhmmss.sss; the value is hhmmss, -1 when absent */
static long _venus_time(const char *s, mp_device_gnss_t *fix) {
	long v;

	if(_venus_dec(s, 0, &v) != YES || v < 0 || v > 235960L)
		return(-1);
	fix->hour = (unsigned char)(v / 10000);
	fix->minute = (unsigned char)((v / 100) % 100);
	fix->second = (unsigned char)(v % 100);
	return(v);
}

/* 1 Monday to 7 Sunday */
static unsigned char _venus_wday(int y, int m, int d) {
	static const unsigned char t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
	int w;

	if(m < 3)
		y--;
	w = (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
	return((unsigned char)(w ? w : 7));
}

static void _venus_gga(mp_drv_venus638_t *gps, char **f, int n) {
	mp_device_gnss_t *fix = &gps->fix;
	long v;

	if(n < 10)
		return;
	gps->ggaTime = (unsigned long)_venus_time(f[1], fix);
	fix->quality = (unsigned char)(f[6][0] >= '0' && f[6][0] <= '9' ? f[6][0] - '0' : 0);
	if(_venus_dec(f[7], 0, &v) == YES)
		fix->satellites = (unsigned char)v;
	if(fix->quality) {
		_venus_coord(f[2], f[3], &fix->latitude);
		_venus_coord(f[4], f[5], &fix->longitude);
		if(_venus_dec(f[9], 3, &v) == YES)
			fix->altitude = v;
	}
}

/* RMC closes the epoch: the fix and the clock are published */
static void _venus_rmc(mp_drv_venus638_t *gps, char **f, int n) {
	mp_device_gnss_t *fix = &gps->fix;
	mp_device_time_t time;
	long t, v, date;
	mp_bool_t active;

	if(n < 10)
		return;
	t = _venus_time(f[1], fix);
	active = f[2][0] == 'A' ? YES : NO;
	if(active == YES) {
		_venus_coord(f[3], f[4], &fix->latitude);
		_venus_coord(f[5], f[6], &fix->longitude);
		if(_venus_dec(f[7], 3, &v) == YES)
			fix->speed = (unsigned long)(((long long)v * 514444LL) / 1000000LL);
		if(_venus_dec(f[8], 2, &v) == YES && v >= 0 && v < 36000)
			fix->heading = (unsigned short)v;
		if(t < 0 || (unsigned long)t != gps->ggaTime || !fix->quality)
			fix->quality = 1;
	}
	else {
		fix->quality = 0;
		fix->speed = 0;
	}
	if(t >= 0 && _venus_dec(f[9], 0, &date) == YES && date > 0) {
		fix->day = (unsigned char)(date / 10000);
		fix->month = (unsigned char)((date / 100) % 100);
		fix->year = (unsigned short)(2000 + (int)(date % 100));
		if(fix->day < 1 || fix->day > 31 || fix->month < 1 || fix->month > 12)
			fix->year = 0;
	}
	mp_device_gnss_publish(&gps->gnss, fix);
	if(active != YES || fix->year == 0)
		return;
	memset(&time, 0, sizeof(time));
	time.day = fix->day;
	time.month = fix->month;
	time.year = fix->year;
	time.hour = fix->hour;
	time.minute = fix->minute;
	time.second = fix->second;
	time.wday = _venus_wday(time.year, time.month, time.day);
	time.running = YES;
	mp_device_clock_publish(&gps->clock, &time);
}

static mp_bool_t _venus_used(mp_drv_venus638_t *gps, unsigned char id) {
	unsigned char i;

	for(i = 0; i < gps->usedCount; i++)
		if(gps->usedId[i] == id)
			return(YES);
	return(NO);
}

static void _venus_apply_used(mp_drv_venus638_t *gps) {
	unsigned char i, n = 0;

	for(i = 0; i < gps->fix.viewed; i++) {
		gps->fix.sat[i].used = _venus_used(gps, gps->fix.sat[i].id);
		if(gps->fix.sat[i].used == YES)
			n++;
	}
	gps->fix.usedCount = n;
}

static void _venus_gsa(mp_drv_venus638_t *gps, char **f, int n) {
	long v;
	int i;

	gps->usedCount = 0;
	for(i = 0; i < 12 && gps->usedCount < sizeof(gps->usedId); i++) {
		if(3 + i >= n || !f[3 + i][0])
			continue;
		if(_venus_dec(f[3 + i], 0, &v) != YES || v <= 0 || v > 255)
			continue;
		gps->usedId[gps->usedCount++] = (unsigned char)v;
	}
	if(n > 16 && _venus_dec(f[16], 2, &v) == YES && v > 0 && v < 10000)
		gps->fix.hdop = (unsigned short)v;
	_venus_apply_used(gps);
}

static void _venus_sky_sat(mp_drv_venus638_t *gps, char **f, int n, int base) {
	mp_device_gnss_sat_t *sat;
	long v;

	if(base >= n || !f[base][0] || gps->skyCount >= MP_DEVICE_GNSS_SAT_MAX)
		return;
	if(_venus_dec(f[base], 0, &v) != YES || v <= 0 || v > 255)
		return;
	sat = &gps->sky[gps->skyCount++];
	memset(sat, 0, sizeof(*sat));
	sat->id = (unsigned char)v;
	sat->elevation = 0xff;
	sat->azimuth = 0xffff;
	sat->snr = 0xff;
	if(base + 1 < n && f[base + 1][0] && _venus_dec(f[base + 1], 0, &v) == YES && v >= 0 && v <= 90)
		sat->elevation = (unsigned char)v;
	if(base + 2 < n && f[base + 2][0] && _venus_dec(f[base + 2], 0, &v) == YES && v >= 0 && v < 360)
		sat->azimuth = (unsigned short)v;
	if(base + 3 < n && f[base + 3][0] && _venus_dec(f[base + 3], 0, &v) == YES && v >= 0 && v <= 99)
		sat->snr = (unsigned char)v;
	sat->used = _venus_used(gps, sat->id);
}

/* one GSV sentence. The set is committed when the last sentence of it arrives. */
static void _venus_gsv(mp_drv_venus638_t *gps, char **f, int n) {
	long total, seq;
	int i;

	if(n < 4 || _venus_dec(f[1], 0, &total) != YES || _venus_dec(f[2], 0, &seq) != YES)
		return;
	if(total < 1 || seq < 1 || seq > total)
		return;
	if(seq == 1) {
		gps->skyCount = 0;
		gps->skyGot = 0;
		gps->skyTotal = (unsigned char)total;
	}
	else if(seq != gps->skyGot + 1 || total != gps->skyTotal)
		return;
	gps->skyGot = (unsigned char)seq;
	for(i = 0; i < 4; i++)
		_venus_sky_sat(gps, f, n, 4 + i * 4);
	if(seq != total)
		return;
	memset(gps->fix.sat, 0, sizeof(gps->fix.sat));
	if(gps->skyCount)
		memcpy(gps->fix.sat, gps->sky, gps->skyCount * sizeof(gps->sky[0]));
	gps->fix.viewed = gps->skyCount;
	_venus_apply_used(gps);
}

static int _venus_hex(char c) {
	if(c >= '0' && c <= '9')
		return(c - '0');
	if(c >= 'A' && c <= 'F')
		return(c - 'A' + 10);
	if(c >= 'a' && c <= 'f')
		return(c - 'a' + 10);
	return(-1);
}

/* the text between $ and the line end: body*hh */
static void _venus_line(mp_drv_venus638_t *gps) {
	char *f[24];
	char *s = gps->line;
	unsigned char cs = 0;
	int n = 0, hi, lo;
	size_t len;

	gps->line[gps->lineLen] = 0;
	for(; *s && *s != '*'; s++)
		cs ^= (unsigned char)*s;
	if(*s != '*')
		return;
	hi = _venus_hex(s[1]);
	lo = _venus_hex(s[2]);
	if(hi < 0 || lo < 0 || ((hi << 4) | lo) != cs)
		return;
	*s = 0;

	gps->heardAt = mp_clock_ticks();
	if(gps->heard != YES) {
		gps->heard = YES;
		gps->boot = 0;
		gps->sendAt = gps->heardAt;
		mp_printk("Venus638: heard at %lu baud", gps->baud);
		_venus_register(gps);
	}

	s = gps->line;
	f[n++] = s;
	for(; *s && n < 24; s++)
		if(*s == ',') {
			*s = 0;
			f[n++] = s + 1;
		}
	len = strlen(f[0]);
	if(len < 5)
		return;
	gps->fix.system = mp_device_gnss_system(f[0]);
	if(!strcmp(f[0] + len - 3, "GGA"))
		_venus_gga(gps, f, n);
	else if(!strcmp(f[0] + len - 3, "GSA"))
		_venus_gsa(gps, f, n);
	else if(!strcmp(f[0] + len - 3, "GSV"))
		_venus_gsv(gps, f, n);
	else if(!strcmp(f[0] + len - 3, "RMC"))
		_venus_rmc(gps, f, n);
}

static void _venus_parse(mp_drv_venus638_t *gps, unsigned char c) {
	if(c == 0xA0 && gps->state != VENUS_PARSE_BODY && gps->state != VENUS_PARSE_CS && gps->state != VENUS_PARSE_LENH && gps->state != VENUS_PARSE_LENL) {
		gps->state = VENUS_PARSE_A1;
		return;
	}
	switch(gps->state) {
		case VENUS_PARSE_IDLE:
			if(c == '$') {
				gps->lineLen = 0;
				gps->state = VENUS_PARSE_LINE;
			}
			break;
		case VENUS_PARSE_LINE:
			if(c == '\r' || c == '\n') {
				_venus_line(gps);
				gps->state = VENUS_PARSE_IDLE;
			}
			else if(c == '$')
				gps->lineLen = 0;
			else if(gps->lineLen < MP_DRV_VENUS638_LINE - 1)
				gps->line[gps->lineLen++] = (char)c;
			else
				gps->state = VENUS_PARSE_IDLE;
			break;
		case VENUS_PARSE_A1:
			gps->state = c == 0xA1 ? VENUS_PARSE_LENH : VENUS_PARSE_IDLE;
			break;
		case VENUS_PARSE_LENH:
			gps->frameLen = (unsigned short)(c << 8);
			gps->state = VENUS_PARSE_LENL;
			break;
		case VENUS_PARSE_LENL:
			gps->frameLen |= c;
			gps->frameAt = 0;
			gps->frameCs = 0;
			gps->state = gps->frameLen && gps->frameLen < 1024 ? VENUS_PARSE_BODY : VENUS_PARSE_IDLE;
			break;
		case VENUS_PARSE_BODY:
			if(gps->frameAt < MP_DRV_VENUS638_FRAME)
				gps->frame[gps->frameAt] = c;
			gps->frameAt++;
			gps->frameCs ^= c;
			if(gps->frameAt >= gps->frameLen)
				gps->state = VENUS_PARSE_CS;
			break;
		case VENUS_PARSE_CS:
			if(c == gps->frameCs)
				_venus_frame(gps);
			gps->state = VENUS_PARSE_IDLE;
			break;
	}
}

static void _venus_drain(mp_drv_venus638_t *gps) {
	unsigned char c;

	while(gps->tail != gps->head) {
		c = gps->ring[gps->tail];
		gps->tail = (unsigned char)((gps->tail + 1) % MP_DRV_VENUS638_RING);
		_venus_parse(gps, c);
	}
}

static mp_bool_t _venus_wanted(mp_drv_venus638_t *gps, unsigned long now) {
	if(gps->running == YES || gps->cmdPending == YES || gps->waiting == YES)
		return(YES);
	if(gps->heard == YES && gps->boot < VENUS_BOOT_STEPS)
		return(YES);
	return(_venus_after(now, gps->probeUntil) ? NO : YES);
}

static void _venus_listen(mp_drv_venus638_t *gps, mp_bool_t on) {
	if(on == gps->listening)
		return;
	gps->listening = on;
	if(on == YES) {
		mp_power_nodeep(gps->kernel, &gps->power, "Venus638");
		gps->state = VENUS_PARSE_IDLE;
		gps->tail = gps->head;
		gps->heardAt = mp_clock_ticks();
		mp_uart_enable_rx_int(&gps->uart);
	}
	else {
		mp_uart_disable_rx_int(&gps->uart);
		mp_power_release(gps->kernel, &gps->power);
	}
}

/* no valid sentence for a while: the module may sit at another rate */
static void _venus_probe_next(mp_drv_venus638_t *gps, unsigned long now) {
	unsigned long baud;

	if(!_venus_after(now, gps->heardAt + _venus_ticks(VENUS_PROBE_MS)))
		return;
	if(gps->heard == YES) {
		mp_printk("Venus638: silent");
		gps->heard = NO;
	}
	gps->probe = (unsigned char)((gps->probe + 1) % (VENUS_PROBE_COUNT + 1));
	baud = gps->probe == 0 ? gps->boardBaud : _venus_probe[gps->probe - 1];
	_venus_baud(gps, baud);
	gps->heardAt = now;
}

static void _venus_pump(mp_drv_venus638_t *gps, unsigned long now) {
	if(gps->waiting == YES) {
		if(_venus_after(now, gps->waitUntil))
			_venus_done(gps, NO);
		return;
	}
	if(gps->heard != YES || gps->txAt < gps->txLen || !_venus_after(now, gps->sendAt))
		return;
	if(gps->boot < VENUS_BOOT_STEPS) {
		_venus_send_boot(gps, _venus_boot[gps->boot]);
		return;
	}
	if(gps->cmdPending == YES)
		_venus_send_cmd(gps);
}

static MP_TASK(_venus_task) {
	mp_drv_venus638_t *gps = task->user;
	unsigned long now;

	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}
	now = mp_clock_ticks();
	_venus_listen(gps, _venus_wanted(gps, now));
	if(gps->listening != YES) {
		mp_task_delay(task, _venus_ticks(2000));
		return;
	}
	_venus_drain(gps);
	now = mp_clock_ticks();
	_venus_probe_next(gps, now);
	_venus_pump(gps, now);
	mp_task_delay(task, _venus_ticks(200));
}

static mp_drv_venus638_t *_venus_from(mp_device_registry_t *device) {
	if(!device || !device->driver)
		return(NULL);
	return(device->driver->self);
}

static mp_ret_t _venus_queue(mp_device_registry_t *device, unsigned char kind, int arg) {
	mp_drv_venus638_t *gps = _venus_from(device);

	if(!gps || gps->cmdPending == YES)
		return(FALSE);
	gps->cmdKind = kind;
	gps->cmdArg = arg;
	gps->step = 0;
	gps->cmdPending = YES;
	if(gps->task)
		mp_task_wake(gps->task);
	return(TRUE);
}

static mp_ret_t _venus_set_rate(mp_device_registry_t *device, const mp_device_value_t *value) {
	if(!value || value->as.index < 0 || value->as.index >= (int)sizeof(_venus_rates))
		return(FALSE);
	return(_venus_queue(device, VENUS_K_RATE, _venus_rates[value->as.index]));
}

static mp_ret_t _venus_get_rate(mp_device_registry_t *device, mp_device_value_t *value) {
	mp_drv_venus638_t *gps = _venus_from(device);
	int i;

	if(!gps || !value || gps->rateSet != YES)
		return(FALSE);
	for(i = 0; i < (int)sizeof(_venus_rates); i++)
		if(_venus_rates[i] == gps->rate) {
			memset(value, 0, sizeof(*value));
			value->arg = MP_DEVICE_ARG_SELECT;
			value->as.index = i;
			return(TRUE);
		}
	return(FALSE);
}

static mp_ret_t _venus_set_pps(mp_device_registry_t *device, const mp_device_value_t *value) {
	if(!value || value->as.index < 0 || value->as.index > 2)
		return(FALSE);
	return(_venus_queue(device, VENUS_K_PPS, value->as.index));
}

static mp_ret_t _venus_get_select(mp_device_value_t *value, mp_bool_t known, int index) {
	if(!value || known != YES)
		return(FALSE);
	memset(value, 0, sizeof(*value));
	value->arg = MP_DEVICE_ARG_SELECT;
	value->as.index = index;
	return(TRUE);
}

static mp_ret_t _venus_get_bool(mp_device_value_t *value, mp_bool_t known, mp_bool_t on) {
	if(!value || known != YES)
		return(FALSE);
	memset(value, 0, sizeof(*value));
	value->arg = MP_DEVICE_ARG_BOOL;
	value->as.on = on == YES ? 1 : 0;
	return(TRUE);
}

static mp_ret_t _venus_get_pps(mp_device_registry_t *device, mp_device_value_t *value) {
	mp_drv_venus638_t *gps = _venus_from(device);

	return(gps && gps->pps <= 2 ? _venus_get_select(value, gps->ppsSet, gps->pps) : FALSE);
}

static mp_ret_t _venus_set_sbas(mp_device_registry_t *device, const mp_device_value_t *value) {
	return(value ? _venus_queue(device, VENUS_K_SBAS, value->as.on ? 1 : 0) : FALSE);
}

static mp_ret_t _venus_get_sbas(mp_device_registry_t *device, mp_device_value_t *value) {
	mp_drv_venus638_t *gps = _venus_from(device);

	return(gps ? _venus_get_bool(value, gps->sbasSet, gps->sbas) : FALSE);
}

static mp_ret_t _venus_set_nav(mp_device_registry_t *device, const mp_device_value_t *value) {
	if(!value || value->as.index < 0 || value->as.index > 1)
		return(FALSE);
	return(_venus_queue(device, VENUS_K_NAV, value->as.index));
}

static mp_ret_t _venus_get_nav(mp_device_registry_t *device, mp_device_value_t *value) {
	mp_drv_venus638_t *gps = _venus_from(device);

	return(gps && gps->nav <= 1 ? _venus_get_select(value, gps->navSet, gps->nav) : FALSE);
}

static mp_ret_t _venus_set_pin(mp_device_registry_t *device, const mp_device_value_t *value) {
	return(value ? _venus_queue(device, VENUS_K_PIN, value->as.on ? 1 : 0) : FALSE);
}

static mp_ret_t _venus_get_pin(mp_device_registry_t *device, mp_device_value_t *value) {
	mp_drv_venus638_t *gps = _venus_from(device);

	return(gps ? _venus_get_bool(value, gps->pinningSet, gps->pinning) : FALSE);
}

static mp_ret_t _venus_set_psave(mp_device_registry_t *device, const mp_device_value_t *value) {
	return(value ? _venus_queue(device, VENUS_K_PSAVE, value->as.on ? 1 : 0) : FALSE);
}

static mp_ret_t _venus_get_psave(mp_device_registry_t *device, mp_device_value_t *value) {
	mp_drv_venus638_t *gps = _venus_from(device);

	return(gps ? _venus_get_bool(value, gps->powerSaveSet, gps->powerSave) : FALSE);
}

static mp_ret_t _venus_set_restart(mp_device_registry_t *device, const mp_device_value_t *value) {
	if(!value || value->as.index < 0 || value->as.index > 2)
		return(FALSE);
	return(_venus_queue(device, VENUS_K_RESTART, value->as.index + 1));
}

static mp_ret_t _venus_set_factory(mp_device_registry_t *device, const mp_device_value_t *value) {
	(void)value;
	return(_venus_queue(device, VENUS_K_FACTORY, 0));
}

static const mp_device_command_t _venus_commands[VENUS_COMMANDS] = {
	{ "rate", "Update rate", MP_DEVICE_ARG_SELECT, 0, 0, _venus_rate_choice, "Hz", _venus_set_rate, _venus_get_rate },
	{ "pps", "1PPS", MP_DEVICE_ARG_SELECT, 0, 0, _venus_pps_choice, NULL, _venus_set_pps, _venus_get_pps },
	{ "sbas", "SBAS", MP_DEVICE_ARG_BOOL, 0, 0, NULL, NULL, _venus_set_sbas, _venus_get_sbas },
	{ "nav-mode", "Navigation mode", MP_DEVICE_ARG_SELECT, 0, 0, _venus_nav_choice, NULL, _venus_set_nav, _venus_get_nav },
	{ "pinning", "Position pinning", MP_DEVICE_ARG_BOOL, 0, 0, NULL, NULL, _venus_set_pin, _venus_get_pin },
	{ "power-save", "Power save", MP_DEVICE_ARG_BOOL, 0, 0, NULL, NULL, _venus_set_psave, _venus_get_psave },
	{ "restart", "Restart", MP_DEVICE_ARG_SELECT, 0, 0, _venus_restart_choice, NULL, _venus_set_restart, NULL },
	{ "factory", "Factory reset", MP_DEVICE_ARG_NONE, 0, 0, NULL, NULL, _venus_set_factory, NULL },
	{ NULL, NULL, MP_DEVICE_ARG_NONE, 0, 0, NULL, NULL, NULL, NULL }
};

static void _venus_start(void *self) {
	mp_drv_venus638_t *gps = self;

	gps->running = YES;
	gps->heardAt = mp_clock_ticks();
	if(gps->task)
		mp_task_wake(gps->task);
}

static void _venus_stop(void *self) {
	mp_drv_venus638_t *gps = self;

	gps->running = NO;
	if(gps->task)
		mp_task_wake(gps->task);
}

/*
 * Keep the devices of a receiver heard lately. A stopped one is kept and
 * listened to for a while, so the next scan knows whether it is still there.
 * Leaving them stale drops them from the registry.
 */
static void _venus_scan(mp_device_driver_t *driver, void *self) {
	mp_drv_venus638_t *gps = self;
	unsigned long now = mp_clock_ticks();

	(void)driver;
	if(gps->running != YES) {
		gps->probeUntil = now + _venus_ticks(VENUS_SCAN_MS);
		if(gps->task)
			mp_task_wake(gps->task);
	}
	if(gps->heard != YES)
		return;
	if(gps->running == YES && _venus_after(now, gps->heardAt + _venus_ticks(VENUS_GONE_MS)))
		return;
	_venus_register(gps);
}

static void _venus_bind(mp_drv_venus638_t *gps) {
	static const mp_device_driver_ops_t ops = {
		"gnss/venus638", VENUS_LABEL, "1.0.0",
		(unsigned int)((1u << MP_DEVICE_GNSS) | (1u << MP_DEVICE_CLOCK)),
		NULL, NULL, _venus_scan, _venus_start, _venus_stop
	};

	if(gps->binding.kernel == gps->kernel)
		return;
	if(mp_device_driver_register(gps->kernel, &gps->binding, &ops, gps, NULL) != TRUE)
		mp_printk("Venus638: can not register the driver");
}

mp_ret_t mp_drv_venus638_init(mp_kernel_t *kernel, mp_drv_venus638_t *gps, mp_options_t *options, char *who) {
	const char *model;
	const char *serial;
	char *value;

	memset(gps, 0, sizeof(*gps));
	gps->kernel = kernel;
	if(mp_device_options_identity(options, &model, &serial) != TRUE) {
		mp_printk("Venus638: model and serial are required");
		return(FALSE);
	}
	gps->model = model;
	gps->serial = serial;
	value = mp_options_get(options, "baudRate");
	gps->boardBaud = value ? strtoul(value, NULL, 10) : 9600;

	if(mp_uart_open(kernel, &gps->uart, options, who ? who : "Venus638") != TRUE) {
		mp_printk("Venus638: can not open the UART");
		return(FALSE);
	}
	gps->uartOpen = YES;
	gps->baud = gps->boardBaud;
	gps->uart.user = gps;
	gps->uart.onRead = _venus_rx;
	gps->uart.onWrite = _venus_tx;
	mp_uart_disable_rx_int(&gps->uart);

	_venus_bind(gps);
	if(gps->binding.kernel != gps->kernel) {
		mp_drv_venus638_fini(gps);
		return(FALSE);
	}
	gps->clock.user = gps;
	gps->probeUntil = mp_clock_ticks() + _venus_ticks(VENUS_WINDOW_MS);
	gps->task = mp_task_create(&kernel->tasks, "Venus638", _venus_task, gps, 0);
	if(!gps->task) {
		mp_printk("Venus638: no task");
		mp_drv_venus638_fini(gps);
		return(FALSE);
	}
	mp_printk("Venus638: listening at %lu baud", gps->baud);
	return(TRUE);
}

void mp_drv_venus638_fini(mp_drv_venus638_t *gps) {
	if(!gps || !gps->kernel)
		return;
	if(gps->task) {
		mp_task_destroy(gps->task);
		gps->task = NULL;
	}
	if(gps->uartOpen == YES) {
		mp_uart_disable_tx_int(&gps->uart);
		mp_uart_disable_rx_int(&gps->uart);
		mp_uart_close(&gps->uart);
		gps->uartOpen = NO;
	}
	if(gps->listening == YES) {
		mp_power_release(gps->kernel, &gps->power);
		gps->listening = NO;
	}
	if(gps->gnssDev.driver)
		mp_device_registry_unregister(&gps->gnssDev);
	if(gps->clockDev.driver)
		mp_device_registry_unregister(&gps->clockDev);
	if(gps->binding.kernel)
		mp_device_driver_unregister(gps->kernel, &gps->binding);
}

#ifdef SUPPORT_COMMON_DEVICE
static mp_ret_t _venus_device_open(mp_kernel_t *kernel, void *self, mp_options_t *options, char *who) {
	return(mp_drv_venus638_init(kernel, self, options, who));
}

static void _venus_device_close(void *self) {
	mp_drv_venus638_fini(self);
}

const mp_device_ops_t mp_drv_venus638_device = {
	"VENUS638", MP_DEVICE_GNSS, _venus_device_open, _venus_device_close, NULL, NULL,
	VENUS_LABEL, "1.0.0"
};
#endif

#endif
