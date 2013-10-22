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
#include <bzlib.h>
#include <talloc.h>
/* For getting the file date for the extended attribute check */
#include <sys/stat.h>
/* For getting the current time for the time stamp on the attribute */
#include <time.h>
/* For saving the size of bzip2 files once calculated: */
#include <attr/xattr.h>

#include "cfile_private.h"
#include "cfile_bzip2.h"

/* Predeclare function calls */
off_t bzip2_size(cfile *fp);
bool bzip2_eof(cfile *fp);
char *bzip2_gets(cfile *fp, char *str, size_t len);
ssize_t bzip2_read(cfile *fp, void *ptr, size_t size, size_t num);
ssize_t bzip2_write(cfile *fp, const void *ptr, size_t size, size_t num);
int bzip2_flush(cfile *fp);
int bzip2_close(cfile *fp);

/*! \brief The bzip2 file structure
 *
 * We only need to store the actual (zlib) file pointer.
 */
typedef struct cfile_bzip2 {
    cfile inherited; /*< our inherited function table */
    BZFILE *bp;      /*< the actual bzlib file pointer */
    char *buffer;    /*< a read buffer for doing gets */
    int buflen;      /*< the length of the buffer we've read */
    int bufpos;      /*< our position in the buffer */
} cfile_bzip2;

static const cfile_vtable bzip2_cfile_table;

/*! The size of the character buffer for reading lines from bzip2 files.
 *
 *  This isn't really a file cache, just a way of saving us single-byte
 *  calls to bzread.
 */
#define BZIP2_BUFFER_SIZE 1024

/* Predeclared prototypes */
static void bzip_attempt_store(cfile *fp, off_t size);

/*! \brief Calculate the size of a bzip2 file by running it through bzcat.
 *
 *  The only way to get the uncompressed size of a bzip2 file, if there's
 *  no other information about it, is to count every character.  Here we
 *  run it through bzcat and wc -c, which some might argue was horribly
 *  inefficient - but these tools are designed for the job, whereas we'd
 *  have to run it through a buffer here anyway.
 *
 *  If we have extended attributes, we can try to cache this value in
 *  them (see below).
 * \param fp The file whose size needs calculating.
 * \return The size of the uncompressed file in bytes.
 * \see cfsize
 */

static off_t bzip_calculate_size(cfile *fp) {
    char *input;
    FILE *fpipe;
    long fsize;

    cfile_bzip2 *cfbp = (cfile_bzip2 *)fp;
    const int max_input_size = 20;
    char *cmd = talloc_asprintf(fp, "bzcat '%s' | wc -c", cfbp->inherited.filename);
    if (! cmd) {
        return 0;
    }
    input = talloc_array(fp, char, max_input_size);
    if (! input) {
        talloc_free(cmd);
        return 0;
    }
    fpipe = popen(cmd, "r");
    if (fpipe) {
        input = fgets(input, max_input_size, fpipe);
    } else {
        talloc_free(cmd);
        talloc_free(input);
        return 0;
    }
    pclose(fpipe);
    talloc_free(cmd);
    fsize = atol(input);
    talloc_free(input);
    return fsize;
}

/*! \struct size_xattr_struct
 *  \brief The structure used in the extended attributes to store uncompressed
 *   file sizes and the associated time stamp.
 *
 *  In order to store the uncompressed file size of a bzip2 file for later
 *  easy retrieval, this structure stores all the necessary information to
 *  both store the size and validate its correctness against the compressed
 *  file.
 *
 *  Note that we don't attempt any check of endianness or internal
 *  validation on this structure.  You're assumed to be reading the file
 *  system with the same operating system that the extended attribute was
 *  written with.
 * \see cfsize
 * \see bzip_attribute_size
 * \see bzip_attempt_store
 */

struct size_xattr_struct {
    off_t file_size;
    time_t time_stamp;
};

/* #define DEBUG_XATTR 1 */

/*! \brief Give the uncompressed file size, or 0 if errors.
 *
 *  This function checks whether we:
 *   - Have extended attributes.
 *   - Can read them.
 *   - The file has the extended attributes for uncompressed file size.
 *   - The attribute is valid (i.e. it's the same size as the structure
 *     it's supposed to be stored in).
 *   - They're not out of date WRT the compressed file.
 *  The function returns the file size if all these are true; otherwise,
 *  0 is returned.
 * \param fp The file handle to check.
 * \return The uncompressed file size attribute if it is valid, false otherwise.
 * \see cfsize
 */

