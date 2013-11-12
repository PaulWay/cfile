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

#include "cfile_buffer.h"

/*! brief Initialise the buffer structure
 * 
 * This routine does the base work of allocating the buffer and filling
 * out its fields.
 */

cfile_buffer *cfile_buffer_alloc(
	const void *context,
    size_t size,
	size_t (*read_into_buffer)(void *private, const char* buffer, size_t size)
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
 */
char buf_fgetc(cfile_buffer *bp, void *private) {
    /* Find buffer structure */
    if (bp->buflen == bp->bufpos) {
        bp->bufpos = 0;
        bp->buflen = bp->read_into_buffer(private, bp->buffer, bp->bufsize);
        if (bp->buflen <= 0) return EOF;
    }
    return bp->buffer[bp->bufpos++]; /* Ah, the cleverness of postincrement */
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
