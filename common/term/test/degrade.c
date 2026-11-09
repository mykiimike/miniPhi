/* Host test of the term color degradation. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define MP_HOST_TASK
#define SUPPORT_COMMON_TERM
#define MP_TERM_HAVE_VT100
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
static unsigned char sink[512];
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
	/* nearest16: exact palette hits */
	expect(mp_term_nearest16(0, 0, 0) == 0, "black -> 0");
	expect(mp_term_nearest16(255, 0, 0) == 9, "pure red -> 9");
	expect(mp_term_nearest16(0, 255, 0) == 10, "pure green -> 10");
	expect(mp_term_nearest16(255, 255, 255) == 15, "white -> 15");
	expect(mp_term_nearest16(127, 127, 127) == 8, "grey -> 8");

	/* nearest16: a tie keeps the lower index (black beats nothing closer) */
	expect(mp_term_nearest16(1, 1, 1) == 0, "near-black -> 0");

	/* nearest256: cube corners */
	expect(mp_term_nearest256(0, 0, 0) == 16, "cube black -> 16");
	expect(mp_term_nearest256(255, 255, 255) == 231, "cube white -> 231");
	expect(mp_term_nearest256(255, 0, 0) == 196, "cube red -> 196");
	expect(mp_term_nearest256(0, 255, 0) == 46, "cube green -> 46");
	expect(mp_term_nearest256(0, 0, 255) == 21, "cube blue -> 21");

	/* degrade: truecolor RGB through a 16-color session lands on nearest16 */
	_reset(MP_TERM_COLOR_16);
	mp_term_fg_rgb(&term, 255, 0, 0);
	expect(eq("\033[91m"), "rgb red degrades to ESC[91m");

	_reset(MP_TERM_COLOR_16);
	mp_term_fg_rgb(&term, 0, 0, 0);
	expect(eq("\033[30m"), "rgb black degrades to ESC[30m");

	/* degrade: truecolor RGB through a 256-color session lands on nearest256 */
	_reset(MP_TERM_COLOR_256);
	mp_term_fg_rgb(&term, 255, 0, 0);
	expect(eq("\033[38;5;196m"), "rgb red degrades to 256 ESC[38;5;196m");

	_reset(MP_TERM_COLOR_256);
	mp_term_fg_rgb(&term, 0, 0, 0);
	expect(eq("\033[38;5;16m"), "rgb black degrades to 256 ESC[38;5;16m");

	/* degrade: a 256-color session clamped from TRUE at init still emits 256 */
	_reset(MP_TERM_COLOR_TRUE);
	mp_term_color(&term, MP_TERM_COLOR_256);
	mp_term_fg_rgb(&term, 255, 255, 255);
	expect(eq("\033[38;5;231m"), "runtime clamp to 256 -> ESC[38;5;231m");

	if(failed) {
		fprintf(stderr, "%d failed\n", failed);
		return(1);
	}
	printf("ok\n");
	return(0);
}
