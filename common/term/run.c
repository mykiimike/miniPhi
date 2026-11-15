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

/**
 * @defgroup mpCommonTerm Terminal codec
 * @ingroup mpCommon
 *
 * The term task. One instance owns the serial line: it drains bytes,
 * probes the glass once a second, and paints the shell. The probe is the
 * last thing written on a wake, so a paint cannot move the cursor the
 * report is about to measure.
 * @{
 */

#include <mp.h>

#ifdef SUPPORT_COMMON_TERM
#ifdef SUPPORT_COMMON_SERIAL

#include <stdarg.h>
#include <string.h>
#include <term/term.h>

static unsigned long _mp_term_run_ms(unsigned long ms) {
	unsigned long ticks = (mp_clock_tick_hz() * ms) / 1000UL;

	return(ticks ? ticks : 1);
}

/*
 * Minicom decodes UTF-8 per read() and reads 128 bytes at a time: a
 * character cut by a read counts as several columns and every cell after
 * it lands to the right. Reads end on USB packets, so with UTF-8 lines
 * each packet is full and no character crosses one. NUL pads; terminals
 * drop it.
 */
static void _mp_term_run_pad(mp_term_ui_t *ui, int to) {
	while(ui->outLen % MP_TERM_UI_PACKET && ui->outLen < to)
		ui->out[ui->outLen++] = 0;
}

/* everything a wake writes leaves in one go: the USB port waits on the
 * host for each packet, so many small writes cost many waits */
/* 1 when the bytes left, 0 when the host did not take them */
/* a host that stopped reading: later sends do not wait out the queue again */
static mp_bool_t _mp_term_run_stalled;

static int _mp_term_run_send(mp_term_ui_t *ui) {
	int n = ui->outLen;
	unsigned long t0;

	if(n > 0 && ui->utf8 == YES)
		_mp_term_run_pad(ui, MP_TERM_UI_OUT);
	/* A repaint is far bigger than the queue. A chunk the queue refuses
	 * would be lost whole and cut a sequence in two: wait for the
	 * interrupt to drain it. */
	if(n > 0 && ui->term && ui->term->line) {
		mp_serial_t *line = (mp_serial_t *)ui->term->line;

		if(_mp_term_run_stalled == YES && mp_serial_pending(line) > 0) {
			ui->outLen = 0;
			mp_term_ui_unsent(ui);
			return(0);
		}
		if(_mp_term_run_stalled == YES) {
			unsigned char cancel[MP_TERM_UI_PACKET];

			memset(cancel, 0, sizeof(cancel));
			cancel[0] = 0x18;
			mp_serial_write(line, cancel, (int)sizeof(cancel));
		}
		_mp_term_run_stalled = NO;
		t0 = mp_clock_ticks();
		while(mp_serial_write(line, ui->out, ui->outLen) != TRUE) {
			if(mp_clock_ticks() - t0 >= _mp_term_run_ms(100)) {
				_mp_term_run_stalled = YES;
				ui->outLen = 0;
				mp_term_ui_unsent(ui);
				return(0);
			}
		}
		if(mp_serial_delivered(line) != YES) {
			ui->outLen = 0;
			mp_term_ui_unsent(ui);
			return(0);
		}
	}
	ui->outLen = 0;
	return(1);
}

static void _mp_term_run_write(void *user, const unsigned char *bytes, int n) {
	mp_term_ui_t *ui = user;
	int i;

	for(i = 0; i < n; i++) {
		unsigned char c = bytes[i];

		/* the UTF-8 probe goes out before utf8 is known: align it too */
		if(c >= 0xc0) {
			int len = c >= 0xf0 ? 4 : c >= 0xe0 ? 3 : 2;
			int at = ui->outLen % MP_TERM_UI_PACKET;

			if(at + len > MP_TERM_UI_PACKET)
				_mp_term_run_pad(ui, MP_TERM_UI_OUT);
		}
		if(ui->outLen >= MP_TERM_UI_OUT)
			_mp_term_run_send(ui);
		ui->out[ui->outLen++] = c;
	}
}

