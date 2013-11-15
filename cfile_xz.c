/*
 * cfile.c
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

#include <string.h>
#include <errno.h>
#include <talloc.h>
#include <lzma.h>

#include "cfile_private.h"
#include "cfile_buffer.h"
#include "cfile_xz.h"

/* Predeclare function calls */
off_t   xz_size(cfile *fp);
bool    xz_eof(cfile *fp);
char   *xz_gets(cfile *fp, char *str, size_t len);
ssize_t xz_read(cfile *fp, void *ptr, size_t size, size_t num);
ssize_t xz_write(cfile *fp, const void *ptr, size_t size, size_t num);
int     xz_flush(cfile *fp);
int     xz_close(cfile *fp);

/*! \brief The xz file structure
 *
 * We only need to store the actual (zlib) file pointer.
 */
typedef struct cfile_xz {
    cfile inherited; /*< our inherited function table */
    FILE *xf;        /*< the actual xz file - just a standard handle */
    lzma_stream stream; /*< the LZMA stream information */
    bool writing;    /*< are we writing this file (i.e. encoding it),
                         or reading (i.e. decoding)? */
    cfile_buffer *buffer; /*< our buffer structure */
} cfile_xz;

static const cfile_vtable xz_cfile_table;

/*! The size of the character buffer for reading lines from xz files.
 *
 *  This isn't really a file cache, just a way of saving us single-byte
 *  calls to bzread.
 */
#define XZ_BUFFER_SIZE 4096

/*! \brief Read callback function to read more data for buffer
 * 
 * This provides uncompressed data to the generic buffer implementation.
 */

size_t xz_read_into_buffer(void *private, const char* buffer, size_t size);
size_t xz_read_into_buffer(void *private, const char* buffer, size_t size) {
    cfile_xz *cfxp = (cfile_xz *)private;

}

/*! \brief Open a xz file for reading or writing
 *
 *  Open the given file using the given mode.  Opens the file and
 *  returns a cfile handle to it.  Mode must start with 'r' or 'w'
 *  to read or write (respectively) - other modes are not expected
 *  to work.
 *
 * \return A successfully created file handle, or NULL on failure.
 */
cfile *xz_open(const char *name, /*!< The name of the file to open */
               const char *mode) /*!< "r" to specify reading, "w" for writing. */
{
    cfile_xz *cfxp;
    FILE *own_file;
	lzma_ret rtn = 0;
    
    if (!(own_file == fopen(name, mode))) {
        return NULL;
    }
    
    cfxp = (cfile_xz *)cfile_alloc(&xz_cfile_table, name, mode);
    if (!cfxp) {
        errno = ENOMEM;
        fclose(own_file);
        return NULL;
    }

    cfxp->buffer = cfile_buffer_alloc(cfxp, XZ_BUFFER_SIZE, xz_read_into_buffer);
    if (!cfxp->buffer) {
        errno = ENOMEM;
        fclose(own_file);
        talloc_free(cfxp);
        return NULL;
    }
    
    cfxp->lzma_stream = LZMA_STREAM_INIT;
    cfxp->writing = (mode[0] == 'w');
    if (cfxp->writing) {
        rtn = lzma_easy_encoder(cfxp->stream, 9, LZMA_CHECK_CRC64);
        cfxp->stream.next_out = cfxp->buffer.buffer;
        cfxp->stream.avail_out = XZ_BUFFER_SIZE;
    } else {
        /* Allow concatenated files to be read - changes read semantics */
        rtn = lzma_auto_decoder(cfxp->stream, UINT64_MAX, LZMA_CONCATENATED);
        cfxp->stream.next_in =  cfxp->buffer.buffer;
        cfxp->stream.avail_in = XZ_BUFFER_SIZE;
    }
    
    if (rtn != LZMA_OK) {
        errno = EINVAL;
        fclose(own_file);
        talloc_free(cfxp); /* includes buffer */
        return NULL;
    }
    
    return (cfile *)cfxp;
}

/*! \brief Returns the _uncompressed_ file size
 *
 * \param fp The file handle to check
 * \return The number of bytes in the uncompressed file.
 */

