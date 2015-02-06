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
static off_t   xz_size(cfile *fp);
static bool    xz_eof(cfile *fp);
static char   *xz_gets(cfile *fp, char *str, size_t len);
static ssize_t xz_read(cfile *fp, void *ptr, size_t size, size_t num);
static ssize_t xz_write(cfile *fp, const void *ptr, size_t size, size_t num);
static int     xz_flush(cfile *fp);
static int     xz_close(cfile *fp);

/*! \brief The xz file structure
 *
 * Because lzma is a stream compression library, we have to handle the
 * file pointer and input/output buffering outselves.  Yay.
 */
typedef struct cfile_xz {
    cfile inherited; /*< our inherited function table */
    FILE *xf;        /*< the actual xz file - just a standard handle */
    lzma_stream stream; /*< the LZMA stream information */
    bool writing;    /*< are we writing this file (i.e. encoding it),
                         or reading (i.e. decoding)?  Only used when
                         closing a file, since we have to flush the buffer.*/
    cfile_buffer *buffer; /*< our buffer structure */
    uint8_t *dec_buf; /*< temporary storage to decode reads into; when writing,
                          the user will provide us with memory to read from.*/
} cfile_xz;

static const cfile_vtable xz_cfile_table;

/*! The size of the character buffer for reading lines from xz files.
 *
 *  Used on both input and output.
 */
#define XZ_BUFFER_SIZE 4096

/*! \brief Read callback function to read more data for buffer
 * 
 * This provides uncompressed data to the generic buffer implementation.
 */

static size_t xz_read_into_buffer(cfile *private);
static size_t xz_read_into_buffer(cfile *private) {
    cfile_xz *cfxp = (cfile_xz *)private;
    size_t from_file = 0;
    lzma_ret rtn;
    
    /*printf("XZRIB(avail_in=%zu, avail_out=%zu, buflen=%zu, bufpos=%zu)\n",
     cfxp->stream.avail_in, cfxp->stream.avail_out, cfxp->buffer->buflen, cfxp->buffer->bufpos);*/
    /* If we need more data from the file, get it. */
    if (cfxp->stream.avail_in == 0) {
        from_file = fread(cfxp->dec_buf, sizeof(uint8_t), XZ_BUFFER_SIZE, cfxp->xf);
        cfxp->stream.next_in = cfxp->dec_buf;
        cfxp->stream.avail_in = from_file;
        /*printf("XZRIB: fetched %zu more from file into decode buffer\n",
         from_file);*/
    }

    /* Now decode the next buffer full of data */
    cfxp->stream.next_out = (uint8_t *)cfxp->buffer->buffer;
    cfxp->stream.avail_out = cfxp->buffer->bufsize;
    rtn = lzma_code(&cfxp->stream, LZMA_RUN);
    if (rtn != LZMA_OK) {
        /* What? */
        /*printf("XZRIB: got %d from lzma_code, bailing out\n", rtn);*/
        return 0;
    }
    
    /*printf("XZRIB: finished read, put %zu bytes in buffer\n", 
     cfxp->buffer->bufsize - cfxp->stream.avail_out);*/
    return cfxp->buffer->bufsize - cfxp->stream.avail_out;
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
    
    if (!(own_file = fopen(name, mode))) {
        goto xz_open_error;
    }
    
    cfxp = (cfile_xz *)cfile_alloc(&xz_cfile_table, name, mode);
    if (!cfxp) {
        errno = ENOMEM;
        goto xz_open_premalloc_error;
    }

    cfxp->xf = own_file;
    cfxp->writing = (mode[0] == 'w');

    cfxp->buffer = cfile_buffer_alloc(cfxp, XZ_BUFFER_SIZE, xz_read_into_buffer);
    if (!cfxp->buffer) {
        errno = ENOMEM;
        goto xz_open_postmalloc_error;
    }
    if (! cfxp->writing) {
        cfxp->dec_buf = talloc_array(cfxp, uint8_t, XZ_BUFFER_SIZE);
        if (!cfxp->dec_buf) {
            errno = ENOMEM;
            goto xz_open_postmalloc_error;
        }
    }
    
    /* Can't do cfxp->stream = (LZMA_STREAM_INIT); because of macros */
    memset((void *)&cfxp->stream, 0, sizeof(lzma_stream));
    if (cfxp->writing) {
        rtn = lzma_easy_encoder(&cfxp->stream, 9, LZMA_CHECK_CRC64);
        cfxp->stream.next_out = (uint8_t *)cfxp->buffer->buffer;
        cfxp->stream.avail_out = XZ_BUFFER_SIZE;
    } else {
        /* Allow concatenated files to be read - changes read semantics */
        rtn = lzma_auto_decoder(&cfxp->stream, UINT64_MAX, LZMA_CONCATENATED);
        cfxp->stream.next_in = (uint8_t *)cfxp->buffer->buffer;
        cfxp->stream.avail_in = 0;
    }
    
    if (rtn != LZMA_OK) {
        errno = EINVAL;
        goto xz_open_postmalloc_error;
    }

    return (cfile *)cfxp;
    
xz_open_postmalloc_error:
    talloc_free(cfxp); /* includes buffer, if allocated */
xz_open_premalloc_error:
    fclose(own_file);
xz_open_error:
    return NULL;
}

