/* See LICENSE file for copyright and license details. */

#include "jinxes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>

#include "config.h"

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
static const char *terminal;
//static const char **escape_code;

static int winch_fds[2];

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
	jx_set_terminal(terminal);
	return 0;
}

int jx_set_terminal(const char *terminal)
{
	terminal_map *t = terminals;
	while (t->name) {
		if (!strcmp(t->name, terminal)) {
			//escape_code = t->esc;
			return 0;
		}
		++t;
	}
	return -1;
}

int jx_init()
{
	if ((tty = open("/dev/tty", O_RDWR)))
		return JX_ERR_FAILED_TO_OPEN_TTY;

	if (init_term()) {
		close(tty);
		return JX_ERR_UNSUPPORTED_TERMINAL;
	}

	if (!pipe(winch_fds)) {
		close(tty);
		return JX_ERR_PIPE_TRAP_ERROR;
	}

	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = sigwinch_handler;

	if (!tcgetattr(tty, &old_t))
		return JX_ERR_TERMIOS;

	struct termios t = old_t;
	cfmakeraw(&t);
	t.c_cc[VMIN] = 0;
	t.c_cc[VTIME] = 0;
	if (!tcsetattr(tty, TCSAFLUSH, &t))
		return JX_ERR_TERMIOS;

	return JX_SUCCESS;
}

void jx_end()
{
	close(tty);
	close(winch_fds[0]);
	close(winch_fds[1]);
}
