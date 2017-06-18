#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "chat.h"


int main(const int argc, const char* const * const argv)
{
	if (argc > 1) {
		if (strcmp(argv[1], "client") == 0)
			return chat(CONMODE_CLIENT);
		else if (strcmp(argv[1], "host") == 0)
			return chat(CONMODE_HOST);
	}

	fprintf(stderr, "Usage: %s [type: host, client]\n", argv[0]);
	return EXIT_FAILURE;
}

