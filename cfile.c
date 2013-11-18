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

/*! \file cfile.c
 *  \brief The main cfile library code.
 */

/*! \mainpage The cfile Library
 *
 * \section introduction Introduction
 *
 *  Put simply, this library is designed to allow your code to read
 *  or write a file regardless of whether it is uncompressed, or
 *  compressed with either bzip2 or gzip.  It automatically detects
 *  the compression type from the file's extension and encapsulates
 *  the appropriate library routines in a common interface.
 *  If the file name is "-", then stdin or stdout is opened as
 *  appropriate.  As a further service, the cfgetline() routine
 *  allows you to read lines of any size from your input file,
 *  automatically resizing the buffer to suit.  Other convenience
 *  routines, such as cfsize(), are provided.
 *
 * \section requirements Requirements
 *
 *  The following libraries are required for cfile:
 *   - The talloc library from http://talloc.samba.org must be
 *     installed.
 *   - zlib and bzlib must be installed.
 *   - In order to determine the uncompressed file size of bzip2
 *     file, the bzcat and wc binaries must be available to the
 *     calling program.
 *   - In order to save the uncompressed file size of bzip2 files
 *     once calculated, the attr/xattr.h library is required.  If
 *     the filesystem you are using does not support extended
 *     user attributes, then nothing will happen.
 *
 * \section optional Optional extras
 *
 *  If the libmagic library is defined at the time of compiling
 *  the cfile library, then libmagic will be used to determine the
 *  type of files being read.  Files being written will still have
 *  their type determined by their file extension.
 *
 *  In order to actually save the uncompressed file size of bzip2 files
 *  once calculated, your file system should have extended user
 *  attributes enabled.  This can be set by having the \c user_xattr
 *  option set in the mount table.  You may need to remount your file
 *  system with <tt>mount -o remount /mountpoint</tt> in order to
 *  enable this functionality.  If this is not set, or other factors
 *  don't allow the extended user attribute to be written, then no bad
 *  will occur - it'll just mean that the size will be calculated from
 *  scratch each time...
 *
 * \section aims Aims
 *
 *  To allow you to read or write files whether it is compressed or
 *  not.
 *
 *  To provide extra, useful functions like cfgetline().
 *
 *  To provide a consistent parameter passing interface rather than
 *  having to know exactly what is passed where and in what form.
 *
 * \section notes Notes
 *
 *  The file extension for gzip files is \c '.gz'.
 *
 *  The file extension for bzip2 files is \c '.bz2'.
 *
 *  If an uncompressed file is being read, the stdio routines will
 *  always be used, despite zlib supporting opening and reading both
 *  gzip-compressed files and uncompressed files.
 *
 *  cfile files do not support random access, simultaneous read and
 *  write access, or appending.
 *
 * \todo Add better error and EOF checking, particularly for bzip.
 *
 * \todo Allow only read or write modes, with no appending.
 *
 * \todo Allow extra parameters in the mode string to specify
 *  compression options.
 *
 * \todo Use the buffer to write to: avoids allocating a new temporary
 *  buffer upon each cfprintf() and cvfprintf().
 *
 * \todo Tridge noted that the standard implementation of stdio has
 *  pointers in the file handle that refer to the functions that are
 *  called when performing operations on that file handle.  It may
 *  therefore be able to provide a wrapper that allows callers to
 *  simply replace a #include <stdio.h> with #include <cfile.h> and
 *  all file operations would then happen transparently.  The modified
 *  fopen would determine the file type and update the jump block
 *  with the relevant functions (either direct calls to the functions
 *  in e.g. zlib, or wrappers that implement the correct semantics.
 *  So the whole thing would be a 'drop in' replacement for stdio,
 *  rather than requiring modification of existing code.
 */

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <talloc.h>
#include <errno.h>

#include "cfile.h"
#include "cfile_private.h"
#include "cfile_normal.h"
#include "cfile_gzip.h"
#include "cfile_bzip2.h"
#include "cfile_xz.h"
#include "cfile_null.h"



/*! \brief The library's Talloc context
 */

void *pwlib_context = NULL;

/*! \brief Allocate a new table with the implementation's functions
 *
 *  Allocate an implementation-specific amount of memory to this pointer,
 *  and fill the generic table structure with the given implementation's
 *  function table.
 *
 * @param vptr
 *     the implementation-specific pointer table
 * @param name
 *     the name of the file being opened
 * @param mode
 *     the mode being used to open the file
 * @return
 *     The allocated memory, or NULL if we couldn't allocate.
 */
