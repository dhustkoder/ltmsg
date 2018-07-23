#include <stdlib.h>
#include <stdarg.h>
#include <wchar.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <locale.h>
#include <sys/select.h>

#ifdef HAVE_NCURSESW_NCURSES_H
#include <ncursesw/ncurses.h>
#elif defined(HAVE_NCURSESW_H)
#include <ncursesw.h>
#elif HAVE_NCURSES_H
#include <ncurses.h>
#endif

#include "io.h"
#include "network.h"


#define CHAT_STACK_SIZE (24)
#define BUFFER_SIZE     (256)


enum ChatCmd {
	CHATCMD_NORMAL,
	CHATCMD_QUIT
};


static const struct ConnectionInfo* cinfo  = NULL;     // connection information
static wchar_t conn_buffer[BUFFER_SIZE]    = { '\0' }; // buffer for in/out msgs
static wchar_t* chatstack[CHAT_STACK_SIZE] = { NULL }; // the chat msg stack with unames
static int chatstack_idx                   = 0;        // current chat stack index

static wchar_t buffer[BUFFER_SIZE]         = { '\0' }; // text box's buffer for user input
static int blen                            = 0;        // text box's current buffer size
static int bidx                            = 0;        // text box's cursor position in the buffer
static int cy, cx;                                     // current cursor position in the window
static int my, mx;                                     // max y and x positions
static int hy, hx;                                     // text box's home y and x (start position)


static void free_char_stack(void)
{
	for (int i = 0; i < chatstack_idx; ++i)
		free(chatstack[i]);
	chatstack_idx = 0;
}


static void chat_stack_push(wchar_t* const str)
{
	if (chatstack_idx >= CHAT_STACK_SIZE) {
		free(chatstack[0]);
		for (int i = 0; i < (CHAT_STACK_SIZE - 1); ++i)
			chatstack[i] = chatstack[i + 1];
		chatstack_idx = CHAT_STACK_SIZE - 1;
	}
	chatstack[chatstack_idx++] = str;
}


static void stack_msg(const char* const uname, const wchar_t* const msg)
{
	const int chars = snprintf(NULL, 0, "%s: %ls", uname, msg);
	wchar_t* const str = malloc(sizeof(wchar_t) * chars + sizeof(wchar_t));
	swprintf(str, chars + 1, L"%s: %ls", uname, msg);
	chat_stack_push(str);
}


static void stack_info(const wchar_t* const fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	vswprintf(conn_buffer, BUFFER_SIZE, fmt, args);
	wchar_t* str = malloc(sizeof(wchar_t) * wcslen(conn_buffer) + sizeof(wchar_t));
	wcscpy(str, conn_buffer);
	chat_stack_push(str);

	va_end(args);
}


static int set_kbd_timeout(const int delay)
{
	static int curdelay = -1;
	const int prevdelay = curdelay;
	curdelay = delay;
	timeout(curdelay);
	return prevdelay;
}


static void initialize_ui(void)
{
	setlocale(LC_ALL, "");
	initscr();
	cbreak();
	noecho();
	intrflush(stdscr, FALSE);
	set_kbd_timeout(5);
	keypad(stdscr, TRUE);
}


static void terminate_ui(void)
{
	endwin();
}


static void print_ui(void)
{
	printw("Host: %s (%s). Client: %s (%s).\n"
	       "=================================================\n",
	       cinfo->host_uname, cinfo->host_ip,
	       cinfo->client_uname, cinfo->client_ip);

	int i;
	for (i = 0; i < chatstack_idx; ++i)
		printw("%ls\n", chatstack[i]);
	for (; i < CHAT_STACK_SIZE; ++i)
		printw("\n");

	printw("==================================================\n> ");
}


static void clear_text_box(void)
{
	cy = hy;
	cx = hx;
	blen = 0;
	bidx = 0;
	buffer[0] = '\0';
}


static void move_cursor_left(void)
{
	if (cy > hy || cx > hx) {
		--bidx;
		--cx;
		if (cx < 0) {
			cx = mx - 1;
			--cy;
		}
		move(cy, cx);
	}
}


static void move_cursor_right(void)
{
	if (bidx < blen) {
		++bidx;
		++cx;
		if (cx >= mx) {
			cx = 0;
			++cy;
		}
		move(cy, cx);
	}
}