static int bzip_attribute_size(cfile *fp) {
    struct stat sp;

    cfile_bzip2 *cfbp = (cfile_bzip2 *)fp;
    struct size_xattr_struct xattr;
    ssize_t check = getxattr(
        cfbp->inherited.filename,
        "user.cfile_uncompressed_size",
        &xattr,
        0); /* 0 means 'tell us how many bytes are in the value' */
#ifdef DEBUG_XATTR
    fprintf(stderr, "Attribute check on %s gave %d\n"
        , cfbp->inherited.filename, (int)check
    );
#endif
    /* Would the attribute be retrieved? */
    if (check <= 0) {
        return 0;
    }
    /* Does the structure size check out? */
#ifdef DEBUG_XATTR
    fprintf(stderr, "Attribute size = %d, structure size = %lu\n"
        , (int)check, sizeof xattr
    );
#endif
    if (check != sizeof xattr) {
        return 0;
    }
    /* Fetch the attribute */
    check = getxattr(
        cfbp->inherited.filename,
        "user.cfile_uncompressed_size",
        &xattr,
        sizeof xattr);
#ifdef DEBUG_XATTR
    fprintf(stderr, "The result of the actual attribute fetch was %d\n"
        , (int)check
    );
#endif
    if (check <= 0) {
        return 0;
    }
#ifdef DEBUG_XATTR
    fprintf(stderr, "file size = %ld, time stamp = %ld\n"
        , (long)xattr.file_size, (long)xattr.time_stamp
    );
#endif
    /* Now check it against the file's modification time */
    if (stat(cfbp->inherited.filename, &sp) == 0) {
#ifdef DEBUG_XATTR
        fprintf(stderr, "stat on file good, mtime = %ld\n"
            ,(long)sp.st_mtime
        );
#endif
        return sp.st_mtime <= xattr.time_stamp ? xattr.file_size : 0;
    } else {
#ifdef DEBUG_XATTR
    fprintf(stderr, "stat on file bad.\n"
    );
#endif
        return 0;
    }
}

/*! \brief Attempt to store the file size in the extended user attributes.
 *
 *  If we've had to calculate the uncompressed file size the hard way,
 *  then it's worth saving this.  This routine attempts to do so.
 *  If we can't the value is discarded and the user will have to wait for
 *  the file size to be calculated afresh each time.
 * \param fp The file handle to check.
 * \param size The uncompressed size of the file in bytes.
 * \see cfsize
 */

static void bzip_attempt_store(cfile *fp, off_t size) {
    cfile_bzip2 *cfbp = (cfile_bzip2 *)fp;
    struct size_xattr_struct xattr;
    /* Set up the structure */
    xattr.file_size = size;
    xattr.time_stamp = time(NULL);
#ifdef DEBUG_XATTR
    fprintf(stderr, "Attempting to store file size = %ld, time stamp = %ld\n"
        , (long)xattr.file_size, (long)xattr.time_stamp
    );
#endif
    /* Store it in the extended attributes if possible. */
#ifdef DEBUG_XATTR
    int rtn =
#endif
    setxattr(
        cfbp->inherited.filename,
        "user.cfile_uncompressed_size",
        &xattr,
        sizeof xattr,
        0); /* flags = default: create or replace as necessary */
    /* Explicitly ignoring the return value... */
#ifdef DEBUG_XATTR
    fprintf(stderr, "setxattr returned %d\n", rtn);
    if (rtn == -1) {
        fprintf(stderr, "error state is %d: %s\n",  errno, strerror(errno));
    }
#endif
}

/*! \brief Open a file for reading or writing
 *
 *  Open the given file using the given mode.  Opens the file and
 *  returns a cfile handle to it.  Mode must start with 'r' or 'w'
 *  to read or write (respectively) - other modes are not expected
 *  to work.
 *
 * \return A successfully created file handle, or NULL on failure.
 */
cfile *bzip2_open(const char *name, /*!< The name of the file to open. */
                  const char *mode) /*!< "r" to specify reading, "w" for writing. */
{
	cfile_bzip2 *cfbp;
	
    BZFILE *own_file = BZ2_bzopen(name, mode);
    if (!own_file) {
        /* Keep any errno set by bzopen - let it handle any invalid modes,
           etc. */
        return NULL;
    }
    cfbp = (cfile_bzip2 *)cfile_alloc(&bzip2_cfile_table, name, mode);
    if (!cfbp) {
        errno = EINVAL;
        BZ2_bzclose(own_file);
        return NULL;
    }
    cfbp->bp = own_file;
    cfbp->buffer = NULL;
    cfbp->buflen = 0;
    cfbp->bufpos = 0;
    return (cfile *)cfbp;
}