cfile *cfile_alloc(const cfile_vtable *vptr, const char *name,
    const char *mode)
{
    cfile *fp;
    if (!pwlib_context) {
        pwlib_context = talloc_init("CFile Talloc context");
    }
    fp = talloc_size(pwlib_context, vptr->struct_size);
    if (fp) {
        fp->vptr = vptr;
        talloc_set_name(fp, "cfile '%s' (mode '%s')", name, mode);
        fp->filename = talloc_strdup(fp, name);
    	talloc_set_destructor(fp, vptr->close);
    }
    return fp;
}

/* Stolen from ccan/str/str.h */
static inline bool strends(const char *str, const char *postfix)
{
       if (strlen(str) < strlen(postfix))
               return false;

       return !strcmp(str + strlen(str) - strlen(postfix), postfix);
}

/*! \brief Set cfile's parent context
 *
 *  Set cfile's parent context.  This allows a caller using talloc to 'own'
 *  all the memory created by cfile.  Because we use destructor functions,
 *  this in turn means that when the caller frees our memory all cfiles will
 *  be automatically closed
 *
 * \param parent_context
 *     the talloc context of the caller.
 * \return
 *     nothing
 */
void cfile_set_context(void *parent_context) {
    if (pwlib_context) {
        /* we've already set up a context - we need the caller to take
         * ownership of our current memory. */
        talloc_steal(parent_context, pwlib_context);
    } else {
        /* set our context to be their context */
        pwlib_context = parent_context;
    }
}

/*! \brief Open a file for reading or writing
 *
 *  Open the given file using the given mode.  Opens the file and
 *  returns a cfile handle to it.  Mode can be any type that the actual file
 *  supports.
 *
 * @param name
 *     The name of the file to open.  If this is "-", then stdin is read
 *     from or stdout is written to, as appropriate (both being used
 *     uncompressed).
 * @param mode
 *     "r" to specify reading, "w" for writing.
 * @return
 *     A successfully created file handle, or NULL on failure.
 */
cfile *cfile_open(const char *name, const char *mode) {
    /* If we have a '-' as a file name, treat it as uncompressed (for now) */
    if (strcmp(name, "-") == 0) {
        return normal_open(name, mode);
    }

    if (cfile_null_candidate(name)) {
        return cfile_null_open(name, mode);
	}
	
#ifdef MAGIC_NONE
    /* We can only determine the file type if it exists - i.e. is being
     * read.  Otherwise, fall through to file extension checking. */
    if (strstr(mode, "r") != NULL) {
        magic_t checker = magic_open(MAGIC_NONE);
        if (checker) {
            char *type = magic_file(checker, name);
            cfile *rtn;
            if (strstr(type, "gzip compressed data") != NULL) {
                rtn = gzip_open(name, mode);
            } else if (strstr(type, "bzip2 compressed data") != NULL) {
                rtn = bzip2_open(name, mode);
            } else if (strstr(type, "XZ compressed data") != NULL) {
				rtn = xz_open(name, mode);
            } else {
                rtn = normal_open(name, mode);
            }
            magic_close(checker);
            return rtn;
        }
    }
#endif
    /* Even though zlib allows reading of uncompressed files, let's
     * not complicate things too much at this stage :-) */
    if (strends(name, ".gz")) {
        return gzip_open(name, mode);
    } else if (strends(name, ".bz2")) {
        return bzip2_open(name, mode);
    } else if (strends(name, ".xz")) {
		return xz_open(name, mode);
    } else {
        return normal_open(name, mode);
    }
}

/*! \brief Open a file from a file descriptor
 *
 *  Allows you to open the file specified by the given file descriptor,
 *  with the same mode options as a regular file.  Originally necessary
 *  to allow access to stdin and stdout, but with the current handling
 *  of "-" by cfile_open this should be mostly unnecessary.
 *
 * \param filedesc
 *     An integer file descriptor number.
 * \param mode
 *     The mode to open the file in ("r" for read, "w" for write).
 * \return
 *     A successfully created file handle, or NULL on failure.
 * \todo
 *     Make this detect a compressed input stream, and allow setting of
 *     the compression type via the mode parameter for an output stream.
 */

cfile *cfile_dopen(int filedesc, const char *mode) {
    /* We don't support trying to determine the nature of a file that's
       already open */
    return normal_dopen(filedesc, mode);
}

