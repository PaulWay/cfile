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
 * 
 * vim: ts=4 et ai:
 */

#include <string.h>
#include <errno.h>
#include <lzma.h>
#include <stdio.h>
#include <string.h>
#include <talloc.h>

#include "cfile.h"

#define BUFFER_SIZE 4096

static char *lzma_ret_code[12] = {
	"LZMA_OK",
	"LZMA_STREAM_END",
	"LZMA_NO_CHECK",
	"LZMA_UNSUPPORTED_CHECK",
	"LZMA_GET_CHECK",
	"LZMA_MEM_ERROR",
	"LZMA_MEMLIMIT_ERROR",
	"LZMA_FORMAT_ERROR",
	"LZMA_OPTIONS_ERROR",
	"LZMA_DATA_ERROR",
	"LZMA_BUFF_ERROR",
	"LZMA_PROG_ERROR"
};

static void *context = NULL;

int encode(const char *filename) {
	lzma_stream xz_stream = LZMA_STREAM_INIT;
	lzma_ret rtn = 0;
	char *in_buf = NULL;
	uint8_t *out_buf = NULL;
	size_t linelen = 0;
	ssize_t filelen = 0;
	FILE *infh;
	FILE *outfh;
	char *outname = NULL;

    in_buf  = talloc_array(context, char, BUFFER_SIZE);
    out_buf = talloc_array(context, uint8_t, BUFFER_SIZE);
    
	/* Initialise encoder.  Put input and output buffer information in
	 * structure and then call lzma_easy_encoder to start an encoder
	 * using this structure. */
	printf("Using easy encoder... ");
	xz_stream.next_out = out_buf;
	xz_stream.avail_out = BUFFER_SIZE;
	rtn = lzma_easy_encoder(&xz_stream, 9, LZMA_CHECK_CRC64);
	printf("Returned %s\n", lzma_ret_code[rtn]);

	/* open the input file */
	infh = fopen(filename, "r");
	if (!infh) {
		fprintf(stderr, "Failed to open %s: %s(%d)\n",
		 filename, strerror(errno), errno
		);
		return errno;
	}

	/* Open the output file */
	outname = talloc_asprintf(context, "%s.xz", filename);
	printf("Writing %s\n", outname);
	outfh = fopen(outname, "w");
	if (!outfh) {
		fprintf(stderr, "Failed to open %s: %s(%d)\n",
		 filename, strerror(errno), errno
		);
		return errno;
	}
	
	/* Read lines from the file, compressing each one as we go. */
	for (;;) {
		fgets(in_buf, BUFFER_SIZE, infh);
		if (feof(infh)) break;
		
		xz_stream.next_in = (uint8_t *)in_buf;
		linelen = strlen(in_buf);
		xz_stream.avail_in = linelen;
		filelen += linelen;

		/*printf("Read '%.*s' -> %zu chars\n", 
		 (int)xz_stream.avail_in - 1, xz_stream.next_in, xz_stream.avail_in
		);*/

		rtn = lzma_code(&xz_stream, LZMA_RUN);
		in_buf[linelen] = '\0';
		/*printf("Read %zu bytes, coding returned %s, output has %lu bytes\n", 
		 linelen, lzma_ret_code[rtn], BUFFER_SIZE - xz_stream.avail_out
		);*/

		/* We have to buffer the write process here too... */
		if (xz_stream.avail_out == 0) {
			fwrite(out_buf, sizeof(uint8_t), BUFFER_SIZE, outfh);
			printf("At %lu bytes of input, wrote %d bytes to disk\n",
			 filelen, BUFFER_SIZE
			);
			
			/* reset the output buffer */
			xz_stream.next_out = out_buf;
			xz_stream.avail_out = BUFFER_SIZE;
		}
		
	}
	fclose(infh);
	printf("Closed input, read %lu bytes\n", filelen);
	
	/* Tell LZMA to finalise its compression */
	for (;;) {
		rtn = lzma_code(&xz_stream, LZMA_FINISH);
		if (xz_stream.avail_out == 0) {
			printf("Buffer full when finalising, got %s, writing %d bytes\n",
			 lzma_ret_code[rtn], BUFFER_SIZE
			);
			/* Write the buffer to it */
			fwrite(out_buf, sizeof(uint8_t), BUFFER_SIZE, outfh);
			xz_stream.next_out = out_buf;
			xz_stream.avail_out = BUFFER_SIZE;
		}
		
		if (rtn == LZMA_STREAM_END) {
			fwrite(out_buf, sizeof(uint8_t), BUFFER_SIZE - xz_stream.avail_out, outfh);
			printf("Final write of %lu bytes\n", BUFFER_SIZE - xz_stream.avail_out);
			break;
		}
		
		if (rtn > 0) {
			printf("Compression fail with %s\n", lzma_ret_code[rtn]);
			break;
		}
	}
	
	fclose(outfh);
	
	/* Free up our allocated memory */
	talloc_free(in_buf);
	talloc_free(out_buf);
	talloc_free(outname);
	
	/* Finish up */
	printf("Talloc report when finishing encoding, after freeing memory:\n");
	talloc_report_full(context, stderr);
	return 0;
}

int main(int argc, char *argv[]) {

    talloc_enable_leak_report();
    context = talloc_init("main test-xz context");
    cfile_set_context(context);
    int n;

	/* input file is our own code */
	if (argc > 1) {
		for (n=1; n<argc; n++) {
			encode(argv[n]);
		}
	} else {
		encode("test-xz.c");
	}
	
	talloc_report_full(context, stderr);
    talloc_free(context);
    return EXIT_SUCCESS;
}
/*flonge*/
