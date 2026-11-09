/* Host test of the term FSM. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>

#define MP_HOST_TASK
#define SUPPORT_COMMON_TERM
#define MP_TERM_HAVE_VT100
#define MP_TERM_HAVE_XTERM
#define MP_TERM_HAVE_OSC
#define MP_TERM_COLOR 3

#include <mp.h>
#include "../term.h"
/* the stand-in mp.h pulls list.h and a task kernel when MP_HOST_TASK is set */
#include "../term.c"
#include "../parse.c"
#include "../color.c"
#include "../drivers/vt100.c"
#include "../drivers/vt102.c"
#include "../drivers/xterm.c"
#include "../drivers/osc.c"
#include "../drivers/builtins.c"

static int failed;

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "fail: %s\n", msg);
}

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

static mp_term_t term;
static unsigned char sink[256];
static int sinkLen;

static void _write(void *user, const unsigned char *bytes, int n) {
	(void)user;
	if(n > 0 && sinkLen + n <= (int)sizeof(sink)) {
		memcpy(sink + sinkLen, bytes, (size_t)n);
		sinkLen += n;
	}
}

static void _reset(void) {
	sinkLen = 0;
	memset(sink, 0, sizeof(sink));
	mp_test_clock(1000);
	mp_term_init(NULL, &term, _write, NULL, MP_TERM_CAP_ALL, MP_TERM_COLOR_TRUE);
}

static int read_one(mp_term_event_t *ev) {
	/* drain until an event or nothing */
	if(!mp_term_read(&term, ev))
		return(0);
	return(1);
}