/*! \brief Returns the _uncompressed_ file size
 *
 *  Unfortunately, bzip2 files do not store the size of the uncompressed
 *  content, so we have to read the entire file
 *  through bzcat and wc -c.  This is easier than reading it directly,
 *  although it then relies on the availability of those two binaries,
 *  and may therefore make this routine not portable.  I'm not sure if
 *  this introduces any security holes in this library.  Unfortunately,
 *  correspondence with Julian Seward has confirmed that there's no
 *  other way of determining the exact uncompressed file size, as it's
 *  not stored in the bzip2 file itself.
 *
 *  HOWEVER: we can save the next call to cfsize on this file a
 *  considerable amount of work if we save the size in a filesystem
 *  extended attribute.  Because rewriting an existing file does a
 *  truncate rather than delete the inode, the attribute may get out of
 *  sync with the actual file.  So we also write the current time as a
 *  timestamp on that data.  If the file's mtime is greater than that
 *  timestamp, then the data is out of date and must be recalculated.
 *  Make sure your file system has the \c user_xattr option set if you
 *  want to use this feature!
 * \param fp The file handle to check
 * \return The number of bytes in the uncompressed file.
 */

off_t bzip2_size(cfile *fp) {
    /* There's no file size information in the file.  So we have
     * to feed the entire file through bzcat and count its characters.
     * Tedious, but then hopefully you only have to do this once; and
     * at least it may cache the file for further reading.  In other
     * words, getting the size of a bzipped file takes a number of
     * seconds - caveat caller... */
    off_t size;
    if ((size = bzip_attribute_size(fp)) == 0) {
        size = bzip_calculate_size(fp);
        bzip_attempt_store(fp, size);
    }
    return size;
}

/*! \brief Returns true if we've reached the end of the file being read.
 *
 *  bzlib doesn't seem to correctly return BZ_STREAM_END
 *  when the stream has actually reached its end, so we have to check
 *  another way - whether the last buffer read was zero bytes long.
 * \param fp The file handle to check.
 * \return True (1) if the file has reached EOF, False (0) if not.
 */

bool bzip2_eof(cfile *fp) {
    cfile_bzip2 *cfbp = (cfile_bzip2 *)fp;
    int errnum;
    BZ2_bzerror(cfbp->bp, &errnum);
    /* this actually returns a pointer to the error message,
     * but we're not using it in this context... */
    if (errnum == 0) {
        /* bzerror doesn't appear to be reporting BZ_STREAM_END
         * when it's run out of characters */
        /* But if we've allocated a buffer, and its length and
         * position are now zero, then we're at the end of it AFAICS */
        if (cfbp->buffer != NULL && cfbp->buflen == 0 && cfbp->bufpos == 0) {
            return 1;
        }
    }
    return (errnum == BZ_OK
         || errnum == BZ_RUN_OK
         || errnum == BZ_FLUSH_OK
         || errnum == BZ_FINISH_OK) ? 0 : 1;
    /* From my reading of the bzip2 documentation, all error
     * conditions and the 'OK' condition of BZ_STREAM_END
     * indicate that you can't read from the file any more, which
     * is a logical EOF in my book. */
}

/*! \brief An implementation of fgetc for bzip2 files.
 *
 *  bzlib does not implement any of the 'low level' string functions.
 *  In order to support treating a bzip2 file as a 'real' file, we
 *  we need to provide fgets (for the cfgetline function, if nothing else).
 *  The stdio.c implementation relies on fgetc to get one character at a
 *  time, but this would be inefficient if done as continued one-byte
 *  reads from bzlib.  So we use the buffer pointer to store chunks of
 *  the file to read from.
 * \param fp The file to read from.
 * \return the character read, or EOF (-1).
 */