static void _mp_term_run_printk(void *user, char *fmt, ...) {
	char buf[MP_TERM_UI_LOG_LEN];
	va_list args;
	int n;

	if(!fmt)
		return;
	va_start(args, fmt);
	n = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	if(n < 0)
		return;
	mp_term_ui_log(user, buf);
}

/* Bytes already received are not a pause: an Escape they follow is not
 * a lone one, however long the events before them took to handle. */
static void _mp_term_run_events(mp_term_ui_t *ui, int more) {
	mp_term_event_t event;

	for(;;) {
		if(more && ui->term->escape == 1)
			ui->term->escapeTick = mp_clock_ticks();
		if(!mp_term_read(ui->term, &event))
			return;
		mp_term_ui_event(ui, &event);
	}
}

static void _mp_term_run_drain(mp_term_ui_t *ui) {
	mp_serial_t *serial;
	mp_circular_t *cir;

	if(!ui->term || !ui->term->line)
		return;
	serial = (mp_serial_t *)ui->term->line;
	cir = &serial->rxCir;
	mp_uart_rx_poll(serial->uart);
	while(cir->totalSize > 0) {
		mp_circular_buffer_t *buffer = mp_circular_read(cir);
		int index;

		if(!buffer)
			return;
		for(index = 0; index < buffer->size; index++) {
			mp_term_rx(ui->term, buffer->data[index]);
			_mp_term_run_events(ui, index + 1 < buffer->size || cir->totalSize > 0);
		}
		mp_mem_free(serial->kernel, buffer);
	}
	_mp_term_run_events(ui, 0);
}

/* bytes still queued from the previous host must not dismiss the splash */
static void _mp_term_run_drop(mp_term_ui_t *ui) {
	mp_serial_t *serial;
	mp_circular_t *cir;

	if(!ui->term || !ui->term->line)
		return;
	serial = (mp_serial_t *)ui->term->line;
	cir = &serial->rxCir;
	while(cir->totalSize > 0) {
		mp_circular_buffer_t *buffer = mp_circular_read(cir);

		if(!buffer)
			return;
		mp_mem_free(serial->kernel, buffer);
	}
}

static MP_TASK(_mp_term_task) {
	mp_term_ui_t *ui = task->user;
	unsigned long wait, frame, second;

	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}
	if(!ui || !ui->term)
		return;

	second = _mp_term_run_ms(1000);
	/* the board stays up across a cable pull, so the splash runs again */
	if(ui->term->line && mp_serial_host_back((mp_serial_t *)ui->term->line) == YES) {
		_mp_term_run_drop(ui);
		mp_term_ui_replay(ui);
	}
	_mp_term_run_drain(ui);
	if(_mp_term_run_stalled == YES && ui->term->line &&
			mp_serial_pending((mp_serial_t *)ui->term->line) > 0) {
		if(task->signal != MP_TASK_SIG_STOP && task->signal != MP_TASK_SIG_DEAD)
			mp_task_delay(task, 1);
		return;
	}
	mp_term_ui_flush(ui);
	/* Ask once a second, and sooner until both the size and the UTF-8 answer
	 * have landed: a reset drops the first burst. */
	if(ui->sized != YES || ui->term->utf8Known != YES)
		second = _mp_term_run_ms(200);
	if(mp_clock_ticks() - ui->term->probeAt >= second) {
		ui->term->probe = NO;
		ui->term->probeUtf8 = NO;
		/* the probe is an ESC sequence: send it alone so it does not cross a
		 * 63-byte USB packet and arrive as a lone ESC + literal text */
		_mp_term_run_send(ui);
		if(ui->term->utf8Known != YES)
			mp_term_query_utf8(ui->term);
		mp_term_query(ui->term);
		mp_term_ui_note_probe(ui);
		ui->term->probeAt = mp_clock_ticks();
	}
	if(!_mp_term_run_send(ui)) {
		/* the paint is still owed; try again as soon as the host acks */
		if(task->signal != MP_TASK_SIG_STOP && task->signal != MP_TASK_SIG_DEAD)
			mp_task_delay(task, 1);
		return;
	}
	/* the host just took a packet, so the line is alive: a host that never
	 * answers the probe (minicom) still gets the card, without a blind burst
	 * that wedges the IN fifo before anyone drains it. */
	if(ui->term->line && mp_serial_sync((mp_serial_t *)ui->term->line) == YES &&
			ui->mode == MP_TERM_UI_MODE_SPLASH && ui->sized != YES)
		mp_term_ui_host_ready(ui);

	wait = second - (mp_clock_ticks() - ui->term->probeAt);
	if(wait == 0 || wait > second)
		wait = second;
	frame = mp_term_waiting(ui->term);
	if(frame && frame < wait)
		wait = frame;
	frame = mp_term_ui_waiting(ui);
	if(frame && frame < wait)
		wait = frame;
	/* a printk that landed while this wake ran would wait a whole second */
	if(ui->logHead != ui->flushedLog)
		wait = 1;
	if(ui->open == ui->logNode && ui->logSeq != ui->paintedLog)
		wait = 1;
	/* a wake leaves the task pending; the delay is what puts it back to sleep */
	if(task->signal != MP_TASK_SIG_STOP && task->signal != MP_TASK_SIG_DEAD)
		mp_task_delay(task, wait);
}

