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
static int initialised;
static struct termios old_t;
static const terminal_map *ttm;
static const char *terminal;
static const char *escape_code[TI_MAX];

static int winch_fds[2];

/* return string descriptions of errors */
const char *jx_error(int e)
{
	switch (e) {
		case JX_ERR_FAILED_TO_OPEN_TTY:
			return "failed to open tty";
		case JX_ERR_TERMIOS:
			return "termios error";
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

/* function that sets the passed terminal */
int jx_set_terminal(const char *terminal)
{
	terminal_map *t = terminals;
	short term[sizeof(terminals)/sizeof(terminal_map)];
	short escode[TI_MAX];
	int i = 0;
	/* find the terminal */
	for (; t->name; t++) {
		if (strcmp(t->name, terminal)) continue;
		/* save the target terminal */
		ttm = t;
		/* find the parent terminal */
		while (t->parent != -1) {
			term[i++] = terminals - t;
			t = terminals + t->parent;
		}
		/* copy escape codes from terminal */
		memcpy(escode, t->esc, sizeof(escode));
		/* modify with each successive variation */
		for (i = i-1; i >= 0; i--) {
			const terminal_variant *tv = terminals[i].esc;
			for (; tv->location != -1; tv++)
				escode[tv->location] = tv->esc;
		}
		/* store pointers to the escape code strings */
		for (i = 0; i < TI_MAX; i++) {
			if (escode[i] & (1 << 14))
				escape_code[i] =
					terminfo_short8_esctable[i & ~(1<<14)];
			else if (escode[i] & (1 << 15))
				escape_code[i] =
					terminfo_short12_esctable[i & ~(1<<15)];
			else
				escape_code[i] =
					terminfo_long_esctable[i];
		}
		return 0;
	}
	return -1;
}

/* function that initialises the library and sets up the terminal */
int jx_init()
{
	if (!(tty = open("/dev/tty", O_RDWR)))
		return JX_ERR_FAILED_TO_OPEN_TTY;

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

	return JX_SUCCESS;
}

/* function that finalises everything */
void jx_end()
{
	/* restore terminal settings */
	tcsetattr(tty, TCSAFLUSH, &old_t);
	close(tty);
	close(winch_fds[0]);
	close(winch_fds[1]);
}
