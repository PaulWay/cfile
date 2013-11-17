/*
 * cfile_buffer.c
 * This file is part of The PaulWay Libraries
 *
 * Copyright (C) 2006 Paul Wayper <paulway@mabula.net>
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

#include "stdbool.h"
#include "stddef.h"
#include "talloc.h"
#include "string.h"

#include "cfile.h"
#include "cfile_buffer.h"

/* Since we do this a lot: */
#define READ_BUFFER \
    bp->bufpos = 0; \
    bp->buflen = bp->read_into_buffer(private);


/*! brief Initialise the buffer structure
 * 
 * This routine does the base work of allocating the buffer and filling
 * out its fields.
 */

cfile_buffer *cfile_buffer_alloc(
	const void *context,
    size_t size,
	size_t (*read_into_buffer)(cfile *private)
) {
    cfile_buffer *buf = talloc(context, cfile_buffer);
    if (!buf) {
        return NULL;
    }
    
    buf->buffer = talloc_array(buf, char, size);
    if (!buf->buffer) {
        talloc_free(buf);
        return NULL;
    }
    buf->bufsize = size;
    buf->buflen = 0;
    buf->bufpos = 0;
    buf->read_into_buffer = read_into_buffer;
    return buf;
}

/*! \brief Read one character from the buffer
 * 
 * This requests more data from the buffer if necessary, then returns the
 * current character.
 * 
 * This can be used as the basis of fgets, but hopefully a more efficient
 * implementation of the latter can be achieved.
 * 
 * \param bp the cfile buffer structure pointer
 * \param private a pointer to the implementation's private data.  This is
 * then passed to the supplied read_into_buffer, which can then cast it back.
 */
char buf_fgetc(cfile_buffer *bp, cfile *private) {
    if (bp->buflen == bp->bufpos) {
        READ_BUFFER;
        if (bp->buflen <= 0) return EOF;
    }
    return bp->buffer[bp->bufpos++]; /* Ah, the cleverness of postincrement */
}

/*! \brief Read a string from the buffer until newline or EOF.
 * 
 * The normal fgets method as implemented in glibc uses fgetc to get
 * characters one at a time from the file, with no knowledge of any
 * underlying buffer.  Since we've got one here, we try to implement a
 * generic fgets replacement by going through the buffer looking for the
 * end of line.
 */

char *buf_fgets(cfile_buffer *bp, char *str, size_t len, cfile *private) {
    /* Implementation modified from glibc's stdio.c */
    char *ptr = str;

    if (len <= 0) return NULL;

    while (--len) {
        /* If we need more string, then get it */
        if (bp->buflen == bp->bufpos) {
            READ_BUFFER;
            if (bp->buflen <= 0) {
                if (ptr == str) return NULL;
                break;
            }
        }

        /* Put next character into target, check for end of line */
        if ((*ptr++ = bp->buffer[bp->bufpos++]) == '\n') break;
    }

    *ptr = '\0';
    return str;
}

/*! \brief Fill a chunk of memory from the buffer.
 * 
 * Copy len bytes from the buffer to the output pointer, refilling the buffer
 * when necessary.
 * 
 * \param bp the buffer to read from
 * \param target the memory to write to
 * \param len the number of bytes to read
 * \param private the private information of the implementation
 * \returns The number of bytes read, which may be less than requested if
 * we ran out of file.
 */
size_t buf_fread(cfile_buffer *bp, void *target, size_t len, cfile *private) {
    char *ptr = target;
    size_t this_chunk, total_copied;

    if (len <= 0) return 0;
    
    for (;;) {
        /* fill buffer if required */
        if (bp->bufpos == bp->buflen) {
            READ_BUFFER;
            if (bp->buflen == 0) {
                if (ptr == target) { return 0; }
                break;
            }
        }
        
        /* I'm sure there's a more efficient way to do this */
        this_chunk = bp->buflen - bp->bufpos;
        this_chunk = (len <= this_chunk) ? len : this_chunk;
        memcpy(ptr, bp->buffer + bp->bufpos, this_chunk);
        ptr += this_chunk;
        len -= this_chunk;
        bp->bufpos += this_chunk;
        total_copied += this_chunk;
    }
    
    return total_copied;
}

/*! \brief Is the buffer empty?
 * 
 * Returns true if last read of uncompressed data has zero bytes - in other 
 * words, if we cannot return another character from the buffer.
 */

bool buf_empty(cfile_buffer *bp) {
    return bp->buflen == 0;
}

/* vim: set ts=4 sw=4 et : */
