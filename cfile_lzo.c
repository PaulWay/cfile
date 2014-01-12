/*
 * cfile_lzo - compressed file read/write library
 * Copyright (C) 2012 Peter Miller
 * Copyright (C) 2013-2014 Paul Wayper <paulway@mabula.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <errno.h>
#include <talloc.h>
#include <lzo/lzoconf.h>
#include <lzo/lzo1x.h>

#include "cfile_private.h"
#include "cfile_buffer.h"
#include "cfile_lzo.h"

/* Predeclare function calls */
static off_t   cfile_lzo_size(cfile *fp);
static bool    cfile_lzo_eof(cfile *fp);
static char   *cfile_lzo_gets(cfile *fp, char *str, size_t len);
static ssize_t cfile_lzo_read(cfile *fp, void *ptr, size_t size, size_t num);
static ssize_t cfile_lzo_write(cfile *fp, const void *ptr, size_t size, size_t num);
static int     cfile_lzo_flush(cfile *fp);
static int     cfile_lzo_close(cfile *fp);

/*! \brief The lzo file structure
 *
 * Like xz, the lzo compress/decompress routines simply operate on bytes;
 * so we have to handle our own file and input/output memory buffers.
 */
typedef struct cfile_lzo {
    cfile inherited; /*< our inherited function table */
    FILE *lzof;      /*< the actual lzo file - just a standard handle */
    lzo_voidp wrkmem; /*< LZO working memory - null if decompressing,
                         allocated to LZO1X_99_MEM_COMPRES if compressing. */
    bool writing;    /*< are we writing this file (i.e. encoding it),
                         or reading (i.e. decoding)? */
    cfile_buffer *buffer; /*< our buffer structure */
} cfile_lzo;

static const cfile_vtable lzo_cfile_table;

/*! The size of the character buffer for reading lines from lzo files.
 *
 *  Used on both input and output.
 */
#define LZO_BUFFER_SIZE 4096

/*! \brief Read callback function to read more data for buffer
 * 
 * This provides uncompressed data to the generic buffer implementation.
 */

static size_t lzo_read_into_buffer(cfile *private);
static size_t lzo_read_into_buffer(cfile *private) {
    (void)private;
	return 0;
}

/*! \brief Returns the _uncompressed_ file size
 *
 * \param fp The file handle to check
 * \return The number of bytes in the uncompressed file.
 */

static off_t cfile_lzo_size(cfile *fp) {
    (void)fp;
    return 0;
}

/*! \brief Returns true if we've reached the end of the file being read.
 *
 *  There are two ways of knowing whether we're at the end of the xz file:
 *  One is by checking the EOF state of the underlying file handle,
 *  the other is by finding out that the last buffer read got zero bytes.
 * \param fp The file handle to check.
 * \return True (1) if the file has reached EOF, False (0) if not.
 */

static bool cfile_lzo_eof(cfile *fp) {
    (void)fp;
    return 1;
}

/*! \brief Get a string from the file, up to a maximum length or newline.
 *
 *  lzo doesn't provide an equivalent to gets, so we use our generic 
 *  buffer implementation.
 * \param fp The file handle to read from.
 * \param str An array of characters to read the file contents into.
 * \param len The maximum length, plus one, of the string to read.  In
 *  other words, if this is 10, then fgets will read a maximum of nine
 *  characters from the file.  The character after the last character
 *  read is always set to \\0 to terminate the string.  The newline
 *  character is kept on the line if there was room to read it.
 * \see bz_fgetc
 * \return A pointer to the string thus read.
 */
 
static char *cfile_lzo_gets(cfile *fp, char *buf, size_t bufsiz) {
    (void)fp;
    (void)buf;
    (void)bufsiz;
    return 0;
}

/*! \brief Print a formatted string to the file, from another function
 *
 *  The standard vfprintf implementation.  For those people that have
 *  to receive a '...' argument in their own function and send it to
 *  a cfile.
 *
 * \param fp The file handle to write to.
 * \param fmt The format string to print.
 * \param ap The compiled va_list of parameters to print.
 * \return The success of the file write operation.
 * \todo Should we be reusing a buffer rather than allocating one each time?
 */

