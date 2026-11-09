/* The export calls stay compilable when TERMINAL is off, and do nothing. */

#include <stdio.h>

typedef signed char mp_ret_t;
typedef signed char mp_bool_t;
typedef int mp_term_color_t;

#include "../term.h"

int main(void) {
	if(mp_term_init(0, 0, 0, 0, 0, 0) != 0)
		return(1);
	mp_term_caps(0, 0);
	mp_term_color(0, 0);
	if(mp_term_caps_get(0) != 0)
		return(1);
	if(mp_term_color_get(0) != 0)
		return(1);
	mp_term_move(0, 0, 0);
	mp_term_clear(0);
	mp_term_clear_line(0);
	mp_term_style(0, 0);
	mp_term_fg_rgb(0, 0, 0, 0);
	mp_term_bg_rgb(0, 0, 0, 0);
	mp_term_fg(0, 0);
	mp_term_bg(0, 0);
	mp_term_title(0, 0);
	mp_term_link(0, 0, 0);
	mp_term_mouse(0, 0);
	mp_term_alt(0, 0);
	mp_term_puts(0, 0, 0);
	mp_term_putc(0, 0);
	mp_term_rx(0, 0);
	if(mp_term_read(0, 0) != 0)
		return(1);
	if(mp_term_waiting(0) != 0)
		return(1);
	mp_term_splash(0, 0, 0);
	mp_term_cursor(0, 0);
	mp_term_query(0);
	mp_term_settle(0);
	mp_term_graphic(0, 0);
	if(mp_term_ui_init(0, 0) != 0)
		return(1);
	if(mp_term_ui_add(0, 0, 0) != 0)
		return(1);
	mp_term_ui_remove(0, 0);
	if(mp_term_ui_start(0, 0, 0, 0) != 0)
		return(1);
	mp_term_ui_stop(0);
	if(mp_term_ui_put(0, 0, 0, 0, 0, 0, 0) != 0)
		return(1);
	if(mp_term_ui_lines(0, 0, 0) != 0)
		return(1);
	mp_term_ui_fill(0, 0, 0, 0, 0, 0, 0, 0);
	mp_term_ui_clear(0);
	mp_term_ui_hint(0, 0);
	if(mp_term_ui_rows(0) != 0)
		return(1);
	if(mp_term_ui_cols(0) != 0)
		return(1);
	mp_term_ui_cursor_at(0, 0, 0);
	mp_term_ui_place(0, 0);
	mp_term_ui_event(0, 0);
	mp_term_ui_flush(0);
	mp_term_ui_log(0, 0);
	mp_term_ui_welcome(0, 0, 0);
	mp_term_ui_replay(0);
	mp_term_ui_enter(0);
	mp_term_ui_note_probe(0);
	mp_term_ui_redraw(0);
	if(mp_term_ui_waiting(0) != 0)
		return(1);
	if(mp_term_ui_at(0, 0, 0, 0, 0, 0) != 0)
		return(1);
	printf("ok\n");
	return(0);
}
