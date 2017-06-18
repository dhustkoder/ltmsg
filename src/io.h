#ifndef LTMSG_IO_H_
#define LTMSG_IO_H_
#include <string.h>
#include <stdbool.h>
#include <unistd.h>


static inline bool readInto(char* const dest, const int fdsrc, const int maxsize)
{
	int n;
	if ((n = read(fdsrc, dest, maxsize - 1)) <= 0) {
		perror("read");
		return false;
	}

	if (dest[n - 1] == '\n')
		--n;

	dest[n] = '\0';
	return true;
}


static inline bool writeInto(const int fd_dest, const char* const src)
{
	if (write(fd_dest, src, strlen(src)) <= 0) {
		perror("write");
		return false;
	}
	return true;
}


static inline void askUserFor(const char* const msg, char* const dest, const int size)
{
	writeInto(STDOUT_FILENO, msg);
	readInto(dest, STDIN_FILENO, size);
}


#endif
