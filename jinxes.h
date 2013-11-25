/* See LICENSE file for copyright and license details. */
#ifndef JINXES_H
#define JINXES_H

#include <stdint.h>
#include <wchar.h>

/* input modifier */
#define JX_MOD_ALT	0x01

/* supported colours (TODO: xterm's 256 colour support) */
#define JX_DEFAULT	0x00
#define JX_BLACK	0x01
#define JX_RED		0x02
#define JX_GREEN	0x03
#define JX_YELLOW	0x04
#define JX_BLUE		0x05
#define JX_MAGENTA	0x06
#define JX_CYAN		0x07
#define JX_WHITE	0x08

/* attribute modification */
#define JX_BOLD		0x10
#define JX_UNDERLINE	0x20
#define JX_REVERSE	0x40

/* error enumeration */
enum {
	JX_SUCCESS,
	JX_ERR_NOT_INIT,
	JX_ERR_FAILED_TO_OPEN_TTY,
	JX_ERR_IOCTL,
	JX_ERR_OPEN_TTY,
	JX_ERR_WINDOW_SIZE,
	JX_ERR_TERMIOS,
	JX_ERR_UNSUPPORTED_TERMINAL,
	JX_ERR_PIPE_TRAP_ERROR,
	JX_ERR_OVERLAPPING_WINDOW,
	JX_ERR_OUT_OF_WINDOW,
	JX_ERR_OUT_OF_PAD,
	JX_ERR_INVALID_WINDOW,
	JX_ERR_INVALID_PAD,
	JX_ERR_INVALID_WINDOW_SIZE,
	JX_ERR_INVALID_PAD_SIZE
};

/* window flags */
#define JX_WF_ANCHOR_LEFT   (1 << 0)
#define JX_WF_ANCHOR_RIGHT  (1 << 1)
#define JX_WF_ANCHOR_TOP    (1 << 2)
#define JX_WF_ANCHOR_BOTTOM (1 << 3)
#define JX_WF_AUTOSIZE      (1 << 4)
#define JX_WF_RELATIVE_X    (1 << 5)
#define JX_WF_RELATIVE_Y    (1 << 6)
#define JX_WF_RELATIVE_W    (1 << 7)
#define JX_WF_RELATIVE_H    (1 << 8)
#define JX_WF_DOCK_LEFT     (1 << 9)
#define JX_WF_DOCK_RIGHT    (1 << 10)
#define JX_WF_DOCK_TOP      (1 << 11)
#define JX_WF_DOCK_BOTTOM   (1 << 12)
#define JX_WF_DIRTY         (1 << 16)
#define JX_WF_PAD           (1 << 18)

typedef struct {
	uint8_t type, mod;
	uint16_t key;
	uint32_t ch;
	int32_t w, h;
} jx_event;

typedef struct jx_window_s {
	/* settings */
	int x, y, w, h;
	int ax, ay, aw, ah;
	int px, py, pw, ph;
	int flags;
	/* state */
	uint16_t fg, bg;
	wchar_t *buffer_text;
	uint16_t *buffer_fg;
	uint16_t *buffer_bg;
	/* linked list */
	struct jx_window_s *prev, *next;
	/* hierarchy */
	struct jx_window_s *parent;
} jx_window;

#define JX_SCREEN (jx_screen())

int jx_initialise();
#define jx_initialize jx_initialise

void jx_terminate();

int jx_set_terminal(const char *terminal);

int jx_columns();
int jx_lines();

jx_window *jx_screen();
jx_window *jx_create_window(jx_window *parent, int x, int y, int w, int h, int flags);
int jx_destroy_window(jx_window *w);
int jx_make_pad(jx_window *w, int pw, int ph);

int jx_move(jx_window *w, int x, int y);
int jx_resize(jx_window *win, int w, int h);
int jx_scroll_pad(jx_window *w, int px, int py);
int jx_resize_pad(jx_window *w, int pw, int ph);

void jx_foreground(jx_window *w, uint16_t fg);
void jx_background(jx_window *w, uint16_t bg);
int jx_putc(jx_window *w, int x, int y, wchar_t ch);
int jx_write(jx_window *win, int x, int y, int w, int h, const char *text);
int jx_clear(jx_window *w);

int jx_layout();
void jx_render();

void jx_cursor(int cx, int cy);

int jx_peek(jx_event *event, int timeout);
int jx_poll(jx_event *event);

uint32_t jx_version();
const char *jx_error(int e);
int jx_last_error();

#endif
