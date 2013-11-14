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

#include "stddef.h"

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
 * 
 * The buffer has a total allocation, but sometimes (e.g. at end of file) the
 * read may not fill the buffer.  Therefore we need to know the total size
 * of the buffer, how much data is actually in it, and our place within that
 * valid data.
 * 
 * The read_into_buffer function is called when the buffer needs more data.
 * It is given a private pointer to the underlying implementations' own data
 * structure (which it has from the CFile pointer), the maximum number of
 * characters to read, and the buffer into which the data shall be put.  It
 * should then return the actual number of characters read.  This should
 * roughly map to what fgets, or your local alternative, gives you.
 */

typedef struct cfile_buffer_struct {
    /*! a read buffer for doing gets */
    char *buffer;
    /*! the memory allocated to this buffer */
    size_t bufsize;
    /*! the length of the buffer we've read */
    size_t buflen;
    /*! our position in the buffer */
    size_t bufpos;
    /*! a function to read more into this buffer */
    size_t (*read_into_buffer)(cfile *private, const char* buffer, size_t size);
} cfile_buffer;

/*! \brief Initialise the buffer structure
 * 
 * This routine does the base work of allocating the buffer and filling
 * out its fields.
 */

cfile_buffer *cfile_buffer_alloc(
    /*! The talloc context to allocate against - usually your own file pointer */
    const void *context,
    /*! The size of the buffer to allocate, in bytes */
    size_t size,
    /*! The read function you want to use */
    size_t (*read_into_buffer)(cfile *private, const char* buffer, size_t size)
);

/*! \brief Read one character from the buffer
 * 
 * This requests more data from the buffer if necessary, then returns the
 * current character.
 * 
 * This can be used as the basis of fgets, but hopefully a more efficient
 * implementation of the latter can be achieved.
 */
char buf_fgetc(cfile_buffer *bp, cfile *private);

/*! \brief Read a string from the buffer until newline or EOF.
 * 
 * The normal fgets method as implemented in glibc uses fgetc to get
 * characters one at a time from the file, with no knowledge of any
 * underlying buffer.  Since we've got one here, we try to implement a
 * generic fgets replacement by going through the buffer looking for the
 * end of line.
 */

char *buf_fgets(cfile_buffer *bp, char *str, size_t len, cfile *private);

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
size_t buf_fread(cfile_buffer *bp, void *target, size_t len, cfile *private);

/*! \brief Is the buffer empty?
 * 
 * Returns true if last read of uncompressed data has zero bytes - in other 
 * words, if we cannot return another character from the buffer.
 */

bool buf_empty(cfile_buffer *bp);

#endif /* CFILE_BUFFER_H */
/* vim: set ts=4 sw=4 et : */
