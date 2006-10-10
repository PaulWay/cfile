/*
 * cfile.c
 * This file is part of The PaulWay Libraries
 *
 * Copyright (C) 2006 - Paul Wayper (paulway@mabula.net)
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
 * along with The PaulWay Libraries; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

/** \mainpage The CFile Library
 *
 * \section introduction Introduction
 *
 *  Put simply, this library is designed to allow your code to read
 *  or write a file regardless of whether it is uncompressed, or
 *  compressed with either bzip2 or gzip.  It automatically detects
 *  the file's extension and uses the appropriate library routines.
 *  If the file name is "-", then stdin or stdout is opened as
 *  appropriate.  As a further service, the cfgetline routine allows
 *  you to read lines of any size from your input file,
 *  automatically resizing the buffer to suit.  Other convenience
 *  routines, such as cfsize, are provided.
 *
 * \section requirements Requirements
 *
 *  The following libraries are required for CFile:
 *   - The talloc library from http://talloc.samba.org must be
 *     installed.
 *   - zlib and bzlib must be installed.
 *   - In order to determine the uncompressed file size of bzip2
 *     file, the bzcat and wc binaries must be available to the
 *     calling program.
 *
 * \section optional Optional extras
 *
 *  If the libmagic library is defined at the time of compiling
 *  the cfile library, then libmagic will be used to determine the
 *  type of files being read.  Files being written will still have
 *  their type determined by their file extension.
 *
 * \section notes Notes
 *
 *  The file extension for gzip files is '.gz'.
 *
 *  The file extension for bzip2 files is '.bz2'.
 *
 *  If an uncompressed file is being read, the stdio routines will
 *  always be used, despite zlib supporting opening and reading both
 *  gzip-compressed files and uncompressed files.
 *
 *  CFile files do not support random access, reading and writing, or
 *  appending.
 *
 * \todo Add better error and EOF checking, particularly for bzip.
 * \todo Allow only read or write modes, with no appending.
 * \todo Allow extra parameters in the mode string to specify
 *  compression options.
 */
 
#include <zlib.h>
#include <bzlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <talloc.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "cfile.h"

void *pwlib_context = NULL;

/*! \enum filetype_enum
 *  \typedef CFile_type
 *  \brief A set of the types of file we recognise.
 */
typedef enum filetype_enum {
    UNCOMPRESSED, /*!< Indicates an uncompressed file */
    GZIPPED,      /*!< Indicates a file compresed with zlib */
    BZIPPED       /*!< Indicates a file compressed with bzlib */
} CFile_type;

/*! \brief the structure of the actual file handle information we pass around
 *
 * This structure contains all the information we need to tote around to
 * access the file, be it through zlib or bzlib or stdio.
 */
struct cfile_struct {
    char *name;         /*!< The name of the file opened */
    CFile_type filetype;/*!< The type of the file opened (see CFile_type) */
    union {             /*!< The various file pointers, all in one box */
        gzFile *gp;     /*!< The gzip typed pointer */
        FILE *fp;       /*!< The regular uncompressed file pointer */
        BZFILE *bp;     /*!< The bzip2 typed file pointer */
    } fileptr;          /*!< The structure used to contain all the file pointers */
    char *buffer;       /*!< Used for buffering fgetc reads from bzip2 files */
    /* This doesn't need to be initialised except for bzip2 files */
    int buflen,         /*!< The length of the content in the buffer */
        bufpos;         /*!< The current position of the next character */
};

/*! The size of the character buffer for reading lines from bzip2 files.
 *
 *  This isn't really a file cache, just a way of saving us single-byte
 *  calls to bzread.
 */
#define CFILE_BUFFER_SIZE 1024

/*! \brief close the file when the file pointer is destroyed.
 *
 *  This function is given to talloc_set_destructor so that, when the
 *  the user does a talloc_free on the file handle or any context that
 *  contained it, the file that was opened with that handle is
 *  automatically closed.
 * \param fp The file handle that is being destroyed.  Thanks to
 *  improvements in the talloc library, this is now a typed pointer
 *  (it was formerly a void pointer that we had to cast).
 * \return The result of closing the file.  This should be 0 when
 *  the file close was successful.
 * \todo The status of closing bzip2 files should be checked.
 */