/*! \brief Returns the _uncompressed_ file size
 *
 *  The common way of reporting your progress through reading a file is
 *  as a proportion of the uncompressed size.  But a simple stat of the
 *  compressed file will give you a much lower figure.  So here we
 *  extract the size of the uncompressed content of the file.
 *
 * \param fp
 *     The file handle to check
 * \return
 *     The number of bytes in the uncompressed file.
 */

off_t cfsize(cfile *fp) {
    if (!fp || !fp->vptr) {
        errno = EINVAL;
        return (off_t)-1;
    }
    return fp->vptr->size(fp);
}

/*! \brief Returns true if we've reached the end of the file being read.
 *
 * \param fp
 *     The file handle to check.
 * \return
 *     True (1) if the file has reached EOF, False (0) if not.
 */

bool cfeof(cfile *fp) {
    if (!fp || !fp->vptr) {
        errno = EINVAL;
        return true;
    }
    return fp->vptr->eof(fp);
}

/*! \brief Get a string from the file, up to a maximum length or newline.
 *
 * @param fp
 *     The file handle to read from.
 * @param str
 *     A character array to read the line into.
 * @param len
 *     The maximum size of the array in bytes.
 * @return
 *     A pointer to the string thus read.
 */

char *cfgets(cfile *fp, char *str, size_t len) {
    if (!fp || !fp->vptr) {
        errno = EINVAL;
        return NULL;
    }
    return fp->vptr->gets(fp, str, len);
}

/*! Macro to check whether the line is terminated by a newline or equivalent */
#define isafullline(line, len) \
    ((line)[(len - 1)] == '\n' || (line)[(len - 1)] == '\r')

/*! \brief Read a full line from the file, regardless of length
 *
 *  Of course, with fgets you can't always guarantee you've read an entire
 *  line.  You have to know the length of the longest line, in advance, in
 *  order to read each line from the file in one call.  cfgetline solves
 *  this problem by progressively extending the string you pass until the
 *  entire line has been read.  To do this it uses talloc_realloc, and it
 *  determines the array's original size using talloc_get_size.  If you
 *  haven't initialised the line beforehand, cfgetline will do so
 *  (allocating it against the file pointer's context).  If you have, then
 *  the magic of talloc_realloc allocates the new space against the
 *  context that you originally allocated your buffer against.  So to
 *  speak.
 *
 *  In normal usage, this 'buffer' will expand but never contract.  If
 *  you need to, you can shrink the buffer yourself once you get it.
 *
 * @param fp
 *     The file handle to read from.
 * @param line
 *     A pointer to a character array to read the line into, and 
 *     extend if required.
 * @return
 *     If a line was read, return true.  If the file ended, return false.
 */

#define CFGETLINE_DEBUG 0

bool cfgetline(cfile *fp, char **line) {
    /* Get a line from the file into the buffer.
     * If you pass a NULL pointer, it will allocate memory for you initially
     * (although against the file's context rather than your own).
     */
    size_t off = 0, len;
#if CFGETLINE_DEBUG
    printf("cfgetline(fp=%p, line=%p)\n", fp, line);
#endif

    for (;;) {
        /* This returns 0 if *line is NULL */
        len = talloc_get_size(*line);
#if CFGETLINE_DEBUG
        printf("   len=%zu, off=%zu\n", len, off);
#endif

        /* Do we need more buffer? */
        if (off + 1 >= len) {
            /* if we receive exactly len-1 characters, there isn't space to
             * store the newline or the null terminator.  So expand if we
             * hit that point, rather than the exact offset. */
            if (len == 0) {
                len = 80;
            } else {
                len *= 2;
            }
#if CFGETLINE_DEBUG
            printf("   realloc line to len = %zu\n", len);
#endif
            *line = talloc_realloc(fp, *line, char, len);
        }

        /* Get more line */
        if (! cfgets(fp, *line + off, len - off)) {
            /* No more line - return a partial like fgets. */
#if CFGETLINE_DEBUG
            printf("   cfgets(fp, line + off=%zu, len=%zu - off=%zu) returned false\n",
             off, len, off);
#endif
            break;
        }
#if CFGETLINE_DEBUG
        printf("   cfgets(fp, line + off=%zu, len=%zu - off=%zu) returned true\n",
         off, len, off);
#endif
        /* And set our line length */
        off += strlen(*line + off);
#if CFGETLINE_DEBUG
        printf("   *line='%s', offset now %zu\n", *line, off);
#endif
        if (isafullline(*line, off)) {
#if CFGETLINE_DEBUG
            printf("   is a full line, breaking\n");
#endif
            break;
        }
    }

#if CFGETLINE_DEBUG
    printf("cfile returns %zu != 0\n", off);
#endif
    /* True if we read anything. */
    return off != 0;
}