static int cfile_lzo_vprintf(cfile *fp, const char *fmt, va_list ap)
    __attribute ((format (printf, 2, 0)));

static int cfile_lzo_vprintf(cfile *fp, const char *fmt, va_list ap) {
    (void)fp;
    return vsnprintf(0, 0, fmt, ap);
}

/*! \brief Read a block of data from the file.
 *
 *  Reads a given number of structures of a specified size from the
 *  file into the memory pointer given.  The destination memory must
 *  be allocated first.  Some read functions only specify one size,
 *  we use two here because that's what fread requires (and it's
 *  better for the programmer anyway IMHO).
 * \param fp The file handle to read from.
 * \param ptr The memory to write into.
 * \param size The size of each structure in bytes.
 * \param num The number of structures to read.
 * \return The success of the file read operation.
 */
 
static ssize_t cfile_lzo_read(cfile *fp, void *ptr, size_t size, size_t num) {
    (void)fp;
    (void)ptr;
    (void)size;
    (void)num;
    return 0;
}

/*! \brief Write a block of data from the file.
 *
 *  Writes a given number of structures of a specified size into the
 *  file from the memory pointer given.
 * \param fp The file handle to write into.
 * \param ptr The memory to read from.
 * \param size The size of each structure in bytes.
 * \param num The number of structures to write.
 * \return The number of _items_ written (num, not size)
 */
 
static ssize_t
cfile_lzo_write(cfile *fp, const void *ptr, size_t size, size_t num) {
    (void)fp;
    (void)ptr;
    (void)size;
    return num;
}

/*! \brief Flush the file's output buffer.
 *
 *  This function flushes any data passed to write or printf but not
 *  yet written to disk.  If the file is being read, it has no effect.
 *  This uses LZMA_FULL_FLUSH, which writes the current block but does
 *  not attempt to force all unbuffered data out.  There may be some
 *  impact on compression ratio, but not as much as LZMA_SYNC_FLUSH.
 * \param fp The file handle to flush.
 * \return the success of the file flush operation.
 */
 
static int cfile_lzo_flush(cfile *fp) {
    (void)fp;
    return 0;
}

/*! \brief Close the given file handle.
 *
 *  This function frees the memory allocated for the file handle and
 *  closes the associated file.
 * \param fp The file handle to close.
 * \return the success of the file close operation.
 */
 
static int cfile_lzo_close(cfile *fp) {
    (void)fp;
    return 0;
}

/*! \brief The function dispatch table for lzo files */

static const cfile_vtable cfile_lzo_vtable = {
    sizeof(cfile),
    cfile_lzo_size,
    cfile_lzo_eof,
    cfile_lzo_gets,
    cfile_lzo_vprintf,
    cfile_lzo_read,
    cfile_lzo_write,
    cfile_lzo_flush,
    cfile_lzo_close,
    "LZO file"
};

/*! \brief Open a xz file for reading or writing
 *
 *  Open the given file using the given mode.  Opens the file and
 *  returns a cfile handle to it.  Mode must start with 'r' or 'w'
 *  to read or write (respectively) - other modes are not expected
 *  to work.
 *
  * @param pathname
  *     The name of the file to open.
  * @param mode
  * 	The mode to use for file operations (read or write).
  * @returns
  *     The new file handle
 */
cfile * cfile_lzo_open(const char *pathname, const char *mode)
{
    cfile_lzo *cflzop;
    FILE *own_file;
    if (!(own_file = fopen(pathname, mode))) {
        goto lzo_open_error;
    }
    
    cflzop = (cfile_lzo *)cfile_alloc(&cfile_lzo_vtable, pathname, mode);
    if (!cflzop) {
        errno = ENOMEM;
        goto lzo_open_premalloc_error;
    }
    
    cflzop->lzof = own_file;

    cflzop->buffer = cfile_buffer_alloc(cflzop, LZO_BUFFER_SIZE, lzo_read_into_buffer);

    return (cfile *)cflzop;

/*lzo_open_postmalloc_error:
    talloc_free(cflzop);*/
lzo_open_premalloc_error:
    fclose(own_file);
lzo_open_error:
    return NULL;
}


/* vim: set ts=4 sw=4 et : */