static int cf_destroyclose(CFile *fp) {
     /* Success = 0, failure = -1 here */
    if (!fp) return 0; /* If we're given null, then assume success */
/*    fprintf(stderr, "_cf_destroyclose(%p)\n", fp);*/
    if (fp->filetype == GZIPPED) {
        return gzclose(fp->fileptr.gp);
        /* I'm not sure of what zlib's error code is here, but one hopes
         * that it's compatible with fclose */
    } else if (fp->filetype == BZIPPED) {
        /* bzclose doesn't return anything - make something up. */
        BZ2_bzclose(fp->fileptr.bp);
        return 0;
    } else {
        return fclose(fp->fileptr.fp);
    }
    /* I'm not sure what should happen if the file couldn't be closed
     * here - the above application and talloc should handle it... */
}

/*! \brief The common things we do with a file handle when it's being opened.
 *
 *  This function sets the remaining fields that are common to all files.
 *  We have this as a separate function because it's called from various
 *  parts of cfopen and also from cfdopen.
 * \param fp The file handle to finalise.
 */
static void finalise_open(CFile *fp) {
    fp->buffer = NULL;
    fp->buflen = 0;
    fp->bufpos = 0;
    talloc_set_destructor(fp, cf_destroyclose);
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
static int bz_fgetc(CFile *fp) {
    if (! fp) return 0;
    /* Should we move this check and creation to the initialisation,
     * so it doesn't slow down the performance of fgetc? */
    if (! fp->buffer) {
        fp->buffer = talloc_array(fp, char, CFILE_BUFFER_SIZE);
        if (! fp->buffer) {
            fprintf(stderr,
                "Error: No memory for bzip2 read buffer!\n"
            );
            return EOF;
        }
    }
    if (fp->buflen == fp->bufpos) {
        fp->bufpos = 0;
        fp->buflen = BZ2_bzread(fp->fileptr.bp, fp->buffer, CFILE_BUFFER_SIZE);
        if (fp->buflen <= 0) return EOF;
    }
    return fp->buffer[fp->bufpos++]; /* Ah, the cleverness of postincrement */
}

/*! \brief Detect the file type from its extension
 *
 *  A common routine to detect the file type from its extension.  This
 *  probably also detects a file with the name like 'foo.gzbar'.
 * \param name The name of the file to check.
 * \return The determined file type.
 * \see CFile_type
 * \todo Make sure that the extension is at the end of the file?
 */
static CFile_type file_extension_type(const char *name) {
    if        (strstr(name,".gz" ) != NULL) {
        return GZIPPED;
    } else if (strstr(name,".bz2") != NULL) {
        return BZIPPED;
    } else {
        return UNCOMPRESSED;
    }
}

/*! \brief Open a file for reading or writing
 *
 *  Open the given file using the given mode.  Opens the file and
 *  returns a CFile handle to it.  Mode must start with 'r' or 'w'
 *  to read or write (respectively) - other modes are not expected
 *  to work.
 *
 * \param name The name of the file to open.  If this is "-", then
 *  stdin is read from or stdout is written to, as appropriate (both
 *  being used uncompressed.)
 * \param mode "r" to specify reading, "w" for writing.
 * \return A successfully created file handle, or NULL on failure.
 */
CFile *cfopen(const char *name, const char *mode) {
    if (!pwlib_context) {
        pwlib_context = talloc_init("PWLib Talloc context");
    }
    CFile *fp = talloc_zero(pwlib_context, CFile);
    if (! fp) {
        fprintf(stderr,
            "Error: no memory for new file handle opening '%s'\n",
            name
        );
        return NULL;
    }
    talloc_set_name(fp, "CFile '%s' (mode '%s')", name, mode);
    fp->name = talloc_strdup(fp, name);
    /* If we have a '-' as a file name, dup stdin or stdout */
    if (strcmp(name, "-") == 0) {
        fp->filetype = UNCOMPRESSED;
        if (strstr(mode, "w") != 0) {
            fp->fileptr.fp = fdopen(fileno(stdout), mode);
        } else if (strstr(mode, "r") != 0) {
            fp->fileptr.fp = fdopen(fileno(stdin), mode);
        } else {
            fprintf(stderr,
                "Error: Can't open - with mode %s!\n", mode
            );
            fp->fileptr.fp = NULL;
        }
        if (! fp->fileptr.fp) {
            talloc_free(fp);
            return NULL;
        }
        finalise_open(fp);
        return fp;
    }
    /* At some stage we should really replace this with a better test,
     * Maybe one based on magic numbers */
#ifdef MAGIC_NONE
    /* We can only determine the file type if it exists - i.e. is being
     * read. */
    if (strstr(mode, "r") != NULL) {
        magic_t checker = magic_open(MAGIC_NONE);
        if (! checker) {
            /* Give up and go back to file extension */
            fp->filetype = file_extension_type(name);
        } else {
            char *type = magic_file(checker, name);
            if (strstr(type, "gzip compressed data") != NULL) {
                fp->filetype = GZIPPED;
            } else if (strstr(type, "bzip2 compressed data") != NULL) {
                fp->filetype = BZIPPED;
            } else {
                fp->filetype = UNCOMPRESSED;
            }
            magic_close(checker);
        }
    } else {
        fp->filetype = file_extension_type(name);
    }
#else
    fp->filetype = file_extension_type(name);
#endif
    /* Even though zlib allows reading of uncompressed files, let's
     * not complicate things too much at this stage :-) */
    if (fp->filetype == GZIPPED) {
        /* Should we do something about specifying a compression level? */
        fp->fileptr.gp = gzopen(name, mode);
    } else if (fp->filetype == BZIPPED) {
        fp->fileptr.bp = BZ2_bzopen(name, mode);
    } else {
        fp->fileptr.fp = fopen(name, mode);
    }
    if (!(fp->fileptr.fp)) {
        talloc_free(fp);
        return NULL;
    }
    finalise_open(fp);
    return fp;
}

/*! \brief Open a file from a file descriptor
 *
 *  Allows you to open the file specified by the given file descriptor,
 *  with the same mode options as a regular file.  Originally necessary
 *  to allow access to stdin and stdout, but with the current handling
 *  of "-" by cfopen this should be mostly unnecessary.
 * \param filedesc An integer file descriptor number.
 * \param mode The mode to open the file in ("r" for read, "w" for write).
 * \return A successfully created file handle, or NULL on failure.
 */

CFile *cfdopen(int filedesc, const char *mode) {
    if (!pwlib_context) {
        pwlib_context = talloc_init("PWLib Talloc context");
    }
    CFile *fp = talloc_zero(pwlib_context, CFile);
    fp->filetype = UNCOMPRESSED;
    fp->fileptr.fp = fdopen(filedesc, mode);
    if (!(fp->fileptr.fp)) {
        talloc_free(fp);
        return NULL;
    }
    finalise_open(fp);
    return fp;
}

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
 * \param fp The file handle to check
 * \return The number of bytes in the uncompressed file.
 */

off_t cfsize(CFile *fp) {
    if (!fp) return 0;
    if (fp->filetype == GZIPPED) {
        FILE *rawfp = fopen(fp->name,"rb"); /* open the compressed file directly */
        if (!rawfp) {
            return 0;
        }
        fseek(rawfp,-4,2);
        int size; /* Make sure this is a 32-bit int! */
        fread(&size,4,1,rawfp);
        fclose(rawfp);
        return (off_t)size;
    } else if (fp->filetype == BZIPPED) {
        /* There's no file size information in the file.  So we have
         * to feed the entire file through bzcat and count its characters.
         * Tedious, but then hopefully you only have to do this once; and
         * at least it may cache the file for further reading.  In other
         * words, getting the size of a bzipped file takes a number of
         * seconds - caveat caller... */
        const int max_input_size = 20;
        char *cmd = talloc_asprintf(fp, "bzcat '%s' | wc -c", fp->name);
        if (! cmd) {
            return 0;
        }
        char *input = talloc_array(fp, char, max_input_size);
        if (! input) {
            talloc_free(cmd);
            return 0;
        }
        FILE *fpipe = popen(cmd, "r");
        if (fpipe) {
            input = fgets(input, max_input_size, fpipe);
        }
        pclose(fpipe);
        talloc_free(cmd);
        long fsize = atol(input);
        talloc_free(input);
        return fsize;
    } else {
        struct stat sp;
        if (stat(fp->name, &sp) == 0) {
            return sp.st_size;
        } else {
            return 0;
        }
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

int cfeof(CFile *fp) {
    if (!fp) return 0;
    if (fp->filetype == GZIPPED) {
        return gzeof(fp->fileptr.gp);
    } else if (fp->filetype == BZIPPED) {
        int errno;
        BZ2_bzerror(fp->fileptr.bp, &errno);
        /* this actually returns a pointer to the error message,
         * but we're not using it in this context... */
        if (errno == 0) {
            /* bzerror doesn't appear to be reporting BZ_STREAM_END
             * when it's run out of characters */
            /* But if we've allocated a buffer, and its length and
             * position are now zero, then we're at the end of it AFAICS */
            if (fp->buffer != NULL && fp->buflen == 0 && fp->bufpos == 0) {
                return 1;
            }
        }
        return (errno == BZ_OK
             || errno == BZ_RUN_OK
             || errno == BZ_FLUSH_OK
             || errno == BZ_FINISH_OK) ? 0 : 1;
        /* From my reading of the bzip2 documentation, all error
         * conditions and the 'OK' condition of BZ_STREAM_END
         * indicate that you can't read from the file any more, which
         * is a logical EOF in my book. */
    } else {
        return feof(fp->fileptr.fp);
    }
}

/*! \brief Get a string from the file, up to a maximum length or newline.
 *
 *  For gzipped and uncompressed files, this simply uses their relative
 *  library's fgets implementation.  Since bzlib doesn't provide such a
 *  function, we have to copy the implementation from stdio.c and use
 *  it here, referring to our own bz_fgetc function.
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
 
char *cfgets(CFile *fp, char *str, int len) {
    if (!fp) return 0;
    if (fp->filetype == GZIPPED) {
        return gzgets(fp->fileptr.gp, str, len);
    } else if (fp->filetype == BZIPPED) {
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
    } else {
        return fgets(str, len, fp->fileptr.fp);
    }
}

/*! Macro to check whether the line is terminated by a newline or equivalent */
#define isafullline(line,len) ((line)[(len-1)] == '\n' || (line)[(len-1)] == '\r')

/*! \brief Read a full line from the file, regardless of length
 *
 *  Of course, with fgets you can't always guarantee you've read an entire
 *  line.  You have to know the length of the longest line, in advance, in
 *  order to read each line from the file in one call.  cfgetline solves
 *  this problem by progressively extending the string you pass until the
 *  entire line has been read.  To do this it uses talloc_realloc, and a
 *  variable which holds the length of the line allocated so far.  If you
 *  haven't initialised the line beforehand, cfgetline will do so
 *  (allocating it against the file pointer's context).  If you have, then
 *  the magic of talloc_realloc allocates the new space against the
 *  context that you originally allocated your buffer against.  So to
 *  speak.
 *
 *  In normal usage, this 'buffer' will expand but never contract.  It
 *  expands to half again its current size, so if you have a very long
 *  line lurking in your input somewhere, then it's going to set the
 *  buffer size for all the lines after it.  If you're concerned by this
 *  wasting a lot of memory, then set the length negative (while keeping
 *  its absolute size).  This will signal to cfgetline to shrink the
 *  line buffer after this line has been read.  For example, if your line
 *  buffer is currently 1024 and you want it to shrink, then set it to
 *  -1024 before calling cfgetline.  In reality, this is almost never
 *  going to be a problem.
 * \param fp The file handle to read from.
 * \param line A character array to read the line into, and optionally
 *  extend.
 * \param maxline A pointer to an integer which will contain the length of
 *  the string currently allocated.
 * \return A pointer to the line thus read.  If talloc_realloc has had to
 *  move the pointer, then this will be different from the line pointer
 *  passed in.  Therefore, the correct usage of cfgetline is something
 *  like 'line = cfgetline(fp, line, &len);'
 */
 
char *cfgetline(CFile *fp, char *line, int *maxline) {
    /* Get a line from the file into the buffer which has been preallocated
     * to maxline.  If the line from the file is too big to fit, we extend
     * the buffer and increase maxline.
     * If you pass a NULL pointer, it will allocate memory for you initially
     * (although against the pwlib's context rather than your own).
     * If maxline is zero, we'll reset it to something reasonable.  (These
     * two options allow you to start with a blank slate and let cfgetline
     * do all the work.
     * If you pass a negative maxline, it'll assume that the absolute value
     * is the size you want but will shrink the allocated memory down to the
     * minimum required to store the line afterward.
     * Otherwise, maxline is assumed to be the length of your string.  You'd
     * better have this right... :-)
     */
    /* Check for the 'shrink' option */
    char shrink = (*maxline < 0);
    if (shrink) {
        *maxline *= -1;
    }
    /* Check for a zero maxline and reset it */
    if (*maxline == 0) {
        *maxline = 80;
    }
    /* Allocate the string if it isn't already */
    if (NULL == line) {
        line = talloc_array(fp, char, *maxline);
    }
    /* Get the line thus far */
    if (! cfgets(fp, line, *maxline)) {
        return NULL;
    }
    unsigned len = strlen(line);
    unsigned extend = 0;
    while (!cfeof(fp) && !isafullline(line,len)) {
        /* Add on how much we want to extend by */
        extend = len / 2;
        *maxline += extend;
        /* talloc_realloc automagically knows which context to use here :-) */
        line = talloc_realloc(fp, line, char, *maxline);
        /* Get more line */
        if (! cfgets(fp, line + len, extend)) {
            /* No more line - what do we return now? */
            if (len == 0) {
                return NULL;
            } else {
                break;
            }
        }
        /* And set our line length */
        len = strlen(line);
    }
    /* If we've been asked to shrink, do so */
    if (shrink) {
        *maxline = len + 1;
        line = talloc_realloc(fp, line, char, *maxline);
    }
    return line;
}

/*! \brief Print a formatted string to the file
 *
 *  The standard fprintf implementation.  For bzip2 and gzip files this
 *  allocates a temporary buffer for each call.  This might seem
 *  inefficient, but otherwise we have the fgets problem all over
 *  again...
 * \param fp The file handle to write to.
 * \param fmt The format string to print.
 * \param ... Any other variables to be printed using the format string.
 * \return The success of the file write operation.
 */

int cfprintf(CFile *fp, const char *fmt, ...) {
    if (!fp) return 0;
    va_list ap;
    va_start(ap, fmt);
    int rtn;
    if (fp->filetype == GZIPPED) {
        char *buf = talloc_vasprintf(fp, fmt, ap);
        rtn = gzprintf(fp->fileptr.gp, "%s", buf);
        talloc_free(buf);
    } else if (fp->filetype == BZIPPED) {
        char *buf = talloc_vasprintf(fp, fmt, ap);
        rtn = BZ2_bzwrite(fp->fileptr.bp, buf, strlen(buf));
        talloc_free(buf);
    } else {
        rtn = vfprintf(fp->fileptr.fp, fmt, ap);
    }
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
 * \param fp The file handle to read from.
 * \param ptr The memory to write into.
 * \param size The size of each structure in bytes.
 * \param num The number of structures to read.
 * \return The success of the file read operation.
 */
 
int cfread(CFile *fp, void *ptr, size_t size, size_t num) {
    if (!fp) return 0;
    int rtn;
    if (fp->filetype == GZIPPED) {
        rtn = gzread(fp->fileptr.gp, ptr, size * num);
    } else if (fp->filetype == BZIPPED) {
        rtn = BZ2_bzread(fp->fileptr.bp, ptr, size * num);
    } else {
        rtn = fread(ptr, size, num, fp->fileptr.fp);
    }
    return rtn;
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
 
int cfwrite(CFile *fp, const void *ptr, size_t size, size_t num) {
    if (!fp) return 0;
    int rtn;
    if (fp->filetype == GZIPPED) {
        rtn = gzwrite(fp->fileptr.gp, ptr, size * num);
    } else if (fp->filetype == BZIPPED) {
        rtn = BZ2_bzwrite(fp->fileptr.bp, ptr, size * num);
    } else {
        rtn = fwrite(ptr, size, num, fp->fileptr.fp);
    }
    return rtn;
}

/*! \brief Close the given file handle.
 *
 *  This function frees the memory allocated for the file handle and
 *  closes the associated file.
 * \param fp The file handle to close.
 * \return the success of the file close operation.
 */
 
int cfclose(CFile *fp) {
    if (!fp) return 0;
    /* Now, according to theory, the talloc destructor should close the
     * file correctly and pass back it's return code */
    return talloc_free(fp);
}
