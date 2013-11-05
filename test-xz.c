/*
 * test-xz.c
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

#include <string.h>
#include <errno.h>
#include <lzma.h>
#include <stdio.h>
#include <string.h>
#include <talloc.h>

#include "cfile.h"

#define BUFFER_SIZE 4096

static void *context = NULL;

int encode(const char *filename) {
	lzma_stream xz_stream = LZMA_STREAM_INIT;
	lzma_ret rtn = 0;
	char *line = NULL;
	uint8_t *buffer = NULL;
	size_t linelen = 0;
	ssize_t filelen = 0;
	FILE *infh;
	FILE *outfh;
	char *outname = NULL;

    line = talloc_array(context, char, 160);
    buffer = talloc_array(context, uint8_t, BUFFER_SIZE);
    
	/* Initialise encoder.  Put input and output buffer information in
	 * structure and then call lzma_easy_encoder to start an encoder
	 * using this structure. */
	printf("Using easy encoder... ");
	xz_stream.next_in = (uint8_t *)line;
	xz_stream.next_out = buffer;
	xz_stream.avail_out = BUFFER_SIZE;
	rtn = lzma_easy_encoder(&xz_stream, 9, LZMA_CHECK_CRC64);
	printf("Returned %d\n", rtn);

	/* open the input file */
	infh = fopen(filename, "r");
	if (!infh) {
		fprintf(stderr, "Failed to open %s: %s(%d)\n",
		 filename, strerror(errno), errno
		);
		return errno;
	}

	/* Read lines from the file, compressing each one as we go. */
	while (! feof(infh)) {
		fgets(line, 160, infh);
		linelen = strlen(line);
		rtn = lzma_code(&xz_stream, LZMA_RUN);
		printf("Coded %zu bytes, got %d, compressed to %lu bytes\n", 
		 linelen, rtn, xz_stream.total_out
		);
		xz_stream.avail_in = linelen;
		
		filelen += linelen;
	}
	fclose(infh);
	
	/* Tell LZMA to finalise its compression */
	for (;;) {
		rtn = lzma_code(&xz_stream, LZMA_FINISH);
		printf("Finalising compression: got %d, %lu bytes ready in buffer\n",
		 rtn, xz_stream.total_out
		);
		if (rtn == LZMA_STREAM_END) break;
	}
	
	/* Open the output file */
	outname = talloc_asprintf(context, "%s.xz", filename);
	outfh = fopen(outname, "w");
	if (!outfh) {
		fprintf(stderr, "Failed to open %s: %s(%d)\n",
		 filename, strerror(errno), errno
		);
		return errno;
	}
	
	/* Write the buffer to it */
	fwrite(buffer, 1, xz_stream.total_out, outfh);
	fclose(outfh);
	
	/* Free up our allocated memory */
	talloc_free(line);
	talloc_free(buffer);
	talloc_free(outname);
	
	/* Finish up */
	printf("Read %lu bytes!\n", filelen);
	printf("Talloc report when finishing encoding, after freeing memory:\n");
	talloc_report_full(context, stderr);
	return 0;
}

int main(int argc, char *argv[]) {

    talloc_enable_leak_report();
    context = talloc_init("main test-xz context");
    cfile_set_context(context);

	/* input file is our own code */
	encode("test-xz.c");
	
	talloc_report_full(context, stderr);
    talloc_free(context);
    return EXIT_SUCCESS;
}
