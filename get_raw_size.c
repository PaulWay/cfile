/*
 * test-cat.c
 * This file is part of The PaulWay Libraries
 *
 * Copyright (C) 2006 - Paul Wayper (paulway@mabula.net)
 * Copyright (C) 2012 Peter Miller
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
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

#include "cfile.h"

off_t get_raw_size(const char *filename);

off_t get_raw_size(const char *filename) {
	cfile *cf;
	off_t size;
	
	cf = cfile_open(filename, "r");
	size = cfsize(cf);
	cfclose(cf);
	
	return size;
}

int main(int argc, char *argv[]) {
	int n = 0;
	
	for (; n < argc; n++) {
		printf("Raw size of %s = %lu\n", argv[n], get_raw_size(argv[n]));
	}
	return 0;
}