mp_ret_t mp_term_ui_start(mp_term_ui_t *ui, struct mp_serial_s *serial,
		const mp_term_banner_line_t *lines, int n) {
	mp_serial_t *port = (mp_serial_t *)serial;
	mp_task_t *task;

	if(!ui || !ui->term || !port)
		return(FALSE);
	if(ui->term->task)
		return(TRUE);
	if(mp_serial_terminal(port) == FALSE)
		return(FALSE);
	mp_printk_set(_mp_term_run_printk, ui);
	mp_term_init(port->kernel, ui->term, _mp_term_run_write, ui,
		MP_TERM_CAP_ALL, MP_TERM_COLOR_16);
	ui->term->line = serial;
	ui->outLen = 0;
	/* clean the glass, then measure it; the card waits for the answer */
	mp_term_ui_welcome(ui, lines, n);
	/* the probe is an ESC sequence: send the welcome first so the probe
	 * leaves in its own packet and does not cross a 63-byte boundary */
	_mp_term_run_send(ui);
	mp_term_query_utf8(ui->term);
	mp_term_query(ui->term);
	mp_term_ui_note_probe(ui);
	ui->term->probeAt = mp_clock_ticks();
	_mp_term_run_send(ui);
	task = mp_task_create(&port->kernel->tasks, "Term", _mp_term_task, ui, 1);
	if(!task) {
		if(mp_printk_call == _mp_term_run_printk && mp_printk_user == ui)
			mp_printk_unset();
		return(FALSE);
	}
	ui->term->task = task;
	port->reader = task;
	return(TRUE);
}

void mp_term_ui_stop(mp_term_ui_t *ui) {
	mp_serial_t *port;

	if(!ui || !ui->term)
		return;
	port = (mp_serial_t *)ui->term->line;
	if(ui->term->task) {
		if(port && port->reader == ui->term->task)
			port->reader = NULL;
		mp_task_destroy(ui->term->task);
		ui->term->task = NULL;
	}
	if(mp_printk_call == _mp_term_run_printk && mp_printk_user == ui)
		mp_printk_unset();
	mp_term_style(ui->term, MP_TERM_STYLE_RESET);
	mp_term_graphic(ui->term, 0);
	mp_term_region(ui->term, 0, 0);
	mp_term_alt(ui->term, 0);
	mp_term_cursor(ui->term, 1);
	_mp_term_run_send(ui);
	ui->mode = MP_TERM_UI_MODE_SPLASH;
}

#endif
#endif

/**@}*/
