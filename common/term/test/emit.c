/* Host test of the term emitters. Not linked into the firmware. */

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
static unsigned char sink[2048];
static int sinkLen;

static void _write(void *user, const unsigned char *bytes, int n) {
	(void)user;
	if(n > 0 && sinkLen + n <= (int)sizeof(sink)) {
		memcpy(sink + sinkLen, bytes, (size_t)n);
		sinkLen += n;
	}
}

static int eq(const char *want) {
	return(sinkLen == (int)strlen(want) && memcmp(sink, want, (size_t)sinkLen) == 0);
}

static void _reset(mp_term_color_t color) {
	sinkLen = 0;
	memset(sink, 0, sizeof(sink));
	mp_term_init(NULL, &term, _write, NULL, MP_TERM_CAP_ALL, color);
}

int main(void) {
	/* 16-color foreground: index 1 (red) -> ESC[31m */
	_reset(MP_TERM_COLOR_16);
	mp_term_fg(&term, 1);
	expect(eq("\033[31m"), "fg index 1 -> ESC[31m");

	/* bright foreground: index 9 (bright red) -> ESC[91m */
	_reset(MP_TERM_COLOR_16);
	mp_term_fg(&term, 9);
	expect(eq("\033[91m"), "fg index 9 -> ESC[91m");

	/* background index 2 -> ESC[42m */
	_reset(MP_TERM_COLOR_16);
	mp_term_bg(&term, 2);
	expect(eq("\033[42m"), "bg index 2 -> ESC[42m");

	/* truecolor foreground -> ESC[38;2;r;g;bm */
	_reset(MP_TERM_COLOR_TRUE);
	mp_term_fg_rgb(&term, 10, 20, 30);
	expect(eq("\033[38;2;10;20;30m"), "fg rgb true -> ESC[38;2;...m");

	/* truecolor background -> ESC[48;2;r;g;bm */
	_reset(MP_TERM_COLOR_TRUE);
	mp_term_bg_rgb(&term, 1, 2, 3);
	expect(eq("\033[48;2;1;2;3m"), "bg rgb true -> ESC[48;2;...m");

	/* no color: fg_rgb emits nothing */
	_reset(MP_TERM_COLOR_NONE);
	mp_term_fg_rgb(&term, 1, 2, 3);
	expect(sinkLen == 0, "no color emits nothing");

	/* move both -> ESC[row;colH */
	_reset(MP_TERM_COLOR_TRUE);
	mp_term_move(&term, 5, 10);
	expect(eq("\033[5;10H"), "move 5,10 -> ESC[5;10H");

	/* move both omitted -> ESC[;H */
	_reset(MP_TERM_COLOR_TRUE);
	mp_term_move(&term, 0, 0);
	expect(eq("\033[;H"), "move 0,0 -> ESC[;H");

	/* move row only -> ESC[row;H */
	_reset(MP_TERM_COLOR_TRUE);
	mp_term_move(&term, 3, 0);
	expect(eq("\033[3;H"), "move 3,0 -> ESC[3;H");

	/* clear -> ESC[2J ESC[1;1H */
	_reset(MP_TERM_COLOR_TRUE);
	mp_term_clear(&term);
	expect(eq("\033[2J\033[1;1H"), "clear -> ESC[2J ESC[1;1H");

	/* clear line -> ESC[2K */
	_reset(MP_TERM_COLOR_TRUE);
	mp_term_clear_line(&term);
	expect(eq("\033[2K"), "clear line -> ESC[2K");

	/* style bold -> ESC[1m */
	_reset(MP_TERM_COLOR_TRUE);
	mp_term_style(&term, MP_TERM_STYLE_BOLD);
	expect(eq("\033[1m"), "style bold -> ESC[1m");

	/* style reset -> ESC[0m */
	_reset(MP_TERM_COLOR_TRUE);
	mp_term_style(&term, MP_TERM_STYLE_RESET);
	expect(eq("\033[0m"), "style reset -> ESC[0m");

	/* style bold+underline -> ESC[1;4m */
	_reset(MP_TERM_COLOR_TRUE);
	mp_term_style(&term, MP_TERM_STYLE_BOLD | MP_TERM_STYLE_UNDERLINE);
	expect(eq("\033[1;4m"), "style bold+underline -> ESC[1;4m");

	/* title (OSC on) -> ESC]0;title BEL */
	_reset(MP_TERM_COLOR_TRUE);
	mp_term_title(&term, "hi");
	expect(eq("\033]0;hi\007"), "title -> ESC]0;hi BEL");

	/* title NULL is a no-op even with OSC */
	_reset(MP_TERM_COLOR_TRUE);
	mp_term_title(&term, NULL);
	expect(sinkLen == 0, "title NULL no-op");

	/* alt screen on -> ESC[?1049h */
	_reset(MP_TERM_COLOR_TRUE);
	mp_term_alt(&term, 1);
	expect(eq("\033[?1049h"), "alt on -> ESC[?1049h");

	/* mouse on -> ESC[?1000;1002;1006h */
	_reset(MP_TERM_COLOR_TRUE);
	mp_term_mouse(&term, 1);
	expect(eq("\033[?1000;1002;1006h"), "mouse on");

	/* DEC special graphics, then back to ASCII */
	_reset(MP_TERM_COLOR_TRUE);
	mp_term_graphic(&term, 1);
	expect(eq("\033(0"), "graphic on");
	_reset(MP_TERM_COLOR_TRUE);
	mp_term_graphic(&term, 0);
	expect(eq("\033(B"), "graphic off");

	/* puts/putc are raw byte pipes */
	_reset(MP_TERM_COLOR_TRUE);
	mp_term_puts(&term, (const unsigned char *)"abc", 3);
	mp_term_putc(&term, '!');
	expect(eq("abc!"), "puts+putc raw");

	/* splash: 16-color (minicom paints 30-37, not 38;2) and a scroll region */
	_reset(MP_TERM_COLOR_TRUE);
	{
		static const mp_term_banner_line_t line = { "", "LOGO" };

		mp_term_splash(&term, &line, 1);
	}
	sink[sinkLen] = 0;
	expect(strstr((char *)sink, "\033[2J") != NULL, "splash clears");
	expect(strstr((char *)sink, "LOGO") != NULL, "splash art");
	expect(strstr((char *)sink, "\033[36m") != NULL, "splash 16-color");
	expect(strstr((char *)sink, "\033[38;2;") == NULL, "splash not truecolor");
	expect(strstr((char *)sink, ";999r") != NULL, "splash pins a region");

	if(failed) {
		fprintf(stderr, "%d failed\n", failed);
		return(1);
	}
	printf("ok\n");
	return(0);
}