/*! \brief Returns the _uncompressed_ file size
 *
 * \param fp The file handle to check
 * \return The number of bytes in the uncompressed file.
 */

static off_t xz_size(cfile *fp) {
    /* cfile_xz *cfxp = (cfile_xz *)fp; */
    
    /* An attempt at reimplementing the rather complex index reading 
     * algorithm in xz/src/xz/index.c.  It's a shame this isn't offered
     * in the lzma library itself.  
     */
    FILE *my_fh;
    off_t size = 0;
    long pos;
    uint32_t *data;
    size_t data_read;
    const size_t max_data_read = 32; /* 32 uint32's - should be enough */
    lzma_stream *stream;
    lzma_stream_flags header_flags;
    lzma_stream_flags footer_flags;
    lzma_ret rtn;
    lzma_vli index_size;
    lzma_index *combined_index = NULL;
    lzma_index *this_index = NULL;

    data = talloc_array(fp, uint32_t, max_data_read);
    if (!data) {
        return 0;
    }
    stream = talloc_zero(data, lzma_stream);
    if (!stream) {
        talloc_free(data);
        return 0;
    }
    if (!(my_fh = fopen(fp->filename, "r"))) {
        talloc_free(data);
        return 0;
    }
    fseek(my_fh, 0, SEEK_END);
    pos = ftell(my_fh);
    if (pos < 2*LZMA_STREAM_HEADER_SIZE) {
        /* Not enough to contain a stream header and footer; exit now. */
        talloc_free(data);
        fclose(my_fh);
    }
    
    /* Each loop iteration decodes one Index */
    do {
        pos -= LZMA_STREAM_HEADER_SIZE;
        fseek(my_fh, pos, SEEK_SET);
        
        /* Locate stream footer, skipping over stream padding */
        /* Inefficient loop, maybe, but simpler logic. */
        for (;;) {
            /* Read entire header space...*/
            data_read = fread(data, 1, LZMA_STREAM_HEADER_SIZE, my_fh);
            /* Break once we hit something other than padding in the
             * *last* word */
            if (data[(LZMA_STREAM_HEADER_SIZE / sizeof(uint32_t))-1] != 0) break;
            /* Otherwise move back one 32-bit word and retry */
            pos -= sizeof(uint32_t);
            fseek(my_fh, pos, SEEK_SET);
        }
        
        /* Decode the stream footer */
        rtn = lzma_stream_footer_decode(&footer_flags, (uint8_t *)data);
        if (rtn != LZMA_OK) {
            break;
        }
        
        /* Check that the size of this index field looks sane */
        index_size = footer_flags.backward_size;
        if ((lzma_vli)(pos) < index_size + LZMA_STREAM_HEADER_SIZE) {
            break;
        }
        
        /* Move to beginning of index and decode */
        /* Skip memory limit check */
        pos -= index_size;
        fseek(my_fh, pos, SEEK_SET);
        rtn = lzma_index_decoder(stream, &this_index, UINT64_MAX);
        if (rtn != LZMA_OK) {
            break;
        }
        do {
            stream->avail_in = index_size > max_data_read ? max_data_read : index_size;
            data_read = fread(data, 1, stream->avail_in, my_fh);
            if (data_read < stream->avail_in) {
                break; /* need to exit entire loop - see below */
            }
            pos += stream->avail_in;
            index_size -= stream->avail_in;
            stream->next_in = (uint8_t *)data;
            rtn = lzma_code(stream, LZMA_RUN);
        } while (rtn == LZMA_OK);
        /* Exit entire loop if we didn't read a full block - see above */
        if (data_read < stream->avail_in) {
            break;
        }
        
        /* Check that we read as much as indicated */
        if (rtn == LZMA_STREAM_END) {
            if (index_size != 0 || stream->avail_in != 0) {
                break;
            }
        } else {
            break;
        }
        
        /* Decode the stream header and check that its stream flags match
         * the stream footer */
        pos -= (footer_flags.backward_size + LZMA_STREAM_HEADER_SIZE);
        if ((lzma_vli)(pos) < lzma_index_total_size(this_index)) {
            break;
        }
        pos -= lzma_index_total_size(this_index);
        fseek(my_fh, pos, SEEK_SET);
        data_read = fread(data, 1, LZMA_STREAM_HEADER_SIZE, my_fh);
        if (data_read < LZMA_STREAM_HEADER_SIZE) {
            break;
        }
        rtn = lzma_stream_header_decode(&header_flags, (uint8_t *)data);
        if (rtn != LZMA_OK) {
            break;
        }
        
        /* combine indexes if we have two */
        if (combined_index != NULL) {
			/* this, other, allocator, padding (%4==0) */
            rtn = lzma_index_cat(this_index, combined_index, NULL, 4);
            if (rtn != LZMA_OK) {
                break;
            }
        }
        
        combined_index = this_index;
        this_index = NULL;
    } while (pos > 0);
    
    fclose(my_fh);
    /* We should now have a combined index to get the size from. */
    size = lzma_index_uncompressed_size(combined_index);
    
    lzma_end(stream);
    lzma_index_end(combined_index, NULL);
    lzma_index_end(this_index, NULL);
    talloc_free(data);
    return size;
}

