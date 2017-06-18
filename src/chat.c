#include <stdlib.h>
#include <stdarg.h>
#include <wchar.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <locale.h>
#include <sys/select.h>
#include <ncurses.h>
#include "io.h"
#include "network.h"


#define CHAT_STACK_SIZE ((int)24)
#define BUFFER_SIZE     ((int)256)


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


static void freeChatStack(void)
{
	for (int i = 0; i < chatstack_idx; ++i)
		free(chatstack[i]);
}


static void chatStackPushBack(wchar_t* const str)
{
	if (chatstack_idx >= CHAT_STACK_SIZE) {
		free(chatstack[0]);
		for (int i = 0; i < (CHAT_STACK_SIZE - 1); ++i)
			chatstack[i] = chatstack[i + 1];
		chatstack_idx = CHAT_STACK_SIZE - 1;
	}
	chatstack[chatstack_idx++] = str;
}


static void stackMsg(const char* const uname, const wchar_t* const msg)
{
	const int chars = snprintf(NULL, 0, "%s: %ls", uname, msg);
	wchar_t* const str = malloc(sizeof(wchar_t) * chars + sizeof(wchar_t));
	swprintf(str, chars + 1, L"%s: %ls", uname, msg);
	chatStackPushBack(str);
}


static void stackInfo(const wchar_t* const fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	vswprintf(conn_buffer, BUFFER_SIZE, fmt, args);
	wchar_t* str = malloc(sizeof(wchar_t) * wcslen(conn_buffer) + sizeof(wchar_t));
	wcscpy(str, conn_buffer);
	chatStackPushBack(str);

	va_end(args);
}


static int setKbdTimeout(const int delay)
{
	static int curdelay = -1;
	const int prevdelay = curdelay;
	curdelay = delay;
	timeout(curdelay);
	return prevdelay;
}


static void initializeUI(void)
{
	setlocale(LC_ALL, "");
	initscr();
	cbreak();
	noecho();
	intrflush(stdscr, FALSE);
	setKbdTimeout(5);
	keypad(stdscr, TRUE);
}


static void terminateUI(void)
{
	endwin();
}


static void printUI(void)
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


static void clearTextBox(void)
{
	cy = hy;
	cx = hx;
	blen = 0;
	bidx = 0;
	buffer[0] = '\0';
}


static void moveCursorLeft(void)
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


static void moveCursorRight(void)
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


static void moveCursorHome(void)
{
	if (bidx != 0) {
		bidx = 0;
		cy = hy;
		cx = hx;
		move(cy, cx);
	}
}


static void moveCursorEnd(void)
{
	if (bidx < blen) {
		bidx = blen;
		cx = hx + (blen % mx);
		cy = hy + (blen / mx);
		move(cy, cx);
	}
}



static void refreshUI(void)
{
	clear();
	move(0, 0);
	printUI();
	getyx(stdscr, hy, hx);
	getmaxyx(stdscr, my, mx);
	cy = hy + (bidx / mx);
	cx = hx + (bidx % mx);
	printw("%ls", buffer);
	move(cy, cx);
	refresh();
}


static bool updateTextBox(void)
{
	wint_t c;
	if (get_wch(&c) == ERR)
		return false;

	#ifdef DEBUG_
	stackInfo(L"KEY PRESSED %li", c);
	#endif

	switch (c) {
	case 10: // also enter (ascii) [fall]
	case KEY_ENTER: // submit msg, if any
		return blen > 0;
	case KEY_LEFT:
		moveCursorLeft();
		return false;
	case KEY_RIGHT:
		moveCursorRight();
		return false;
	case 127: // also backspace (ascii) [fall]
	case KEY_BACKSPACE:
		if (bidx > 0) {
			memmove(&buffer[bidx - 1], &buffer[bidx],
			        sizeof(*buffer) * (blen - bidx));
			buffer[--blen] = '\0';
			moveCursorLeft();
			refreshUI();
		}
		return false;
	case KEY_HOME:
		moveCursorHome();
		return false;
	case KEY_END:
		moveCursorEnd();
		return false;
	case KEY_RESIZE:
		refreshUI();
		return false;
	}

	if (blen < (BUFFER_SIZE - 1)) {
		if (bidx < blen)
			memmove(&buffer[bidx + 1], &buffer[bidx],
			        sizeof(*buffer) * (blen - bidx));
		buffer[bidx] = (wchar_t) c;
		buffer[++blen] = '\0';
		moveCursorRight();
		refreshUI();
	}

	return false;
}


static enum ChatCmd parseChatCmd(const char* const uname, const wchar_t* const cmd, const bool islocal)
{
	if (wcscmp(cmd, L"/quit") == 0) {
		stackInfo(L"Connection closed by %s. Press any key to exit...", uname);
		refreshUI();
		const int prev = setKbdTimeout(-1);
		getch();
		setKbdTimeout(prev);
		return CHATCMD_QUIT;
	} else if (islocal) {
			stackInfo(L"Unknown command \'%ls\'.", cmd);
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
	if ((cinfo = initializeConnection(mode)) == NULL)
		return EXIT_FAILURE;

	initializeUI();
	refreshUI();

	const char *uname = NULL;
	const wchar_t *msg = NULL;

	for (;;) {
		if (checkfd(cinfo->remote_fd)) {
			wreadInto(conn_buffer, cinfo->remote_fd, BUFFER_SIZE);
			uname = cinfo->remote_uname;
			msg = conn_buffer;
		} else if (updateTextBox()) {
			wwriteInto(cinfo->remote_fd, buffer);
			uname = cinfo->local_uname;
			msg = buffer;
		}

		if (uname != NULL) {
			const bool islocal = uname == cinfo->local_uname;

			if (msg[0] == '/') {
				if (parseChatCmd(uname, msg, islocal) == CHATCMD_QUIT)
					break;
			} else {
				stackMsg(uname, msg);
			}

			if (islocal)
				clearTextBox();
			
			uname = NULL;
			refreshUI();
		}
	}

	terminateUI();
	freeChatStack();
	return EXIT_SUCCESS;
}

