#include <stdio.h>
#include <string.h>
#include "config.h"

/**
 * cfile - compressed file read/write library.
 *
 * C file library that provides transparent read/write access to
 * gzip and bzip2 compressed files.
 *
 * Licence: GPL (2 or any later version)
 */
int main(int argc, char *argv[])
{
	if (argc != 2)
		return 1;

	if (strcmp(argv[1], "depends") == 0) {
		printf("ccan/talloc\n");
		return 0;
	}

	return 1;
}
