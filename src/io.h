#ifndef LTMSG_IO_H_
#define LTMSG_IO_H_
#include <wchar.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>


static inline void readInto(char* const dest, const int fdsrc, const int maxsize)
{
	int n = read(fdsrc, dest, maxsize - 1);
	if (n > 0 && dest[n - 1] == '\n')
		--n;
	dest[n] = '\0';
}


static inline void writeInto(const int fd_dest, const char* const src)
{
	write(fd_dest, src, strlen(src));
}


static inline void wreadInto(wchar_t* const dest, const int fdscr, const int maxsize)
{
	int n = read(fdscr, dest, maxsize * sizeof(wchar_t) - 1);
	if ((n/sizeof(wchar_t)) > 1 && dest[(n/sizeof(wchar_t)) - 1] == L'\n')
		--n;
	dest[n/sizeof(wchar_t)] = L'\0';
}


static inline void wwriteInto(const int fd_dest, const wchar_t* const src)
{
	write(fd_dest, src, wcslen(src) * sizeof(wchar_t)); 
}


static inline void askUserFor(const char* const msg, char* const dest, const int size)
{
	writeInto(STDOUT_FILENO, msg);
	readInto(dest, STDIN_FILENO, size);
}


#endif
