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
static int escape_code_len[TS_MAX];

static char buf_INput[MAX_INPUT_BUFFER];
static int buf_INput_i;
static char buf_OUTput[MAX_OUTPUT_BUFFER];
static int buf_OUTput_i;

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
#define BUF_PUTC(b,x) buf_##b##put[buf_##b##put_i++] = x
#define BUF_PUT(b,x,l) memcpy(buf_##b##put + buf_##b##put_i, x, l), \
	buf_##b##put_i += l
#define BUF_PUTE(b,x) BUF_PUT(b, escape_code[x], escape_code_len[x])
#define BUF_RESET(b) buf_##b##put_i = 0
#define BUF_FLUSHIF(b) if (buf_##b##put_i > MAX_##b##PUT_FLUSH) \
	write(tty, buf_##b##put, buf_##b##put_i -= buf_##b##put_i), \
	buf_##b##put_i = 0
#define BUF_FLUSH(b) write(tty, buf_##b##put, \
	buf_##b##put_i -= buf_##b##put_i), buf_##b##put_i = 0
#define BUF_DEBUG(b) debug_print(buf_##b##put, buf_##b##put_i)

/* return string descriptions of errors */
const char *jx_error(int e)
{
	switch (e) {
		case JX_ERR_NOT_INIT:
			return "terminal has not been initialised";
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
	short escode[TS_MAX];
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
					terminfo_short8_esctable[i & ~(1<<14)];
			else if (escode[i] & (1 << 15))
				escape_code[i] =
					terminfo_short12_esctable[i & ~(1<<15)];
			else
				escape_code[i] =
					terminfo_long_esctable[i];
			escape_code_len[i] = strlen(escape_code[i]);
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

	BUF_PUTE(OUT, TS_ENTER_CA_MODE);
	BUF_PUTE(OUT, TS_KEYPAD_XMIT);
	BUF_PUTE(OUT, TS_CURSOR_INVISIBLE);
	BUF_PUTE(OUT, TS_CLEAR_SCREEN);
	BUF_DEBUG(OUT);
	BUF_FLUSH(OUT);

	initialised = true;

	return JX_SUCCESS;
}

/* function that finalises everything */
void jx_end()
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
