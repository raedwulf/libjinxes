/* See LICENSE file for copyright and license details. */

#include "jinxes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>

#include "config.h"

#define TERMINFO_BOOLEAN
#define TERMINFO_ENUM
#include "terminfo.h"
#undef TERMINFO_ENUM
#undef TERMINFO_BOOLEAN

#define TERMINFO_NUMBER
#define TERMINFO_ENUM
#include "terminfo.h"
#undef TERMINFO_ENUM
#define TERMINFO_MAP
#include "terminfo.h"
#undef TERMINFO_MAP
#undef TERMINFO_NUMBER

#define TERMINFO_STRING
#define TERMINFO_ENUM
#include "terminfo.h"
#undef TERMINFO_ENUM
#define TERMINFO_MAP
#include "terminfo.h"
#undef TERMINFO_MAP
#undef TERMINFO_STRING

#define TERMINFO_ESCAPE_CODES
#define TERMINFO_TERM_LIST
#include "terminfo.h"
#undef TERMINFO_TERM_LIST
#undef TERMINFO_ESCAPE_CODES

#if defined(__APPLE__) && defined(__MACH__)
#define PLATFORM_MACOSX
#elif defined(__linux__)
#define PLATFORM_LINUX
#elif defined(sun) || defined(__sun)
#define PLATFORM_SOLARIS
#elif defined(_WIN32)
#error Are you kidding me?
#else
#error Unsupported platform.
#endif

#if defined(PLATFORM_SOLARIS)
static inline cfmakeraw(struct termios *termios)
{
	t->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
                        | INLCR | IGNCR | ICRNL | IXON);
	t->c_oflag &= ~OPOST;
	t->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	t->c_cflag &= ~(CSIZE | PARENB);
	t->c_cflag |= CS8;
}
#endif

/* jinxes static state variables */
static int tty;
static bool initialised;
static struct termios old_t;
static int winch_fds[2];

static const terminal_map *ttm;
static const char *terminal;
static const char *escape_code[TS_MAX];
static unsigned short t_columns, t_lines;
static int escape_code_len[TS_MAX];

static char IN[MAX_INPUT_BUFFER];
static int IN_index;
static char OUT[MAX_OUTPUT_BUFFER];
static int OUT_index;

static jx_window *window_head, *window_tail;

void debug_print(char* buffer, int l)
{
	const char *esc_char= "\a\b\f\n\r\t\v\\";
	const char *essc_str= "abfnrtv\\";
	char* dest = malloc(l*4 + 1), *p = dest;
	for(int i = 0; i < l; i++) {
		int j = 0;
		for(; j < (int)sizeof(esc_char); j++) {
			if (buffer[i] == esc_char[j]) {
				*p++ = '\\';
				*p++ = essc_str[j];
				break;
			}
		}
		if (buffer[i] < 32) {
			snprintf(p, 5, "\\%03o", buffer[i]);
			p += 4;
		} else if (j == sizeof(esc_char))
			*p++ = buffer[i];
	}
	*p='\0';
	fputs(dest, stderr);
	free(dest);
}