off_t xz_size(cfile *fp) {
    cfile_xz *cfxp = (cfile_xz *)fp;
    
    /* See source of xz for this - basically read the footer off the end of
     * the file and then try to find further footers earlier in the file */
}

/*! \brief Returns true if we've reached the end of the file being read.
 *
 *  There are two ways of knowing whether we're at the end of the xz file:
 *  One is by checking the EOF state of the underlying file handle,
 *  the other is by finding out that the last buffer read got zero bytes.
 * \param fp The file handle to check.
 * \return True (1) if the file has reached EOF, False (0) if not.
 */

bool xz_eof(cfile *fp) {
    cfile_xz *cfxp = (cfile_xz *)fp;
    if (feof(cfxp->fp)) return 1;
    if (buf_empty(cfxp->buffer)) return 1;
    return 0;
}

/*! \brief Get a string from the file, up to a maximum length or newline.
 *
 *  liblzma doesn't provide an equivalent to gets, so we use our generic 
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
 
char *xz_gets(cfile *fp, char *str, size_t len) {
    cfile_xz *cfxp = (cfile_xz *)fp;
    return buf_fgets(cfxp->buffer, str, len, (void *)cfxp);
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

int xz_vprintf(cfile *fp, const char *fmt, va_list ap)
  __attribute ((format (printf, 2, 0)));

int xz_vprintf(cfile *fp, const char *fmt, va_list ap) {
    int rtn;
    char *buf = talloc_vasprintf(fp, fmt, ap);
    rtn = xz_write(fp, buf, strlen(buf));
    talloc_free(buf);
    return rtn;
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
 
ssize_t xz_read(cfile *fp, void *ptr, size_t size, size_t num) {
    cfile_xz *cfxp = (cfile_xz *)fp;
    return BZ2_bzread(cfxp->bp, ptr, size * num);
}

/*! \brief Write a block of data from the file.
 *
 *  Writes a given number of structures of a specified size into the
 *  file from the memory pointer given.
 * \param fp The file handle to write into.
 * \param ptr The memory to read from.
 * \param size The size of each structure in bytes.
 * \param num The number of structures to write.
 * \return The success of the file write operation.
 */
 
ssize_t xz_write(cfile *fp, const void *ptr, size_t size, size_t num) {
    cfile_xz *cfxp = (cfile_xz *)fp;
    cfxp->stream.next_in = ptr;
    cfxp->stream.avail_in = size * num;
    ssize_t written = 0;
    for (;;) {
        lzma_ret rtn = lzma_code(&cfxp->stream, LZMA_RUN);
        if (rtn != LZMA_OK) {
            /* do anything else? */
            return 0;
        }
        /* Leave early if there's still room for more compressed data */
        if (cfxp->stream.avail_out == cfxp->buffer.bufsize) break;
        /* Write the entire buffer, reset pointer and available size */
        fwrite(cfxp->stream.next_out, sizeof(uint8_t),
         cfxp->buffer.bufsize, cfxp->xf);
        cfxp->stream.next_out = cfxp->buffer.buffer;
        cfxp->stream.avail_out = cfxp->buffer.bufsize;
    }
    return rtn;
}

/*! \brief Flush the file's output buffer.
 *
 *  This function flushes any data passed to write or printf but not
 *  yet written to disk.  If the file is being read, it has no effect.
 * \param fp The file handle to flush.
 * \return the success of the file flush operation.
 */
 
int xz_flush(cfile *fp) {
    cfile_xz *cfxp = (cfile_xz *)fp;
    return BZ2_bzflush(cfxp->bp);
}

/*! \brief Close the given file handle.
 *
 *  This function frees the memory allocated for the file handle and
 *  closes the associated file.
 * \param fp The file handle to close.
 * \return the success of the file close operation.
 */
 
int xz_close(cfile *fp) {
    cfile_xz *cfxp = (cfile_xz *)fp;

}

/*! \brief The function dispatch table for xz files */

static const cfile_vtable xz_cfile_table = {
    sizeof(cfile_xz),
    xz_size,
    xz_eof,
    xz_gets,
    xz_vprintf,
    xz_read,
    xz_write,
    xz_flush,
    xz_close,
    "xz file"
};