static int bz_fgetc(cfile *fp) {
    cfile_bzip2 *cfbp = (cfile_bzip2 *)fp;
    /* Should we move this check and creation to the initialisation,
     * so it doesn't slow down the performance of fgetc? */
    if (! cfbp->buffer) {
        cfbp->buffer = talloc_array(cfbp, char, BZIP2_BUFFER_SIZE);
        if (! cfbp->buffer) {
            errno = EINVAL;
            return EOF;
        }
    }
    if (cfbp->buflen == cfbp->bufpos) {
        cfbp->bufpos = 0;
        cfbp->buflen = BZ2_bzread(cfbp->bp, cfbp->buffer, BZIP2_BUFFER_SIZE);
        if (cfbp->buflen <= 0) return EOF;
    }
    return cfbp->buffer[cfbp->bufpos++]; /* Ah, the cleverness of postincrement */
}

/*! \brief Get a string from the file, up to a maximum length or newline.
 *
 *  bzlib doesn't provide an equivalent to gets, so we have to copy the
 *  implementation from stdio.c and use it here, referring to our own
 *  bz_fgetc function.
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
 
char *bzip2_gets(cfile *fp, char *str, size_t len) {
    /*cfile_bzip2 *cfbp = (cfile_bzip2 *)fp;*/
    /* Implementation pulled from glibc's stdio.c */
    char *ptr = str;
    int ch;

    if (len <= 0) return NULL;

    while (--len) {
        if ((ch = bz_fgetc(fp)) == EOF) {
            if (ptr == str) return NULL;
            break;
        }

        if ((*ptr++ = ch) == '\n') break;
    }

    *ptr = '\0';
    return str;
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

int bzip2_vprintf(cfile *fp, const char *fmt, va_list ap)
  __attribute ((format (printf, 2, 0)));

int bzip2_vprintf(cfile *fp, const char *fmt, va_list ap) {
    cfile_bzip2 *cfbp = (cfile_bzip2 *)fp;
    int rtn;
    char *buf = talloc_vasprintf(fp, fmt, ap);
    rtn = BZ2_bzwrite(cfbp->bp, buf, strlen(buf));
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
 
ssize_t bzip2_read(cfile *fp, void *ptr, size_t size, size_t num) {
    cfile_bzip2 *cfbp = (cfile_bzip2 *)fp;
    return BZ2_bzread(cfbp->bp, ptr, size * num);
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
 
ssize_t bzip2_write(cfile *fp, const void *ptr, size_t size, size_t num) {
    cfile_bzip2 *cfbp = (cfile_bzip2 *)fp;
    ssize_t rtn = BZ2_bzwrite(cfbp->bp, (void *)ptr, size * num);
    /* talloc_free(my_ptr); */
    return rtn;
}

/*! \brief Flush the file's output buffer.
 *
 *  This function flushes any data passed to write or printf but not
 *  yet written to disk.  If the file is being read, it has no effect.
 * \param fp The file handle to flush.
 * \return the success of the file flush operation.
 * \note for gzip files, under certain compression methods, flushing
 *  may result in lower compression performance.  We use Z_SYNC_FLUSH
 *  to write to the nearest byte boundary without unduly impacting
 *  compression.
 */
 
int bzip2_flush(cfile *fp) {
    cfile_bzip2 *cfbp = (cfile_bzip2 *)fp;
    return BZ2_bzflush(cfbp->bp);
}

/*! \brief Close the given file handle.
 *
 *  This function frees the memory allocated for the file handle and
 *  closes the associated file.
 * \param fp The file handle to close.
 * \return the success of the file close operation.
 */
 
int bzip2_close(cfile *fp) {
    cfile_bzip2 *cfbp = (cfile_bzip2 *)fp;
    /* Use the ReadClose or WriteClose routines to get the error
     * status.  If we were writing, this gives us the uncompressed
     * file size, which can be stored in the extended attribute. */
    /* How do we know whether we were reading or writing?  If the
     * buffer has been allocated, we've been read from (in theory).
     */
    int bzerror;
    if (cfbp->buffer) {
        BZ2_bzReadClose(&bzerror, cfbp->bp);
    } else {
        /* Writing: get the uncompressed byte count and store it. */
        unsigned uncompressed_size;
        /* 0 = don't bother to complete the file if there was an error */
        BZ2_bzWriteClose(&bzerror, cfbp->bp, 0, &uncompressed_size, NULL);
        bzip_attempt_store(fp, uncompressed_size);
    }
    return bzerror;
}

/*! \brief The function dispatch table for bzip2 files */

static const cfile_vtable bzip2_cfile_table = {
    sizeof(cfile_bzip2),
    bzip2_size,
    bzip2_eof,
    bzip2_gets,
    bzip2_vprintf,
    bzip2_read,
    bzip2_write,
    bzip2_flush,
    bzip2_close,
    "BZip2 file"
};


