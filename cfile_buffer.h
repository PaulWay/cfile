/*
 * cfile_buffer.h
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

#ifndef CFILE_BUFFER_H
#define CFILE_BUFFER_H

#include "cfile.h"

/*! \brief Internal buffer handling structure
 * 
 * Both bzip2 and xz have no 'fgets' or 'fgetc' equivalents.  For reads, we
 * need to have our own internal buffer that we can use the decompression
 * routines to put data into, and then read uncompressed data from until
 * we need more, and so on.  This allows to handle this independently of
 * the compression type, so as to not duplicate code.
 */

typedef struct cfile_buffer_struct {
	/*! a read buffer for doing gets */
    char *buffer;
    /*! the length of the buffer we've read */
    size_t buflen;
    /*! our position in the buffer */
    size_t bufpos;
    /*! a function to read more into this buffer */
    size_t (*read_into_buffer)(void *private, size_t size, const char* buffer);
} cfile_buffer;

/*! brief Initialise the buffer structure
 * 
 * This routine does the base work of allocating the buffer and filling
 * out its fields.
 */

cfile_buffer *cfile_buffer_alloc(
	const void *context,
	size_t (*read_into_buffer)(void *private, size_t size, const char* buffer)
);

#endif /* CFILE_BUFFER_H */
/* vim: set ts=4 sw=4 et : */
