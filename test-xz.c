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

typedef struct bufstruct_struct {
	char *buffer;
	size_t bufsize;
	size_t buflen;
	size_t bufpos;
} bufstruct;

static const char *lzma_ret_code[12] = {
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

int write_one_line(char *in_buf, size_t linelen, lzma_stream *xz_stream, 
 uint8_t *out_buf, FILE *outfh);

int write_one_line(char *in_buf, size_t linelen, lzma_stream *xz_stream, 
 uint8_t *out_buf, FILE *outfh) {
	lzma_ret rtn;
	xz_stream->next_in = (uint8_t *)in_buf;
	xz_stream->avail_in = linelen;

	/*printf("Read '%.*s' -> %zu chars\n", 
	 (int)xz_stream->avail_in - 1, xz_stream->next_in, xz_stream->avail_in
	);*/

	rtn = lzma_code(xz_stream, LZMA_RUN);
	printf("Read %zu bytes, coding returned %s, output has %lu bytes\n", 
	 linelen, lzma_ret_code[rtn], BUFFER_SIZE - xz_stream->avail_out
	);
	if (rtn != LZMA_OK) {
		printf("   Error %s from lzma_code - finishing up.\n",
		 lzma_ret_code[rtn]
		);
		return 0;
	}

	/* We have to buffer the write process here too... */
	if (xz_stream->avail_out == 0) {
		for (;;) {
			fwrite(out_buf, sizeof(uint8_t), BUFFER_SIZE, outfh);
			printf("Wrote %d bytes to disk, avail_in = %zu\n",
			 BUFFER_SIZE, xz_stream->avail_in
			);
			/* reset the output buffer */
			xz_stream->next_out = out_buf;
			xz_stream->avail_out = BUFFER_SIZE;
			rtn = lzma_code(xz_stream, LZMA_RUN);
			printf("   after lzma_code in write loop, avail_in = %zu, avail_out = %zu\n",
			 xz_stream->avail_in, xz_stream->avail_out
			);
			if (xz_stream->avail_out > 0) {
				printf("Exiting loop\n");
				break;
			}
		}
		
	}
	return 1;
}

/* Stolen from ccan/str/str.h */
static inline bool strends(const char *str, const char *postfix)
{
       if (strlen(str) < strlen(postfix))
               return false;

       return !strcmp(str + strlen(str) - strlen(postfix), postfix);
}

int encode(const char *filename);
int encode(const char *filename) {
	lzma_stream xz_stream = LZMA_STREAM_INIT;
	lzma_ret rtn = 0;
	char *in_buf = NULL;
	uint8_t *out_buf = NULL;
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
		if (!write_one_line(in_buf, strlen(in_buf), &xz_stream, out_buf, outfh)) {
			break;
		}
		filelen += strlen(in_buf);
	}
	fclose(infh);
	printf("Closed input, read %lu bytes == %lu bytes\n", filelen, xz_stream.total_in);
	
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
	lzma_end(&xz_stream);
	
	/* Finish up */
	printf("Talloc report when finishing encoding, after freeing memory:\n");
	talloc_report_full(context, stderr);
	return 0;
}

ssize_t decompress_from_file(
 uint8_t *in_buf, bufstruct *buf, lzma_stream *xz_stream, FILE *in_fh);

