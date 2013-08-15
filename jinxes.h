/* See LICENSE file for copyright and license details. */
#ifndef JINXES_H
#define JINXES_H

#include <stdint.h>

// Currently there is only one modificator. See also struct tb_event's mod
// field.
#define JX_MOD_ALT	0x01

// Colors (see struct tb_cell's fg and bg fields).
#define JX_DEFAULT	0x00
#define JX_BLACK	0x01
#define JX_RED		0x02
#define JX_GREEN	0x03
#define JX_YELLOW	0x04
#define JX_BLUE		0x05
#define JX_MAGENTA	0x06
#define JX_CYAN		0x07
#define JX_WHITE	0x08

// Attributes, it is possible to use multiple attributes by combining them
// using bitwise OR ('|'). Although, colors cannot be combined. But you can
// combine attributes and a single color. See also struct tb_cell's fg and bg
// fields.
#define JX_BOLD		0x10
#define JX_UNDERLINE	0x20
#define JX_REVERSE	0x40

enum {
	JX_SUCCESS,
	JX_ERR_NOT_INIT,
	JX_ERR_OPEN_TTY,
	JX_ERR_WINDOW_SIZE,
	JX_ERR_TERMIOS,
	JX_ERR_UNSUPPORTED_TERMINAL,
	JX_ERR_PIPE_TRAP_ERROR
};

typedef struct {
	uint8_t type, mod;
	uint16_t key;
	uint32_t ch;
	int32_t w , h;
} jx_event;

typedef struct {
	uint32_t ch;
	uint16_t fg, bg;
} jx_cell;

int jx_init();
void jx_end();

int jx_set_terminal(const char *terminal);

void jx_clear();
void jx_fg(uint16_t fg);
void jx_bg(uint16_t bg);

void jx_show();

void jx_cursor(int cx, int cy);

void jx_putc(int x, int y, const jx_cell *cell);
void jx_put(int x, int y, uint32_t ch);
void jx_blitc(int x, int y, int w, int h, const jx_cell *cells);
void jx_blit(int x, int y, int w, int h, const char *text);

int jx_peek(jx_event *event, int timeout);
int jx_poll(jx_event *event);

uint32_t jx_version();
const char *jx_error(int e);

#endif
