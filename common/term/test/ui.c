/* Host test of the term shell. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define MP_HOST_TASK
#define SUPPORT_COMMON_TERM
#define MP_TERM_HAVE_VT100
#define MP_TERM_HAVE_VT102
#define MP_TERM_HAVE_XTERM
#define MP_TERM_HAVE_OSC
#define MP_TERM_COLOR 3

#include <mp.h>
#include "../term.h"
#include "../term.c"
#include "../parse.c"
#include "../color.c"
#include "../drivers/vt100.c"
#include "../drivers/vt102.c"
#include "../drivers/xterm.c"
#include "../drivers/osc.c"
#include "../drivers/builtins.c"
#include "../ui.c"
#include "../pages/console.c"
#include "../pages/messages.c"

static int failed;

static void dump_rows(void);

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "fail: %s\n", msg);
	dump_rows();
}

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

void mp_task_wake(mp_task_t *task) {
	(void)task;
}

static mp_term_t term;
static mp_term_ui_t ui;
static unsigned char sink[131072];
static int sinkLen;
static int sinkDrop;

static void _write(void *user, const unsigned char *bytes, int n) {
	(void)user;
	if(n <= 0)
		return;
	if(sinkLen + n <= (int)sizeof(sink)) {
		memcpy(sink + sinkLen, bytes, (size_t)n);
		sinkLen += n;
	}
	else
		sinkDrop += n;
}

static int row_has(int row, const char *s) {
	int n = (int)strlen(s);
	int c, i;

	if(n < 1 || row < 0 || row >= ui.rows)
		return(0);
	for(c = 0; c + n <= ui.cols; c++) {
		for(i = 0; i < n; i++) {
			int ch, fg, bg;

			if(!mp_term_ui_at(&ui, row, c + i, &ch, &fg, &bg) || ch != (unsigned char)s[i])
				break;
		}
		if(i == n)
			return(1);
	}
	return(0);
}

static void dump_rows(void) {
	int row, c, ch, fg, bg;

	for(row = 0; row < ui.rows && row < 6; row++) {
		fprintf(stderr, "  r%d ", row);
		for(c = 0; c < ui.cols && c < 78; c++) {
			if(!mp_term_ui_at(&ui, row, c, &ch, &fg, &bg) || ch < 32 || ch > 126)
				fputc('.', stderr);
			else
				fputc(ch, stderr);
		}
		fputc('\n', stderr);
	}
}

static void key(int k) {
	mp_term_event_t ev;

	memset(&ev, 0, sizeof(ev));
	ev.type = MP_TERM_EVENT_KEY;
	ev.key = k;
	mp_term_ui_event(&ui, &ev);
}

static void text(const char *s) {
	mp_term_event_t ev;

	memset(&ev, 0, sizeof(ev));
	ev.type = MP_TERM_EVENT_TEXT;
	ev.bytes = (const unsigned char *)s;
	ev.len = (int)strlen(s);
	mp_term_ui_event(&ui, &ev);
}

static void paint_info(mp_term_ui_t *shell, void *user) {
	(void)user;
	mp_term_ui_clear(shell);
	mp_term_ui_put(shell, 0, 0, "INFO-LINE", 7, 0, 0);
	mp_term_ui_hint(shell, "info");
}

static const mp_term_page_t infoPage = { paint_info, NULL, 0, NULL };
static const mp_term_page_t histPage = { paint_info, NULL, 0, NULL };

static const mp_term_banner_line_t splashLines[] = {
	{ "", "  ___  _    _" },
	{ "", " | _ \\| |_ (_)" },
	{ "Board", "xiao" },
	{ "Kernel", "miniPhi 0.1" },
};

static void resize(int rows, int cols) {
	mp_term_event_t ev;

	memset(&ev, 0, sizeof(ev));
	ev.type = MP_TERM_EVENT_RESIZE;
	ev.x = rows;
	ev.y = cols;
	mp_term_ui_event(&ui, &ev);
}

/* the column where s starts on row, or -1 */
static int row_at(int row, const char *s) {
	int n = (int)strlen(s);
	int c, i, ch;

	for(c = 0; c + n <= ui.cols; c++) {
		for(i = 0; i < n; i++)
			if(!mp_term_ui_at(&ui, row, c + i, &ch, NULL, NULL) || ch != (unsigned char)s[i])
				break;
		if(i == n)
			return(c);
	}
	return(-1);
}