/*! \brief Print a formatted string to the file, from another function
 *
 *  The standard vfprintf implementation.  For those people that have
 *  to receive a '...' argument in their own function and send it to
 *  a cfile.
 *
 * @param fp
 *     The file handle to write to.
 * @param fmt
 *     The format string to print.
 * @param ap
 *     The compiled va_list of parameters to print.
 * @return
 *     The success of the file write operation.
 * @todo
 *     Should we be reusing a buffer rather than allocating one each time?
 */
int cvfprintf(cfile *fp, const char *fmt, va_list ap) {
    if (!fp || !fp->vptr) {
        errno = EINVAL;
        return -1;
    }
    return fp->vptr->vprintf(fp, fmt, ap);
}

/*! \brief Print a formatted string to the file
 *
 *  The standard fprintf implementation.  For bzip2 and gzip files this
 *  allocates a temporary buffer for each call.  This might seem
 *  inefficient, but otherwise we have the fgets problem all over
 *  again...
 *
 * @param fp
 *     The file handle to write to.
 * @param fmt
 *     The format string to print.
 * @param ...
 *     Any other variables to be printed using the format string.
 * @return
 *     The success of the file write operation.
 */

int cfprintf(cfile *fp, const char *fmt, ...) {
    /* if (!fp) return 0; # Checked by cvfprintf anyway */
    va_list ap;
    int rtn;
    
    va_start(ap, fmt);
    rtn = cvfprintf(fp, fmt, ap);
    va_end(ap);
    return rtn;
}

/*! \brief Read a block of data from the file.
 *
 *  Reads a given number of structures of a specified size from the
 *  file into the memory pointer given.  The destination memory must
 *  be allocated first.  Some read functions only specify one size,
 *  we use two here because that's what fread requires (and it's
 *  better for the programmer anyway IMHO).
 *
 * @param fp
 *     The file handle to read from.
 * @param ptr
 *     The memory to write into.
 * @param size
 *     The size of each structure in bytes.
 * @param num
 *     The number of structures to read.
 * @return
 *     The success of the file read operation.
 */

int cfread(cfile *fp, void *ptr, size_t size, size_t num) {
    if (!fp || !fp->vptr) {
        errno = EINVAL;
        return -1;
    }
    return fp->vptr->read(fp, ptr, size, num);
}

/*! \brief Write a block of data from the file.
 *
 *  Writes a given number of structures of a specified size into the
 *  file from the memory pointer given.
 *
 * @param fp
 *     The file handle to write into.
 * @param ptr
 *     The memory to read from.
 * @param size
 *     The size of each structure in bytes.
 * @param num
 *     The number of structures to write.
 * @return
 *     The success of the file write operation.
 */

int cfwrite(cfile *fp, const void *ptr, size_t size, size_t num) {
    if (!fp || !fp->vptr) {
        errno = EINVAL;
        return -1;
    }
    return fp->vptr->write(fp, ptr, size, num);
}

/*! \brief Flush the file's output buffer.
 *
 *  This function flushes any data passed to write or printf but not
 *  yet written to disk.  If the file is being read, it has no effect.
 *
 * @param fp
 *     The file handle to flush.
 * @return
 *     the success of the file flush operation.
 * @note
 *     for gzip files, under certain compression methods, flushing
 *     may result in lower compression performance.  We use Z_SYNC_FLUSH
 *     to write to the nearest byte boundary without unduly impacting
 *     compression.
 */

int cfflush(cfile *fp) {
    if (!fp || !fp->vptr) {
        errno = EINVAL;
        return -1;
    }
    return fp->vptr->flush(fp);
}

/*! \brief Close the given file handle.
 *
 *  This function frees the memory allocated for the file handle and
 *  closes the associated file.
 *
 * @param fp
 *     The file handle to close.
 * @return
 *     the success of the file close operation.
 */

int cfclose(cfile *fp) {
    if (!fp || !fp->vptr) {
        errno = EINVAL;
        return -1;
    }

    /*
     * Now, according to theory, the talloc destructor should close the
     * file correctly and pass back it's return code.
     */
    return talloc_free(fp);
}


/* vim: set ts=8 sw=4 et : */
