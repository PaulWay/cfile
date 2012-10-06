/*
 * test-cat.c
 * This file is part of The PaulWay Libraries
 *
 * Copyright (C) 2006 - Paul Wayper (paulway@mabula.net)
 *
 * The PaulWay Libraries are free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * The PaulWay Libraries are distributed in the hope that they will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with The PaulWay Libraries; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

/** \file test-cat.c
 * \brief A 'cat' analogue which uses the cfile library.
 *
 * test-cat is a 'cat' analogue which uses the cfile library.  It's used
 * as a partial test of the file reading routines provided by cfile.  To
 * test it, simply run
 * <tt>'test-cat $compressed_file | zdiff - $compressed_file'</tt>
 * (or whatever your local compressed-file-reading-diff variant is).
 * If the output is different, then obviously the cfile library is wrong!
 */
 
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>

#include <talloc.h>

#include "cfile.h"

void *context = NULL;

cfile *out = NULL;

void write_file (const char *name) {
    /* write_file - read the named file and write it to stdout
     */
    cfile *in = cfopen(name, "r");
    if (! in) {
        fprintf(stderr,
            "Error: can't open %s for reading: %s!\n",
            name, strerror(errno)
        );
    }
    char *line = NULL;
    int linelen = 0;
    while (! cfeof(in)) {
        line = cfgetline(in, line, &linelen);
        if (! line) break;
        cfprintf(out, "%s", line);
    }
    cfclose(in);
} /* write_file */

int main (int argc, char *argv[])
{
	context = talloc_init("main test-cat context");
	out = cfdopen(1, "w");
	if (! out) {
	    fprintf(stderr,
	        "Error: Can't open output: %s!\n", strerror(errno)
	    );
	    talloc_free(context);
	    return 1;
	}
	int arg;
	for (arg = 1; arg < argc; arg += 1) {
	    write_file(argv[arg]);
	}
	cfclose(out);
	talloc_free(context);
	return 0;
}