static void move_cursor_begin(void)
{
	if (bidx != 0) {
		bidx = 0;
		cy = hy;
		cx = hx;
		move(cy, cx);
	}
}


static void move_cursor_end(void)
{
	if (bidx < blen) {
		bidx = blen;
		cx = hx + (blen % mx);
		cy = hy + (blen / mx);
		move(cy, cx);
	}
}



static void refresh_ui(void)
{
	clear();
	move(0, 0);
	print_ui();
	getyx(stdscr, hy, hx);
	getmaxyx(stdscr, my, mx);
	cy = hy + (bidx / mx);
	cx = hx + (bidx % mx);
	printw("%ls", buffer);
	move(cy, cx);
	refresh();
}


static bool update_text_box(void)
{
	extern int get_wch(wint_t* wch);

	wint_t c;
	if (get_wch(&c) == ERR)
		return false;

	#ifdef DEBUG_
	stack_info(L"KEY PRESSED %li", c);
	#endif

	switch (c) {
	case 10: // also enter (ascii) [fall]
	case KEY_ENTER: // submit msg, if any
		return blen > 0;
	case KEY_LEFT:
		move_cursor_left();
		return false;
	case KEY_RIGHT:
		move_cursor_right();
		return false;
	case 127: // also backspace (ascii) [fall]
	case KEY_BACKSPACE:
		if (bidx > 0) {
			memmove(&buffer[bidx - 1], &buffer[bidx],
			        sizeof(*buffer) * (blen - bidx));
			buffer[--blen] = '\0';
			move_cursor_left();
			refresh_ui();
		}
		return false;
	case KEY_HOME:
		move_cursor_begin();
		return false;
	case KEY_END:
		move_cursor_end();
		return false;
	case KEY_RESIZE:
		refresh_ui();
		return false;
	}

	if (blen < (BUFFER_SIZE - 1)) {
		if (bidx < blen)
			memmove(&buffer[bidx + 1], &buffer[bidx],
			        sizeof(*buffer) * (blen - bidx));
		buffer[bidx] = (wchar_t) c;
		buffer[++blen] = '\0';
		move_cursor_right();
		refresh_ui();
	}

	return false;
}


static enum ChatCmd parseChatCmd(const char* const uname, const wchar_t* const cmd, const bool islocal)
{
	if (wcscmp(cmd, L"/quit") == 0) {
		stack_info(L"Connection closed by %s. Press any key to exit...", uname);
		refresh_ui();
		const int prev = set_kbd_timeout(-1);
		getch();
		set_kbd_timeout(prev);
		return CHATCMD_QUIT;
	} else if (islocal && wcscmp(cmd, L"/clean") == 0) {
		free_char_stack();
	} else if (islocal) {
		stack_info(L"Unknown command \'%ls\'.", cmd);
	}

	return CHATCMD_NORMAL;
}


static bool checkfd(const int fd)
{
	struct timeval timeout = { 0, 5000 };
	fd_set fds;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	return select(fd + 1, &fds, NULL, NULL, &timeout) > 0;
}


int chat(const enum ConnectionMode mode)
{
	if ((cinfo = initialize_connection(mode)) == NULL)
		return EXIT_FAILURE;

	initialize_ui();
	refresh_ui();

	const char *uname = NULL;
	const wchar_t *msg = NULL;

	for (;;) {
		if (checkfd(cinfo->remote_fd)) {
			wread_into(conn_buffer, cinfo->remote_fd, BUFFER_SIZE);
			uname = cinfo->remote_uname;
			msg = conn_buffer;
		} else if (update_text_box()) {
			wwrite_into(cinfo->remote_fd, buffer);
			uname = cinfo->local_uname;
			msg = buffer;
		}

		if (uname != NULL) {
			const bool islocal = uname == cinfo->local_uname;

			if (msg[0] == '/') {
				if (parseChatCmd(uname, msg, islocal) == CHATCMD_QUIT)
					break;
			} else {
				stack_msg(uname, msg);
			}

			if (islocal)
				clear_text_box();
			
			uname = NULL;
			refresh_ui();
		}
	}

	terminate_ui();
	free_char_stack();
	return EXIT_SUCCESS;
}

