/*
 * cfile - compressed file read/write library
 * Copyright (C) 2006 Paul Wayper
 * Copyright (C) 2012 Peter Miller
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
 *
 *
 * \brief Implementation for a normal uncompressed file.
 */

#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <talloc.h>

#include "cfile_private.h"
#include "cfile_normal.h"

/*! \brief The normal, uncompressed file structure
 *
 * We only need to store the actual file pointer.
 */
typedef struct cfile_normal {
    cfile *vptr;     /*< something or other */
    cfile inherited; /*< our inherited cfile structure */
    FILE *fp;        /*< the actual uncompressed file pointer */
} cfile_normal;

static const cfile_vtable normal_cfile_table;

/*! \brief Returns the _uncompressed_ file size
 *
 *  The common way of reporting your progress through reading a file is
 *  as a proportion of the uncompressed size.  But a simple stat of the
 *  compressed file will give you a much lower figure.  So here we
 *  extract the size of the uncompressed content of the file.  Naturally
 *  this process is easy with uncompressed files.  It's also fairly
 *  easy with gzip files - the size is a 32-bit little-endian signed
 *  int (I think) at the end of the file.  Unfortunately, bzip2 files
 *  do not carry this information, so we have to read the entire file
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

static off_t normal_size(cfile *fp) {
    cfile_normal *cfnp = (cfile_normal *)fp;
    struct stat sp;
    if (stat(cfnp->inherited.filename, &sp) == 0) {
        return sp.st_size;
    } else {
        return 0;
    }
}

/*! \brief Returns true if we've reached the end of the file being read.
 *
 *  This mostly passes through the state of the lower-level's EOF
 *  checking.  But bzlib doesn't seem to correctly return BZ_STREAM_END
 *  when the stream has actually reached its end, so we have to check
 *  another way - whether the last buffer read was zero bytes long.
 * \param fp The file handle to check.
 * \return True (1) if the file has reached EOF, False (0) if not.
 */

static bool normal_eof(cfile *fp) {
    cfile_normal *cfnp = (cfile_normal *)fp;
    return feof(cfnp->fp);
}

/*! \brief Get a string from the file, up to a maximum length or newline.
 *
 *  For uncompressed files we simply use stdio's fgets implementation.
 *
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

static char *normal_gets(cfile *fp, char *str, size_t len) {
    cfile_normal *cfnp = (cfile_normal *)fp;
    return fgets(str, len, cfnp->fp);
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

static int normal_vprintf(cfile *fp, const char *fmt, va_list ap)
  __attribute ((format (printf, 2, 0)));

static int normal_vprintf(cfile *fp, const char *fmt, va_list ap) {
    cfile_normal *cfnp = (cfile_normal *)fp;
    return vfprintf(cfnp->fp, fmt, ap);
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

static ssize_t normal_read(cfile *fp, void *ptr, size_t size, size_t num) {
    cfile_normal *cfnp = (cfile_normal *)fp;
    return fread(ptr, size, num, cfnp->fp);
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

static ssize_t normal_write(cfile *fp, const void *ptr, size_t size,
    size_t num)
{
    cfile_normal *cfnp = (cfile_normal *)fp;
    return fwrite(ptr, size, num, cfnp->fp);
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

static int normal_flush(cfile *fp) {
    cfile_normal *cfnp = (cfile_normal *)fp;
    return fflush(cfnp->fp);
}

/*! \brief Close the given file handle.
 *
 *  This function frees the memory allocated for the file handle and
 *  closes the associated file.
 * \param fp The file handle to close.
 * \return the success of the file close operation.
 */

static int normal_close(cfile *fp) {
    cfile_normal *cfnp = (cfile_normal *)fp;
    if (cfnp->fp == stdin || cfnp->fp == stdout || cfnp-> fp == stderr)
        return 0;
    return fclose(cfnp->fp);
}


/*! \brief Open a file for reading or writing
 *
 *  Open the given file using the given mode.  Opens the file and
 *  returns a cfile handle to it.
 *
 * @param name
 *     The name of the file to open.
 *     If this is "-", then stdin is read from or stdout is
 *     written to, as appropriate (both being used uncompressed).
 * @param mode
 *     can be any mode that fopen allows
 * @return
 *     A successfully created file handle, or NULL on failure.
 */
cfile *normal_open(const char *name, const char *mode)
{
    /* If we have a '-' as a file name, dup stdin or stdout */
    FILE *own_file;
    cfile_normal *cfnp;
    if (strcmp(name, "-") == 0) {
        if (strstr(mode, "r") != 0) {
            own_file = stdin;
        } else if ((strstr(mode, "w") != 0)
               ||  (strstr(mode, "a") != 0)) {
            own_file = stdout;
        } else {
            errno = EINVAL;
            return NULL;
        }
        if (! own_file) {
            errno = EINVAL;
            return NULL;
        }
    } else {
        own_file = fopen(name, mode);
        if (! own_file) {
            /* leave operating system error unmolested */
            return NULL;
        }
    }
    cfnp = (cfile_normal *)cfile_alloc(&normal_cfile_table,
        name, mode);
    if (!cfnp) {
        errno = EINVAL;
        return NULL;
    }
    cfnp->fp = own_file;
    return (cfile *)cfnp;
}

/*! \brief Open a file from a file descriptor
 *
 *  Allows you to open the file specified by the given file descriptor,
 *  with the same mode options as a regular file.  Originally necessary
 *  to allow access to stdin and stdout, but with the current handling
 *  of "-" by cfopen this should be mostly unnecessary.
 *
 * \param filedesc
 *     An integer file descriptor number.
 * \param mode
 *     The mode to open the file in ("r" for read, "w" for write).
 * \return
 *     A successfully created file handle, or NULL on failure.
 */

cfile *normal_dopen(const int filedesc, const char *mode) {
    FILE *own_file = fdopen(filedesc, mode);
    char *name = talloc_asprintf(pwlib_context,
     "file descriptor %d (mode %s)", filedesc, mode);
    cfile_normal *cfnp = (cfile_normal *)cfile_alloc(&normal_cfile_table,
     name, mode);
    if (!cfnp) {
        errno = EINVAL;
        return NULL;
    }
    cfnp->fp = own_file;
    return (cfile *)cfnp;
}

/*! \brief The function dispatch table for normal files
 */
static const cfile_vtable normal_cfile_table = {
    sizeof(cfile_normal),
    normal_size,
    normal_eof,
    normal_gets,
    normal_vprintf,
    normal_read,
    normal_write,
    normal_flush,
    normal_close,
    "Normal file"
};


/* vim: set ts=8 sw=4 et : */