ssize_t decompress_from_file(
 uint8_t *in_buf, bufstruct *buf, lzma_stream *xz_stream, FILE *in_fh) {
	/* Implementation of fgets modified from glibc's stdio.c */
    char *ptr = buf->buffer;
    size_t read_size = 0;
    ssize_t put_size = 0;
	lzma_ret rtn = 0;
	size_t len = buf->bufsize;

	printf("Decompress_into_file()\n");
	
    if (len <= 0) return 0;

    while (--len) {
		put_size++;
		
        /* If we need more string, then get it */
        if (buf->bufpos == buf->buflen) {
			printf("      out of buffer, fetch more from lzma\n");

			if (xz_stream->avail_in == 0) {
				printf("      lzma empty, fetch more from file...");
				/* No: give it more from the file */
				read_size = fread(in_buf, sizeof(uint8_t), BUFFER_SIZE, in_fh);
				printf("      read %zu bytes\n", read_size);
				if (read_size == 0) {
					printf("      no more from file: finish here.\n");
					if (ptr == buf->buffer) {
						*ptr = '\0';
						return 0;
					}
					break;
				} 
				xz_stream->next_in   = in_buf;
				xz_stream->avail_in  = read_size;
			}
			xz_stream->next_out  = (uint8_t *)buf->buffer;
			xz_stream->avail_out = buf->bufsize;
			rtn = lzma_code(xz_stream, LZMA_RUN);
			if (rtn != LZMA_OK) {
				printf("Error in decode of %zu byte block: %s(%d)\n",
				 read_size, lzma_ret_code[rtn], rtn);
				/* Do something else? */
			}
			printf("      lzma given %zu bytes to decode, returned %s[%d],"
			 " avail_in=%zu, avail_out=%zu\n",
			 read_size, lzma_ret_code[rtn], rtn, xz_stream->avail_in,
			 xz_stream->avail_out);
			buf->bufpos = 0;
			buf->buflen = buf->bufsize - xz_stream->avail_out;
        }

        /* Put next character into target, check for end of line */
        if ((*ptr++ = buf->buffer[(buf->bufpos)++]) == '\n') break;
		printf("   len=%zu, put_size=%zu, buflen=%zu, bufpos=%zu, char=%c\n",
		 len, put_size, buf->buflen, buf->bufpos, buf->buffer[buf->bufpos - 1]);
    }

    *ptr = '\0';
	return put_size-1;
}

int decode(const char *filename);
int decode(const char *filename) {
	lzma_stream xz_stream = LZMA_STREAM_INIT;
	lzma_ret rtn = 0;
	uint8_t *in_buf = NULL;
	bufstruct *out_buf;
	ssize_t filelen = 0, readlen = 0;
	FILE *in_fh, *out_fh;
	char *outname = NULL;

    in_buf  = talloc_array(context, uint8_t, BUFFER_SIZE);
    out_buf = talloc(context, bufstruct);
    out_buf->buffer = talloc_array(out_buf, char, BUFFER_SIZE);
    out_buf->bufsize = BUFFER_SIZE;
    out_buf->buflen = 0;
    out_buf->bufpos = 0;
    
	/* Initialise decoder.  Put input and output buffer information in
	 * structure and then call lzma_auto_decoder to let liblzma work
	 * out what we're decoding. */
	printf("Using auto decoder... ");
	xz_stream.next_out = (uint8_t *)out_buf->buffer;
	xz_stream.avail_out = BUFFER_SIZE;
	rtn = lzma_auto_decoder(&xz_stream, UINT64_MAX, LZMA_CONCATENATED);
	printf("Returned %s\n", lzma_ret_code[rtn]);

	/* open the input file */
	in_fh = fopen(filename, "r");
	if (!in_fh) {
		fprintf(stderr, "Failed to open %s: %s(%d)\n",
		 filename, strerror(errno), errno
		);
		return errno;
	}

	/* Open the output file - clone the truncated name and terminate*/
	outname = talloc_memdup(context, filename, strlen(filename)-3+1);
	outname[strlen(filename)-3] = '\0';
	printf("Writing %s\n", outname);
	out_fh = fopen(outname, "w");
	if (!out_fh) {
		fprintf(stderr, "Failed to open %s: %s(%d)\n",
		 filename, strerror(errno), errno
		);
		return errno;
	}
	
	/* Read blocks from the file, decompressing each one as we go. */
	for (;;) {
		readlen = decompress_from_file(in_buf, out_buf, &xz_stream, in_fh);
		printf("got %zu bytes from decompress\n", readlen);
		fwrite(out_buf->buffer, sizeof(char), readlen, out_fh);
		if (readlen == 0) break;
		filelen += readlen;
	}
	fclose(in_fh);
	
	fclose(out_fh);
	printf("Closed output, wrote %lu bytes == %lu bytes\n", filelen, xz_stream.total_out);
	
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
    int n;

    talloc_enable_leak_report();
    context = talloc_init("main test-xz context");
    cfile_set_context(context);

	/* input file is our own code */
	if (argc > 1) {
		for (n=1; n<argc; n++) {
			if (strends(argv[n], ".xz")) {
				decode(argv[n]);
			} else {
				encode(argv[n]);
			}
		}
	} else {
		encode("test-xz.c");
	}
	
	talloc_report_full(context, stderr);
    talloc_free(context);
    return EXIT_SUCCESS;
}