int main(void) {
	mp_term_event_t ev;

	/* text */
	_reset();
	mp_term_rx(&term, 'a');
	mp_term_rx(&term, 'b');
	expect(read_one(&ev) == 1 && ev.type == MP_TERM_EVENT_TEXT && ev.len == 2 && ev.bytes[0] == 'a' && ev.bytes[1] == 'b', "text run");

	/* arrow up: ESC [ A */
	_reset();
	mp_term_rx(&term, 0x1b);
	mp_term_rx(&term, '[');
	mp_term_rx(&term, 'A');
	expect(read_one(&ev) == 1 && ev.type == MP_TERM_EVENT_KEY && ev.key == MP_TERM_KEY_UP, "arrow up");

	/* arrow down */
	_reset();
	mp_term_rx(&term, 0x1b);
	mp_term_rx(&term, '[');
	mp_term_rx(&term, 'B');
	expect(read_one(&ev) == 1 && ev.type == MP_TERM_EVENT_KEY && ev.key == MP_TERM_KEY_DOWN, "arrow down");

	/* home: ESC [ H (no params) */
	_reset();
	mp_term_rx(&term, 0x1b);
	mp_term_rx(&term, '[');
	mp_term_rx(&term, 'H');
	expect(read_one(&ev) == 1 && ev.type == MP_TERM_EVENT_KEY && ev.key == MP_TERM_KEY_HOME, "home");

	/* delete: ESC [ 3 ~ */
	_reset();
	mp_term_rx(&term, 0x1b);
	mp_term_rx(&term, '[');
	mp_term_rx(&term, '3');
	mp_term_rx(&term, '~');
	expect(read_one(&ev) == 1 && ev.type == MP_TERM_EVENT_KEY && ev.key == MP_TERM_KEY_DELETE, "delete");

	/* page up: ESC [ 5 ~ */
	_reset();
	mp_term_rx(&term, 0x1b);
	mp_term_rx(&term, '[');
	mp_term_rx(&term, '5');
	mp_term_rx(&term, '~');
	expect(read_one(&ev) == 1 && ev.type == MP_TERM_EVENT_KEY && ev.key == MP_TERM_KEY_PAGE_UP, "page up");

	/* enter, backspace, tab */
	_reset();
	mp_term_rx(&term, '\r');
	expect(read_one(&ev) == 1 && ev.type == MP_TERM_EVENT_KEY && ev.key == MP_TERM_KEY_ENTER, "enter");
	_reset();
	mp_term_rx(&term, 0x7f);
	expect(read_one(&ev) == 1 && ev.type == MP_TERM_EVENT_KEY && ev.key == MP_TERM_KEY_BACKSPACE, "backspace");

	/* OSC title: ESC ] 0 ; hi BEL */
	_reset();
	mp_term_rx(&term, 0x1b);
	mp_term_rx(&term, ']');
	mp_term_rx(&term, '0');
	mp_term_rx(&term, ';');
	mp_term_rx(&term, 'h');
	mp_term_rx(&term, 'i');
	mp_term_rx(&term, 0x07);
	expect(read_one(&ev) == 1 && ev.type == MP_TERM_EVENT_OSC && ev.len == 2 && ev.bytes[0] == 'h' && ev.bytes[1] == 'i', "osc title");

	/* OSC with ST: ESC ] 2 ; x ESC \ */
	_reset();
	mp_term_rx(&term, 0x1b);
	mp_term_rx(&term, ']');
	mp_term_rx(&term, '2');
	mp_term_rx(&term, ';');
	mp_term_rx(&term, 'x');
	mp_term_rx(&term, 0x1b);
	mp_term_rx(&term, '\\');
	expect(read_one(&ev) == 1 && ev.type == MP_TERM_EVENT_OSC && ev.len == 1 && ev.bytes[0] == 'x', "osc st");

	/* mouse 1006: ESC [ < 0 ; 10 ; 20 M */
	_reset();
	mp_term_rx(&term, 0x1b);
	mp_term_rx(&term, '[');
	mp_term_rx(&term, '<');
	mp_term_rx(&term, '0');
	mp_term_rx(&term, ';');
	mp_term_rx(&term, '1');
	mp_term_rx(&term, '0');
	mp_term_rx(&term, ';');
	mp_term_rx(&term, '2');
	mp_term_rx(&term, '0');
	mp_term_rx(&term, 'M');
	expect(read_one(&ev) == 1 && ev.type == MP_TERM_EVENT_MOUSE && ev.x == 10 && ev.y == 20 && (ev.value & 7) == 0, "mouse press");

	/* mouse release: ... m */
	_reset();
	mp_term_rx(&term, 0x1b);
	mp_term_rx(&term, '[');
	mp_term_rx(&term, '<');
	mp_term_rx(&term, '0');
	mp_term_rx(&term, ';');
	mp_term_rx(&term, '5');
	mp_term_rx(&term, ';');
	mp_term_rx(&term, '6');
	mp_term_rx(&term, 'm');
	expect(read_one(&ev) == 1 && ev.type == MP_TERM_EVENT_MOUSE && ev.x == 5 && ev.y == 6 && (ev.value & 0x10) != 0, "mouse release");

	/* paste start: ESC [ ? 2004 h */
	_reset();
	mp_term_rx(&term, 0x1b);
	mp_term_rx(&term, '[');
	mp_term_rx(&term, '?');
	mp_term_rx(&term, '2');
	mp_term_rx(&term, '0');
	mp_term_rx(&term, '0');
	mp_term_rx(&term, '4');
	mp_term_rx(&term, 'h');
	expect(read_one(&ev) == 1 && ev.type == MP_TERM_EVENT_PASTE && ev.value == 1, "paste start");

	/* paste end: ESC [ ? 2004 l */
	_reset();
	mp_term_rx(&term, 0x1b);
	mp_term_rx(&term, '[');
	mp_term_rx(&term, '?');
	mp_term_rx(&term, '2');
	mp_term_rx(&term, '0');
	mp_term_rx(&term, '0');
	mp_term_rx(&term, '4');
	mp_term_rx(&term, 'l');
	expect(read_one(&ev) == 1 && ev.type == MP_TERM_EVENT_PASTE && ev.value == 0, "paste end");

	/* resize reply: ESC [ 8 ; 24 ; 80 t */
	_reset();
	mp_term_rx(&term, 0x1b);
	mp_term_rx(&term, '[');
	mp_term_rx(&term, '8');
	mp_term_rx(&term, ';');
	mp_term_rx(&term, '2');
	mp_term_rx(&term, '4');
	mp_term_rx(&term, ';');
	mp_term_rx(&term, '8');
	mp_term_rx(&term, '0');
	mp_term_rx(&term, 't');
	expect(read_one(&ev) == 1 && ev.type == MP_TERM_EVENT_RESIZE && ev.x == 24 && ev.y == 80, "resize");

	/* a CPR is the glass only after a corner probe: ESC [ 40 ; 100 R */
	_reset();
	mp_term_query(&term);
	mp_term_rx(&term, 0x1b);
	mp_term_rx(&term, '[');
	mp_term_rx(&term, '4');
	mp_term_rx(&term, '0');
	mp_term_rx(&term, ';');
	mp_term_rx(&term, '1');
	mp_term_rx(&term, '0');
	mp_term_rx(&term, '0');
	mp_term_rx(&term, 'R');
	expect(read_one(&ev) == 1 && ev.type == MP_TERM_EVENT_RESIZE && ev.x == 40 && ev.y == 100, "probe");
	expect(term.rows == 40 && term.cols == 100, "probe stored");

	/* the UTF-8 probe answers first: column 2 means one cell for 3 bytes */
	{
		static const char *replies[2] = { "\033[1;2R\033[30;90R", "\033[1;4R\033[30;90R" };
		int k, i;

		for(k = 0; k < 2; k++) {
			_reset();
			mp_term_query_utf8(&term);
			mp_term_query(&term);
			for(i = 0; replies[k][i]; i++)
				mp_term_rx(&term, (unsigned char)replies[k][i]);
			expect(term.utf8Known == YES && term.utf8 == (k == 0 ? YES : NO), "utf8 answer");
			expect(read_one(&ev) == 1 && ev.type == MP_TERM_EVENT_RESIZE && ev.x == 30 && ev.y == 90,
				"size after utf8");
		}
	}

	/* a dropped UTF-8 probe: the size report must stay the size */
	_reset();
	mp_term_query_utf8(&term);
	mp_term_query(&term);
	{
		static const char size[] = "\033[36;111R";
		int i;

		for(i = 0; size[i]; i++)
			mp_term_rx(&term, (unsigned char)size[i]);
	}
	expect(term.utf8Known != YES, "size is not a utf8 answer");
	expect(read_one(&ev) == 1 && ev.type == MP_TERM_EVENT_RESIZE && ev.x == 36 && ev.y == 111,
		"size kept");

	/* ESC then a digit is Alt+digit, not a lone Escape plus text */
	_reset();
	mp_term_rx(&term, 0x1b);
	mp_term_rx(&term, '1');
	expect(read_one(&ev) == 1 && ev.type == MP_TERM_EVENT_KEY && ev.key == MP_TERM_KEY_ALT('1'), "alt+1");
	expect(read_one(&ev) == 0, "alt+1 has no text");

	/* lone Escape fires after the timeout, not before */
	_reset();
	mp_term_rx(&term, 0x1b);
	expect(read_one(&ev) == 0, "escape not immediate");
	mp_test_clock(1000 + 49);
	expect(read_one(&ev) == 0, "escape before timeout");
	mp_test_clock(1000 + 51);
	expect(read_one(&ev) == 1 && ev.type == MP_TERM_EVENT_KEY && ev.key == MP_TERM_KEY_ESCAPE, "lone escape after timeout");
	/* the key that follows is itself, not a second Escape that would close the menu */
	mp_term_rx(&term, 0x1b);
	mp_term_rx(&term, '[');
	mp_term_rx(&term, 'B');
	expect(read_one(&ev) == 1 && ev.type == MP_TERM_EVENT_KEY && ev.key == MP_TERM_KEY_DOWN, "down after escape");
	expect(read_one(&ev) == 0, "down after escape has no extra key");

	/* SS3 cursor keys are keys, not Escape then text */
	_reset();
	mp_term_rx(&term, 0x1b);
	mp_term_rx(&term, 'O');
	mp_term_rx(&term, 'B');
	expect(read_one(&ev) == 1 && ev.type == MP_TERM_EVENT_KEY && ev.key == MP_TERM_KEY_DOWN, "ss3 down");
	expect(read_one(&ev) == 0, "ss3 down has no extra key");

	/* ESC then a real sequence clears the lone-escape */
	_reset();
	mp_term_rx(&term, 0x1b);
	mp_test_clock(1000 + 99);
	mp_term_rx(&term, '[');
	mp_term_rx(&term, 'A');
	expect(read_one(&ev) == 1 && ev.type == MP_TERM_EVENT_KEY && ev.key == MP_TERM_KEY_UP, "escape cleared by sequence");

	if(failed) {
		fprintf(stderr, "%d failed\n", failed);
		return(1);
	}
	printf("ok\n");
	return(0);
}
