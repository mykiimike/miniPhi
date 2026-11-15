/* Included from term.h. The shell: a menu bar, one flat menu, and pages. */

#ifndef _HAVE_MP_TERM_UI_H
	#define _HAVE_MP_TERM_UI_H

	#ifndef MP_TERM_UI_COLS
		#define MP_TERM_UI_COLS 160
	#endif
	#ifndef MP_TERM_UI_ROWS
		#define MP_TERM_UI_ROWS 40
	#endif
	#ifndef MP_TERM_UI_NODES
		#define MP_TERM_UI_NODES 24
	#endif
	#ifndef MP_TERM_UI_LOG
		#define MP_TERM_UI_LOG 128
	#endif
	#ifndef MP_TERM_UI_LOG_LEN
		#define MP_TERM_UI_LOG_LEN 96
	#endif
	#ifndef MP_TERM_UI_SPLASH
		#define MP_TERM_UI_SPLASH 20
	#endif
	/* the line's packet size: UTF-8 never crosses one (USB full speed bulk) */
#ifndef MP_TERM_UI_PACKET
#define MP_TERM_UI_PACKET 64
#endif

#ifndef MP_TERM_UI_OUT
		#define MP_TERM_UI_OUT 512
	#endif

	#define MP_TERM_UI_DEPTH 3
	#define MP_TERM_UI_CON   16
	#define MP_TERM_UI_HIST  8

	#define MP_TERM_UI_MODE_SPLASH 0
	#define MP_TERM_UI_MODE_PAGE   1
	#define MP_TERM_UI_MODE_BAR    2
	#define MP_TERM_UI_MODE_MENU   3

	/* cell flags. Colors are the 8 minicom actually paints. */
	#define MP_TERM_UI_DEC   0x01
	#define MP_TERM_UI_BOLD  0x02
	#define MP_TERM_UI_UNDER 0x04

	typedef struct mp_term_ui_s mp_term_ui_t;

	/**
	 * A page. paint draws into the body (below the bar, above the status).
	 * key returns TRUE when it consumed the event. refresh is milliseconds
	 * between paints, 0 paints only when the page is dirty.
	 */
	typedef struct {
		void (*paint)(mp_term_ui_t *ui, void *user);
		int (*key)(mp_term_ui_t *ui, const mp_term_event_t *event, void *user);
		unsigned long refresh;
		void *user;
	} mp_term_page_t;

	typedef struct {
		char name[16];
		char accel;
		mp_bool_t used;
		mp_bool_t builtin;
		int parent;
		int child;
		int next;
		int badge;
		const mp_term_page_t *page;
	} mp_term_ui_node_t;

	typedef struct {
		int node;
		int cursor;
		int scroll;
		int top, left, width, height, count;
	} mp_term_ui_box_t;

	struct mp_term_ui_s {
		mp_term_t *term;
		int rows, cols;
		int mode;
		int open;
		int root;
		int menuDepth;
		int menuParent[MP_TERM_UI_DEPTH];
		int menuCursor[MP_TERM_UI_DEPTH];
		mp_term_ui_node_t node[MP_TERM_UI_NODES];
		mp_term_ui_box_t box[MP_TERM_UI_DEPTH];
		int depth;

		unsigned short page[MP_TERM_UI_ROWS][MP_TERM_UI_COLS];
		unsigned short sent[MP_TERM_UI_ROWS][MP_TERM_UI_COLS];

		int emitRow, emitCol, emitFg, emitBg, emitStyle, emitDec;
		int curRow, curCol;
		mp_bool_t cursorShown;
		mp_bool_t drew;
		mp_bool_t bodyDirty;
		unsigned long paintAt;

		char hint[48];
		char place[24];

		char splashLabel[MP_TERM_UI_SPLASH][16];
		char splashValue[MP_TERM_UI_SPLASH][MP_TERM_UI_COLS];
		int splashN;
		/** the glass answered once; the splash waits for it to center */
		mp_bool_t sized;
		/** the alternate screen was taken after the host answered */
		mp_bool_t glass;
		/* line cells go out as UTF-8 box drawing, else ASCII */
		mp_bool_t utf8;
		unsigned long startAt;
		unsigned long splashAt;
		int splashFrame;
		unsigned long splashBeat;

		/** bytes of one wake, sent in one go */
		unsigned char out[MP_TERM_UI_OUT];
		int outLen;

		char conLine[MP_TERM_UI_CON][MP_TERM_UI_COLS];
		int conHead, conCount, conLen, conCur, conHistN, conHistAt;
		char conInput[MP_TERM_UI_COLS];
		char conHist[MP_TERM_UI_HIST][MP_TERM_UI_COLS];
		char conDraft[MP_TERM_UI_COLS];
		int conNode;

		struct {
			unsigned long ticks;
			char text[MP_TERM_UI_LOG_LEN];
		} log[MP_TERM_UI_LOG];
		int logHead, logCount, logUnread, logSkip, flushedLog;
		/** one per stored line; paintedLog is the seq last drawn */
		unsigned long logSeq, paintedLog;
		mp_bool_t logFollow;
		int logNode;
	};

	/** Empty shell bound to a term. Registers Console and Messages. */
	mp_ret_t mp_term_ui_init(mp_term_ui_t *ui, mp_term_t *term);

	/**
	 * Add a page at a path such as "System/Power/History". Parents are
	 * created as needed. Depth is at most 3. An existing path keeps the
	 * node and replaces the page. FALSE when the path is empty, too deep,
	 * or the node pool is full.
	 */
	mp_ret_t mp_term_ui_add(mp_term_ui_t *ui, const char *path, const mp_term_page_t *page);
	/** Remove a path and everything under it. Empty groups disappear. */
	void mp_term_ui_remove(mp_term_ui_t *ui, const char *path);

	/**
	 * Take the serial line, paint the splash, and run the term task.
	 * lines may be NULL. The strings are copied.
	 */
	mp_ret_t mp_term_ui_start(mp_term_ui_t *ui, struct mp_serial_s *serial,
		const mp_term_banner_line_t *lines, int n);
	void mp_term_ui_stop(mp_term_ui_t *ui);

	/** Body drawing. Row 0 is the first line under the bar. */
	void mp_term_ui_clear(mp_term_ui_t *ui);
	/**
	 * Write text. A newline breaks the line, and a run that would pass the
	 * right edge continues under itself, on a space when there is one.
	 * Returns how many body rows it used.
	 */
	int mp_term_ui_put(mp_term_ui_t *ui, int row, int col, const char *text,
		int fg, int bg, int flags);
	/** How many body rows mp_term_ui_put would use for this text. */
	int mp_term_ui_lines(const mp_term_ui_t *ui, int col, const char *text);
	void mp_term_ui_fill(mp_term_ui_t *ui, int row, int col, int n, int ch,
		int fg, int bg, int flags);
	void mp_term_ui_hint(mp_term_ui_t *ui, const char *text);
	void mp_term_ui_cursor_at(mp_term_ui_t *ui, int row, int col);
	int mp_term_ui_rows(const mp_term_ui_t *ui);
	int mp_term_ui_cols(const mp_term_ui_t *ui);

	/** Right side of the status bar: the board name. */
	void mp_term_ui_place(mp_term_ui_t *ui, const char *where);
	void mp_term_ui_event(mp_term_ui_t *ui, const mp_term_event_t *event);
	void mp_term_ui_flush(mp_term_ui_t *ui);
	/** One printk line. Safe from an interrupt. Wakes the term task. */
	void mp_term_ui_log(mp_term_ui_t *ui, const char *text);
	void mp_term_ui_welcome(mp_term_ui_t *ui, const mp_term_banner_line_t *lines, int n);
	/** The host left and came back. Paint the same splash on the new glass. */
	void mp_term_ui_replay(mp_term_ui_t *ui);
	/** The driver felt the host take a packet: the line is alive, so paint. */
	void mp_term_ui_host_ready(mp_term_ui_t *ui);
	/** Leave the splash and paint the shell. */
	void mp_term_ui_enter(mp_term_ui_t *ui);
	/** Ticks until the next animation frame, 0 when nothing animates. */
	unsigned long mp_term_ui_waiting(const mp_term_ui_t *ui);
	/** Forget what the glass shows and paint everything again. */
	void mp_term_ui_redraw(mp_term_ui_t *ui);
	/** The last paint did not reach the host. Send it again. */
	void mp_term_ui_unsent(mp_term_ui_t *ui);
	/** The corner probe just went out. The next CPR is not a key. */
	void mp_term_ui_note_probe(mp_term_ui_t *ui);

	/**
	 * Visible cell, absolute, 0-based, including an open menu.
	 * Returns 1 and fills ch/fg/bg. ch is 7-bit.
	 */
	int mp_term_ui_at(mp_term_ui_t *ui, int row, int col, int *ch, int *fg, int *bg);

#endif