static void test_splash(void) {
	int r, ch, top = -1, go = -1;

	mp_test_clock(0);
	expect(mp_term_ui_init(&ui, &term) == TRUE, "splash init");
	mp_term_ui_place(&ui, "xiao");
	mp_term_ui_welcome(&ui, splashLines, 4);
	expect(mp_term_ui_waiting(&ui) > 0, "waits for the size");
	mp_term_ui_flush(&ui);
	expect(ui.sized != YES, "no paint before the size");
	resize(30, 100);
	expect(ui.rows == 30 && ui.cols == 100, "splash sized");
	expect(mp_term_ui_waiting(&ui) == 40, "frame step");
	mp_test_clock(10000);
	mp_term_ui_flush(&ui);
	for(r = 0; r < ui.rows && top < 0; r++) {
		int c;

		for(c = 0; c < ui.cols / 2; c++)
			if(mp_term_ui_at(&ui, r, c, &ch, NULL, NULL) && ch == 'l') {
				top = r;
				break;
			}
	}
	expect(top > 0, "card centered left");
	expect(row_at(top, "xiao") > 0, "board on the frame");
	for(r = 0; r < ui.rows && go < 0; r++)
		if(row_at(r, "press any key") == (100 - 13) / 2)
			go = r;
	expect(go > top, "call to action centered");
	expect(mp_term_ui_waiting(&ui) == 0, "animation done");
	key('x');
	expect(ui.mode == MP_TERM_UI_MODE_PAGE, "key enters the shell");
	expect(row_at(1, "phi>") == 1, "prompt after splash");
	mp_term_ui_replay(&ui);
	expect(ui.mode == MP_TERM_UI_MODE_SPLASH, "cable returns to the splash");
	mp_test_clock(20000);
	mp_term_ui_flush(&ui);
	go = -1;
	for(r = 0; r < ui.rows && go < 0; r++)
		if(row_at(r, "press any key") == (100 - 13) / 2)
			go = r;
	expect(go > 0, "splash again");
	mp_test_clock(0);
}