#define SCLEN(x) x,sizeof(x)
#define BUF_PUTC(b,x) b##[b##_index++] = x
#define BUF_PUT(b,x,l) memcpy(b + b##_index, x, l), b##_index += l
#define BUF_PUTE(b,x) BUF_PUT(b, escape_code[x], escape_code_len[x])
#define BUF_RESET(b) b##_index = 0
#define BUF_FLUSHIF(b) if (b##_index > MAX_##b##_FLUSH) \
	write(tty, b, b##_index), b##_index = 0
#define BUF_FLUSH(b) write(tty, b, b##_index), b##_index = 0
#ifdef DEBUG
#define BUF_DEBUG(b) debug_print(b, b##_index)
#else
#define BUF_DEBUG(b)
#endif

#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* return string descriptions of errors */
const char *jx_error(int e)
{
	switch (e) {
		case JX_ERR_NOT_INIT:
			return "terminal has not been initialised";
		case JX_ERR_OPEN_TTY:
			return "failed to open tty";
		case JX_ERR_WINDOW_SIZE:
			return "failed to set window size";
		case JX_ERR_TERMIOS:
			return "termios error";
		case JX_ERR_IOCTL:
			return "ioctl error";
		case JX_ERR_UNSUPPORTED_TERMINAL:
			return "unsupported terminal";
		case JX_ERR_PIPE_TRAP_ERROR:
			return "pipe error";
		default:
			return "unknown";
	}
}

/* to handle the window change, pipe the size changes to the event handler */
static void sigwinch_handler(int sig)
{
	struct winsize size;
	int ret = ioctl(tty, TIOCGWINSZ, &size);
	write(winch_fds[1], &ret, sizeof(int));
	write(winch_fds[1], &size.ws_row, sizeof(unsigned short));
	write(winch_fds[1], &size.ws_col, sizeof(unsigned short));
}

/* initialise the terminal from environment, fall back to xterm-256color */
static int init_term()
{
	terminal = getenv("TERM");
	if (!terminal)
		terminal = "xterm-256color";

	/* TODO: fall intelligently to similar terminal
	 * names/terminfo database*/

	return jx_set_terminal(terminal);
}

/* check if terminal has capability */
static bool has_bool(terminfo_boolean b)
{
	if (b >= 32) return (ttm->caps_ & (1 << (b - 32)));
	else return (ttm->caps & (1 << b));
}

/* check if window pointer is valid */
static int check_window(jx_window *w)
{
	for (jx_window *a = window_head; a != window_tail; a = a->next)
		if (w == a)
			return JX_SUCCESS;
	return JX_ERR_INVALID_WINDOW;
}

/* initialise the library and sets up the terminal */
int jx_initialise()
{
	if (!(tty = open("/dev/tty", O_RDWR)))
		return JX_ERR_OPEN_TTY;

	if (init_term()) {
		close(tty);
		return JX_ERR_UNSUPPORTED_TERMINAL;
	}

	if (pipe(winch_fds)) {
		close(tty);
		return JX_ERR_PIPE_TRAP_ERROR;
	}

	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = sigwinch_handler;

	if (tcgetattr(tty, &old_t))
		return JX_ERR_TERMIOS;

	struct termios t = old_t;
	cfmakeraw(&t);
	t.c_cc[VMIN] = 0;
	t.c_cc[VTIME] = 0;
	if (tcsetattr(tty, TCSAFLUSH, &t))
		return JX_ERR_TERMIOS;

	OUT_index = 0; IN_index = 0;

	BUF_PUTE(OUT, TS_CLEAR_SCREEN);
	BUF_PUTE(OUT, TS_ENTER_CA_MODE);
	BUF_PUTE(OUT, TS_KEYPAD_XMIT);
	BUF_PUTE(OUT, TS_CURSOR_INVISIBLE);
	BUF_DEBUG(OUT);
	BUF_FLUSH(OUT);

	/* TODO: check if this is needed */
	int ret;
	sigwinch_handler(0);
	read(winch_fds[0], &ret, sizeof(int));
	read(winch_fds[0], &t_columns, sizeof(unsigned short));
	read(winch_fds[0], &t_lines, sizeof(unsigned short));

	if (ret == -1)
		return JX_ERR_IOCTL;

	/* create first window for the screen */
	window_head = window_tail = calloc(sizeof(jx_window), 1);
	window_head->w = t_columns;
	window_head->h = t_lines;
	window_head->flags = JX_WF_AUTOSIZE | JX_WF_AUTOSIZE;

	initialised = true;

	return JX_SUCCESS;
}

/* finalise everything */
void jx_terminate()
{
	if (initialised) {
		/* clear the screen and restore mode */
		BUF_PUTE(OUT, TS_CURSOR_NORMAL);
		BUF_PUTE(OUT, TS_EXIT_ATTRIBUTE_MODE);
		BUF_PUTE(OUT, TS_CLEAR_SCREEN);
		BUF_PUTE(OUT, TS_KEYPAD_LOCAL);
		BUF_PUTE(OUT, TS_EXIT_CA_MODE);
		BUF_FLUSH(OUT);
		/* restore terminal settings */
		tcsetattr(tty, TCSAFLUSH, &old_t);
		close(tty);
		close(winch_fds[0]);
		close(winch_fds[1]);
		initialised = false;
	}
}

/* set the terminal */
int jx_set_terminal(const char *terminal)
{
	terminal_map *t = terminals;
	short term[sizeof(terminals)/sizeof(terminal_map)];
	unsigned short escode[TS_MAX];
	int i = 0;
	/* find the terminal */
	for (; t->name; t++) {
		if (strcmp(t->name, terminal)) continue;
		/* save the target terminal */
		ttm = t;
		/* find the parent terminal */
		while (t->parent != -1) {
			term[i++] = t - terminals;
			t = terminals + t->parent;
		}
		/* copy escape codes from terminal */
		memcpy(escode, t->esc, sizeof(escode));
		/* modify with each successive variation */
		for (i = i-1; i >= 0; i--) {
			const terminal_variant *tv = terminals[term[i]].esc;
			for (; tv->location != -1; tv++)
				escode[tv->location] = tv->esc;
		}
		/* store pointers to the escape code strings */
		for (i = 0; i < TS_MAX; i++) {
			if (escode[i] & (1 << 14))
				escape_code[i] =
					terminfo_short8_esctable[escode[i] & ~(1<<14)];
			else if (escode[i] & (1 << 15))
				escape_code[i] =
					terminfo_short12_esctable[escode[i] & ~(1<<15)];
			else
				escape_code[i] =
					terminfo_long_esctable[escode[i]];
			escape_code_len[i] = strlen(escape_code[i]);
		}
		return 0;
	}
	return -1;
}

/* get the top level main screen window */
jx_window *jx_screen()
{
	return window_head;
}

/* create a window to edit */
jx_window *jx_create_window(jx_window *parent, int x, int y, int w, int h,
		int flags)
{
	/* validate parent window */
	if (check_window(parent))
		return NULL;
	jx_window *win = calloc(sizeof(jx_window), 1);
	win->x = x;
	win->y = y;
	win->w = w;
	win->h = h;
	win->buffer_text = malloc(w * h * sizeof(wchar_t));
	win->buffer_fg = malloc(w * h * sizeof(uint16_t));
	win->buffer_bg = malloc(w * h * sizeof(uint16_t));
	win->flags = flags | JX_WF_DIRTY;
	win->parent = parent;
	win->prev = window_tail;
	window_tail->next = win;
	return win;
}

/* destroy a window */
int jx_destroy_window(jx_window *w)
{
	/* validate window */
	if (check_window(w))
		return JX_ERR_INVALID_WINDOW;

	/* destroy children */
	for (jx_window *a = window_head; a != window_tail; a = a->next)
		if (a->parent == w)
			jx_destroy_window(a);

	/* finally destroy window */
	for (jx_window *a = window_head; a != window_tail; a = a->next) {
		if (a == w) {
			if (w->flags & JX_WF_PAD) {
				free(w->buffer_text);
				free(w->buffer_fg);
				free(w->buffer_bg);
			}
			a->prev->next = w->next;
			a->next->prev = w->prev;
			free(w);

			return JX_SUCCESS;
		}
	}

	return JX_ERR_INVALID_WINDOW;
}

/* change a window into a pad */
int jx_make_pad(jx_window *w, int pw, int ph)
{
	/* validate window */
	if (check_window(w))
		return JX_ERR_INVALID_WINDOW;

	if (pw < w->w || ph < w->h)
		return JX_ERR_INVALID_PAD_SIZE;

	w->flags |= JX_WF_PAD | JX_WF_DIRTY;
	if (w->buffer_text)
		free(w->buffer_text);
	w->buffer_text = malloc(pw * ph * sizeof(wchar_t));
	if (w->buffer_fg)
		free(w->buffer_fg);
	w->buffer_fg = malloc(pw * ph * sizeof(uint16_t));
	if (w->buffer_bg)
		free(w->buffer_bg);
	w->buffer_bg = malloc(pw * ph * sizeof(uint16_t));

	return JX_SUCCESS;
}

/* move a window */
int jx_move(jx_window *w, int x, int y)
{
	/* validate window */
	if (check_window(w))
		return JX_ERR_INVALID_WINDOW;
	/* check if any work needs to be done */
	if (w->x == x && w->y == y)
		return JX_SUCCESS;

	/* the parent window is dirty */
	w->parent->flags |= JX_WF_DIRTY;
	/* the siblings are dirty if this window overlaps them */
	for (jx_window *a = window_head; a != window_tail; a = a->next) {
		/* only process windows that are under this window */
		if (a == w) break;
		/* if overlap, mark as dirty */
		if (a->parent == w->parent &&
		    a->x < w->x + w->w && a->x + a->w > w->x &&
		    a->y < w->y + w->h && a->y + a->h > w->y)
			a->flags |= JX_WF_DIRTY;
	}
	/* this window is dirty */
	w->flags |= JX_WF_DIRTY;
	w->x = x;
	w->y = y;

	return JX_SUCCESS;
}

int jx_resize(jx_window *win, int w, int h)
{
	/* validate window */
	if (check_window(win))
		return JX_ERR_INVALID_WINDOW;
	/* check size is valid */
	if (w <= 0 || h <= 0)
		return JX_ERR_INVALID_WINDOW_SIZE;
	/* check if any work needs to be done */
	if (win->w == w && win->h == h)
		return JX_SUCCESS;

	win->parent->flags |= JX_WF_DIRTY;
	/* the siblings are dirty if this window overlaps them */
	for (jx_window *a = window_head; a != window_tail; a = a->next) {
		/* only process windows that are under this window */
		if (a == win) break;
		/* if overlap, mark as dirty */
		if (a->parent == win->parent &&
		    (a->x < win->x + win->w && a->x + a->w > win->x &&
		    a->y < win->y + win->h && a->y + a->h > win->y) ||
		    (a->x < win->x + w && a->x + a->w > win->x &&
		    a->y < win->y + h && a->y + a->h > win->y))
			a->flags |= JX_WF_DIRTY;
	}
	if (win->flags & JX_WF_PAD) {
		if (win->pw < w || win->ph < h)
			jx_resize_pad(win, MAX(w, win->pw), MAX(h, win->ph));
	} else {
		wchar_t *buffer_text = malloc(w * h * sizeof(wchar_t));
		memcpy(buffer_text, win->buffer_text, w * h * sizeof(wchar_t));
		uint16_t *buffer_fg = malloc(w * h * sizeof(uint16_t));
		memcpy(buffer_fg, win->buffer_fg, w * h * sizeof(uint16_t));
		uint16_t *buffer_bg = malloc(w * h * sizeof(uint16_t));
		memcpy(buffer_bg, win->buffer_bg, w * h * sizeof(uint16_t));
		win->buffer_text = buffer_text;
		win->buffer_fg = buffer_fg;
		win->buffer_bg = buffer_bg;
	}
	win->w = w;
	win->h = h;

	return JX_SUCCESS;
}

int jx_scroll_pad(jx_window *w, int px, int py)
{
	/* validate window */
	if (check_window(w))
		return JX_ERR_INVALID_WINDOW;
	/* validate is pad */
	if (w->flags & JX_WF_PAD)
		return JX_ERR_INVALID_PAD;
	/* check if its in range */
	if (w->px < 0 || w->py < 0 ||
	    w->px + w->w > w->pw || w->py + w->h > w->ph)
		return JX_ERR_OUT_OF_PAD;
	/* check if any work needs to be done */
	if (w->px == px && w->py && py)
		return JX_SUCCESS;

	w->flags |= JX_WF_DIRTY;
	w->px = px;
	w->py = py;

	return JX_SUCCESS;
}

int jx_resize_pad(jx_window *w, int pw, int ph)
{
	/* validate window */
	if (check_window(w))
		return JX_ERR_INVALID_WINDOW;
	/* validate is pad */
	if (w->flags & JX_WF_PAD)
		return JX_ERR_INVALID_PAD;
	/* check size is valid */
	if (pw <= 0 || ph <= 0)
		return JX_ERR_INVALID_PAD_SIZE;
	/* check if its in range */
	if (pw < w->w || ph < w->h)
		return JX_ERR_OUT_OF_PAD;
	/* move the scroll into range */
	if (w->px + w->w > pw)
		w->px = pw - w->w;
	if (w->py + w->h > ph)
		w->py = ph - w->h;
	
	wchar_t *buffer_text = malloc(pw * ph * sizeof(wchar_t));
	memcpy(buffer_text, w->buffer_text, pw * ph * sizeof(wchar_t));
	uint16_t *buffer_fg = malloc(pw * ph * sizeof(uint16_t));
	memcpy(buffer_fg, w->buffer_fg, pw * ph * sizeof(uint16_t));
	uint16_t *buffer_bg = malloc(pw * ph * sizeof(uint16_t));
	memcpy(buffer_bg, w->buffer_bg, pw * ph * sizeof(uint16_t));
	w->buffer_text = buffer_text;
	w->buffer_fg = buffer_fg;
	w->buffer_bg = buffer_bg;
	w->flags |= JX_WF_DIRTY;
	w->pw = pw;
	w->ph = ph;

	return JX_SUCCESS;
}

/* set the default foreground for a window */
void jx_foreground(jx_window *w, uint16_t fg)
{
	w->fg = fg;
	w->flags |= JX_WF_DIRTY;
}

/* set the default background for a window */
void jx_background(jx_window *w, uint16_t bg)
{
	w->bg = bg;
	w->flags |= JX_WF_DIRTY;
}

/* clear the terminal */
int jx_clear(jx_window *w)
{
	if (w == window_head) {
		jx_foreground(w, JX_DEFAULT);
		jx_background(w, JX_DEFAULT);
		BUF_PUTE(OUT, TS_EXIT_CA_MODE);
		BUF_PUTE(OUT, TS_CLEAR_SCREEN);
		BUF_PUTE(OUT, TS_ENTER_CA_MODE);
		BUF_DEBUG(OUT);
		BUF_FLUSH(OUT);
	} else {
		/* validate window */
		if (check_window(w))
			return JX_ERR_INVALID_WINDOW;
	}

	int width = w->flags & JX_WF_PAD ? w->pw : w->w;
	int height = w->flags & JX_WF_PAD ? w->ph : w->h;
	memset(w->buffer_text, 0, width * height * sizeof(wchar_t));
	memset(w->buffer_fg, 0, width * height * sizeof(uint16_t));
	memset(w->buffer_bg, 0, width * height * sizeof(uint16_t));

	return JX_SUCCESS;
}

/* return the number of columns in the terminal */
int jx_columns()
{
	return t_columns;
}

/* return the number of lines in the terminal */
int jx_lines()
{
	return t_lines;
}