/*! \brief Returns true if we've reached the end of the file being read.
 *
 *  There are two ways of knowing whether we're at the end of the xz file:
 *  One is by checking the EOF state of the underlying file handle,
 *  the other is by finding out that the last buffer read got zero bytes.
 * \param fp The file handle to check.
 * \return True (1) if the file has reached EOF, False (0) if not.
 */

static bool xz_eof(cfile *fp) {
    cfile_xz *cfxp = (cfile_xz *)fp;
    /* we are done if the input file is empty and the buffer is 
     * exhausted too.  Asking feof on a writing file is nonsensical. */
    return feof(cfxp->xf) && buf_empty(cfxp->buffer);
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
 
static char *xz_gets(cfile *fp, char *str, size_t len) {
    cfile_xz *cfxp = (cfile_xz *)fp;
    return buf_fgets(cfxp->buffer, str, len, fp);
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

static int xz_vprintf(cfile *fp, const char *fmt, va_list ap)
  __attribute ((format (printf, 2, 0)));

static int xz_vprintf(cfile *fp, const char *fmt, va_list ap) {
    int rtn;
    char *buf = talloc_vasprintf(fp, fmt, ap);
    rtn = xz_write(fp, buf, sizeof(char), strlen(buf));
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
 
static ssize_t xz_read(cfile *fp, void *ptr, size_t size, size_t num) {
    cfile_xz *cfxp = (cfile_xz *)fp;
    ssize_t read_bytes = 0;
    ssize_t target_bytes = (size * num);
    lzma_ret rtn;

    cfxp->stream.next_in = ptr;
    cfxp->stream.avail_in = num;
    
    do {
        rtn = lzma_code(&cfxp->stream, LZMA_RUN);
        if (rtn != LZMA_OK) {
            /* Do something else? */
            return read_bytes;
        }
        if (cfxp->stream.avail_in == 0) {
            /* read more file into the buffer */
            read_bytes += xz_read_into_buffer(fp);
        }
    } while (read_bytes < target_bytes);
    return read_bytes;
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
 
static ssize_t xz_write(cfile *fp, const void *ptr, size_t size, size_t num) {
    cfile_xz *cfxp = (cfile_xz *)fp;
    lzma_ret rtn;

    cfxp->stream.next_in = ptr;
    cfxp->stream.avail_in = size * num;
    
    for (;;) {
        rtn = lzma_code(&cfxp->stream, LZMA_RUN);
        if (rtn != LZMA_OK) {
            /* do anything else? */
            return 0;
        }
        /* Leave early if there's still room for more compressed data */
        if (cfxp->stream.avail_out > 0) break;
        /* Write the entire buffer, reset pointer and available size */
        fwrite(cfxp->buffer->buffer, sizeof(uint8_t),
         cfxp->buffer->bufsize, cfxp->xf);
        cfxp->stream.next_out = (uint8_t *)cfxp->buffer->buffer;
        cfxp->stream.avail_out = cfxp->buffer->bufsize;
    }
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
 
static int xz_flush(cfile *fp) {
    cfile_xz *cfxp = (cfile_xz *)fp;
    lzma_ret rtn;
    size_t written;

    for (;;) {
        rtn = lzma_code(&cfxp->stream, LZMA_FULL_FLUSH);
        if (rtn == LZMA_STREAM_END) {
            break;
        }
        if (rtn != LZMA_OK) {
            errno = EINVAL;
            return EOF;
        }
    }
    written = fwrite(cfxp->buffer->buffer, sizeof(char *), cfxp->buffer->bufsize - cfxp->stream.avail_out, cfxp->xf);
    return written > 0;
}

/*! \brief Close the given file handle.
 *
 *  This function frees the memory allocated for the file handle and
 *  closes the associated file.
 * \param fp The file handle to close.
 * \return the success of the file close operation.
 */
 
static int xz_close(cfile *fp) {
    cfile_xz *cfxp = (cfile_xz *)fp;
    lzma_ret rtn;
    
    if (cfxp->writing) {
        for (;;) {
            rtn = lzma_code(&cfxp->stream, LZMA_FINISH);
            if (cfxp->stream.avail_out == 0) {
                /* Write the buffer to it */
                fwrite(cfxp->buffer->buffer, sizeof(uint8_t),
                 cfxp->buffer->bufsize, cfxp->xf);
                cfxp->stream.next_out = (uint8_t *)cfxp->buffer->buffer;
                cfxp->stream.avail_out = cfxp->buffer->bufsize;
            }
            
            if (rtn == LZMA_STREAM_END) {
                fwrite(cfxp->buffer->buffer, sizeof(uint8_t),
                 cfxp->buffer->bufsize - cfxp->stream.avail_out, cfxp->xf);
                break;
            }
            
            if (rtn > 0) {
                break;
            }
        }
    }
    lzma_end(&cfxp->stream);
    return fclose(cfxp->xf);
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