int main(void) {
	int before, body, i;
	char buf[16];

	memset(&term, 0, sizeof(term));
	mp_term_init(NULL, &term, _write, NULL, MP_TERM_CAP_ALL, MP_TERM_COLOR_16);
	test_splash();
	memset(&term, 0, sizeof(term));
	mp_term_init(NULL, &term, _write, NULL, MP_TERM_CAP_ALL, MP_TERM_COLOR_16);
	expect(mp_term_ui_init(&ui, &term) == TRUE, "init");
	expect(mp_term_ui_add(&ui, "A/B/C/D", &infoPage) == FALSE, "depth");
	expect(mp_term_ui_add(&ui, "", &infoPage) == FALSE, "empty path");
	expect(mp_term_ui_add(&ui, "System/Info", &infoPage) == TRUE, "add info");
	expect(mp_term_ui_add(&ui, "System/Power/History", &histPage) == TRUE, "add history");

	mp_term_ui_place(&ui, "xiao");
	mp_term_ui_enter(&ui);
	expect(ui.rows == 24 && ui.cols == 80, "default glass");
	expect(row_has(0, "Console"), "current page");
	expect(!row_has(0, "Messages") && !row_has(0, "System"), "no header menu");
	expect(row_has(0, "00:00:00"), "clock");
	expect(row_has(ui.rows - 1, "xiao"), "place");
	expect(row_has(ui.rows - 1, "160 MHz"), "mhz");
	expect(row_at(1, "phi>") == 1, "prompt");

	before = sinkLen;
	mp_term_ui_flush(&ui);
	expect(sinkLen == before && sinkDrop == 0, "clean flush");

	{
		char longline[200];
		int w = mp_term_ui_cols(&ui) - 2;

		memset(longline, 'A', (size_t)w);
		longline[w] = ' ';
		longline[w + 1] = 'B';
		longline[w + 2] = 0;
		mp_term_ui_clear(&ui);
		expect(mp_term_ui_put(&ui, 0, 2, longline, 7, 0, 0) == 2, "wraps");
		expect(row_at(2, "B") == 2, "wrapped tail");
		expect(mp_term_ui_put(&ui, 4, 2, "one\ntwo", 7, 0, 0) == 2, "newline");
		expect(row_at(5, "one") == 2 && row_at(6, "two") == 2, "newline rows");
	}

	text("zq");
	expect(row_at(1, "zq") == 6, "typed");
	key(MP_TERM_KEY_ENTER);
	expect(row_at(1, "zq") == 6, "submitted");
	expect(row_at(2, "phi>") == 1, "prompt moves down");
	key(MP_TERM_KEY_UP);
	expect(row_at(2, "zq") == 6, "history");

	key(MP_TERM_KEY_ESCAPE);
	expect(row_has(ui.rows - 1, "choose"), "menu hint");
	expect(row_has(8, "Menu"), "menu title");
	expect(row_has(10, "Console"), "menu console");
	expect(row_has(12, "System"), "menu system");
	key(MP_TERM_KEY_ESCAPE);
	expect(ui.mode == MP_TERM_UI_MODE_PAGE && row_at(1, "phi>") == 1, "esc closes menu");
	key(MP_TERM_KEY_ESCAPE);
	key(MP_TERM_KEY_DOWN);
	key(MP_TERM_KEY_DOWN);
	key(MP_TERM_KEY_ENTER);
	expect(ui.mode == MP_TERM_UI_MODE_MENU && row_has(9, "System"), "submenu title");
	expect(row_has(11, "Info"), "menu info");
	expect(row_has(12, "Power"), "menu power");
	key(MP_TERM_KEY_ESCAPE);
	expect(ui.mode == MP_TERM_UI_MODE_MENU && row_has(10, "Console"), "esc back to main menu");
	key(MP_TERM_KEY_RIGHT);
	key(MP_TERM_KEY_ENTER);
	expect(row_has(1, "INFO-LINE"), "page");
	expect(row_has(0, "System > Info"), "crumb");
	key(MP_TERM_KEY_ESCAPE);
	key(MP_TERM_KEY_ENTER);
	key(MP_TERM_KEY_DOWN);
	key(MP_TERM_KEY_ENTER);
	key(MP_TERM_KEY_ENTER);
	expect(row_has(0, "System > Power > History"), "crumb history");

	mp_term_ui_log(&ui, "boot line");
	mp_term_ui_flush(&ui);
	expect(!row_has(0, "Messages 1"), "no message count");
	key(MP_TERM_KEY_ALT('m'));
	expect(row_has(1, "boot line"), "messages");
	expect(!row_has(0, "Messages 1"), "still no message count");

	body = mp_term_ui_rows(&ui);
	for(i = 0; i < body; i++) {
		snprintf(buf, sizeof(buf), "L%02d", i);
		mp_term_ui_log(&ui, buf);
	}
	mp_term_ui_flush(&ui);
	sinkLen = 0;
	sinkDrop = 0;
	mp_term_ui_log(&ui, "LAST");
	mp_term_ui_flush(&ui);
	sink[sinkLen < (int)sizeof(sink) ? sinkLen : (int)sizeof(sink) - 1] = 0;
	expect(sinkDrop == 0 && strstr((char *)sink, "\033[M") != NULL, "hardware scroll");
	expect(strstr((char *)sink, "\033[1S") == NULL, "no CSI S");
	expect(sinkLen < 1500, "scroll stays small");

	/* once the ring is full, logCount stops growing; the open page still follows */
	{
		char top[16], bot[16], held[16];
		int n = MP_TERM_UI_LOG;
		int first = n - body;

		for(i = 0; i < n; i++) {
			snprintf(buf, sizeof(buf), "N%02d", i);
			mp_term_ui_log(&ui, buf);
		}
		mp_term_ui_flush(&ui);
		snprintf(top, sizeof(top), "N%02d", first);
		snprintf(bot, sizeof(bot), "N%02d", n - 1);
		expect(row_has(1, top) && row_has(body, bot), "full ring follows");
		expect(!row_has(1, "N00"), "oldest scrolled off");
		sinkLen = 0;
		key(MP_TERM_KEY_UP);
		sink[sinkLen < (int)sizeof(sink) ? sinkLen : (int)sizeof(sink) - 1] = 0;
		expect(strstr((char *)sink, "\033[L") != NULL, "scroll back");
		mp_term_ui_log(&ui, "NEW");
		mp_term_ui_flush(&ui);
		snprintf(held, sizeof(held), "N%02d", first - 1);
		expect(row_has(1, held) && !row_has(body, "NEW"), "scroll holds");
		key(MP_TERM_KEY_END);
		expect(row_has(body, "NEW"), "end follows");
	}

	key(MP_TERM_KEY_ALT('s'));
	expect(row_has(11, "Info"), "alt opens system");
	mp_term_ui_remove(&ui, "System");
	mp_term_ui_flush(&ui);
	expect(!row_has(0, "System"), "removed");
	key(MP_TERM_KEY_ALT('c'));
	expect(row_has(2, "phi>"), "back to console");

	/* lines go out as UTF-8 box drawing once the glass decodes it, never as DEC */
	sinkLen = 0;
	mp_term_ui_redraw(&ui);
	sink[sinkLen] = 0;
	expect(strstr((char *)sink, "\033(0") == NULL && strstr((char *)sink, "|") != NULL, "ascii lines");
	term.utf8 = YES;
	term.utf8Known = YES;
	sinkLen = 0;
	mp_term_ui_flush(&ui);
	sink[sinkLen] = 0;
	expect(strstr((char *)sink, "\xe2\x94\x82") != NULL && strstr((char *)sink, "\033(0") == NULL, "utf8 lines");

	resize(30, 80);
	expect(ui.rows == 30 && ui.cols == 80, "resize");
	expect(term.rows == 30 && term.cols == 80, "real size kept");

	key(MP_TERM_KEY_ESCAPE);
	key(MP_TERM_KEY_END);
	expect(row_has(15, "Exit"), "menu exit");
	key(MP_TERM_KEY_ENTER);
	expect(ui.mode == MP_TERM_UI_MODE_SPLASH, "exit returns to the splash");
	expect(!row_has(11, "Menu") && !row_has(15, "Exit"), "menu leaves with the splash");
	mp_test_clock(mp_clock_ticks() + 1000);
	key('x');
	expect(ui.mode == MP_TERM_UI_MODE_PAGE, "key leaves the splash");

	if(failed) {
		fprintf(stderr, "%d failed\n", failed);
		return(1);
	}
	return(0);
}
