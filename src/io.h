#ifndef LTMSG_IO_H_
#define LTMSG_IO_H_
#include <wchar.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>


static inline void read_into(char* const dest, const int fdsrc, const int maxsize)
{
	int n = read(fdsrc, dest, maxsize - 1);
	if (n > 0 && dest[n - 1] == '\n')
		--n;
	dest[n] = '\0';
}


static inline void write_into(const int fd_dest, const char* const src)
{
	write(fd_dest, src, strlen(src));
}


static inline void wread_into(wchar_t* const dest, const int fdscr, const int maxsize)
{
	int n = read(fdscr, dest, maxsize * sizeof(wchar_t) - 1) / sizeof(wchar_t);
	if (n > 1 && dest[n - 1] == L'\n')
		--n;
	dest[n] = L'\0';
}


static inline void wwrite_into(const int fd_dest, const wchar_t* const src)
{
	write(fd_dest, src, wcslen(src) * sizeof(wchar_t)); 
}



#endif
