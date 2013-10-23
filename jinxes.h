/* See LICENSE file for copyright and license details. */
#ifndef JINXES_H
#define JINXES_H

#include <stdint.h>

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
	JX_ERR_OVERLAPPING_REGION,
	JX_ERR_OUT_OF_REGION
};

/* region flags */
#define JX_RF_ANCHOR_LEFT   (1 << 0)
#define JX_RF_ANCHOR_RIGHT  (1 << 1)
#define JX_RF_ANCHOR_TOP    (1 << 2)
#define JX_RF_ANCHOR_BOTTOM (1 << 3)
#define JX_RF_AUTOSIZE      (1 << 4)
#define JX_RF_RELATIVE_X    (1 << 5)
#define JX_RF_RELATIVE_Y    (1 << 6)
#define JX_RF_RELATIVE_W    (1 << 7)
#define JX_RF_RELATIVE_H    (1 << 8)

typedef struct {
	uint8_t type, mod;
	uint16_t key;
	uint32_t ch;
	int32_t w, h;
} jx_event;

typedef struct {
	/* settings */
	int x, y, w, h;
	int ax, ay, aw, ah;
	int flags;
	/* state */
	uint16_t fg, bg;
} jx_region;

#define JX_SCREEN ((jx_region *)NULL)
#define JX_MAX_REGIONS 128

int jx_initialise();
#define jx_initialize jx_initialise

void jx_terminate();

int jx_set_terminal(const char *terminal);

int jx_columns();
int jx_lines();

void jx_clear();
jx_region *jx_create_region(int x, int y, int w, int h, int flags);
void jx_destroy_region(jx_region *r);

void jx_begin(jx_region *r);
void jx_foreground(jx_region *r, uint16_t fg);
void jx_background(jx_region *r, uint16_t bg);
int jx_putc(jx_region *r, int x, int y, uint32_t ch);
int jx_write(jx_region *r, int x, int y, int w, int h, const char *text);
void jx_clear(jx_region *r);
void jx_end(jx_region *r);

int jx_layout();
void jx_show();

void jx_cursor(int cx, int cy);

int jx_peek(jx_event *event, int timeout);
int jx_poll(jx_event *event);

uint32_t jx_version();
const char *jx_error(int e);

#endif
