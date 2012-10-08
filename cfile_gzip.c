#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <zlib.h>
#include <talloc.h>

#include "cfile_private.h"
#include "cfile_gzip.h"

/*! \brief The gzip file structure
 *
 * We only need to store the actual (zlib) file pointer.
 */
typedef struct cfile_gzip {
    cfile inherited; /*< our inherited function table */
    gzFile *gp;             /*< the actual zlib file pointer */
} cfile_gzip;

static const cfile_vtable gzip_cfile_table;

/*! \brief Open a file for reading or writing
 *
 *  Open the given file using the given mode.  Opens the file and
 *  returns a cfile handle to it.  Mode must start with 'r' or 'w'
 *  to read or write (respectively) - other modes are not expected
 *  to work.
 *
 * \return A successfully created file handle, or NULL on failure.
 */
cfile *gzip_open(const char *name, /*!< The name of the file to open.  
               At this stage we don't attempt to pick up reading stdin or
               writing stdout as gzip compressed streams. */
              const char *mode) /*!< "r" to specify reading, "w" for writing. */
{
    gzFile *own_file = gzopen(name, mode);
    if (!own_file) {
        /* Keep any errno set by gzopen - let it handle any invalid modes,
           etc. */
        return NULL;
    }
    cfile_gzip *cfzp = (cfile_gzip *)cfile_alloc(&gzip_cfile_table, name, mode);
    if (!cfzp) {
        errno = EINVAL;
        return NULL;
    }
    cfzp->gp = own_file;
    return (cfile *)cfzp;
}

/* We don't, as yet, support opening a file descriptor as a gzip stream. */

/*! \brief Returns the _uncompressed_ file size
 *
 *  Determining the uncompressed file size is fairly
 *  easy with gzip files - the size is a 32-bit little-endian signed
 *  int (I think) at the end of the file.
 *
 * \param fp The file handle to check
 * \return The number of bytes in the uncompressed file.
 */

off_t gzip_size(cfile *fp) {
    FILE *rawfp = fopen(fp->filename,"rb"); /* open the compressed file directly */
    if (!rawfp) {
        return 0;
    }
    fseek(rawfp,-4,2);
    int size; /* Make sure this is a 32-bit int! */
    fread(&size,4,1,rawfp);
    fclose(rawfp);
    return (off_t)size;
}

/*! \brief Returns true if we've reached the end of the file being read.
 *
 *  This passes through the state of the lower-level's EOF checking.
 *
 * \param fp The file handle to check.
 * \return True (1) if the file has reached EOF, False (0) if not.
 */

int gzip_eof(cfile *fp) {
    cfile_gzip *cfzp = (cfile_gzip *)fp;
    return gzeof(cfzp->gp);
}

/*! \brief Get a string from the file, up to a maximum length or newline.
 *
 *  For gzipped files this simply uses zlib's fgets implementation.
 *
 * \param fp The file handle to read from.
 * \param str An array of characters to read the file contents into.
 * \param len The maximum length, plus one, of the string to read.  In
 *  other words, if this is 10, then fgets will read a maximum of nine
 *  characters from the file.  The character after the last character
 *  read is always set to \\0 to terminate the string.  The newline
 *  character is kept on the line if there was room to read it.
 * \return A pointer to the string thus read.
 */
 
char *gzip_gets(cfile *fp, char *str, int len) {
    cfile_gzip *cfzp = (cfile_gzip *)fp;
    return gzgets(cfzp->gp, str, len);
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
int gzip_vprintf(cfile *fp, const char *fmt, va_list ap) {
    cfile_gzip *cfzp = (cfile_gzip *)fp;
    int rtn;
    char *buf = talloc_vasprintf(fp, fmt, ap);
    /* Problem in zlib forbids gzprintf of more than 4095 characters
     * at a time.  Use gzwrite to get around this, assuming that it
     * doesn't have the same problem... */
    rtn = gzwrite(cfzp->gp, buf, strlen(buf));
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
 
int gzip_read(cfile *fp, void *ptr, size_t size, size_t num) {
    cfile_gzip *cfzp = (cfile_gzip *)fp;
    return gzread(cfzp->gp, ptr, size * num);
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
 
int gzip_write(cfile *fp, const void *ptr, size_t size, size_t num) {
    cfile_gzip *cfzp = (cfile_gzip *)fp;
    return gzwrite(cfzp->gp, ptr, size * num);
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
 
int gzip_flush(cfile *fp) {
    cfile_gzip *cfzp = (cfile_gzip *)fp;
    return gzflush(cfzp->gp, Z_SYNC_FLUSH);
}

/*! \brief Close the given file handle.
 *
 *  This function frees the memory allocated for the file handle and
 *  closes the associated file.
 * \param fp The file handle to close.
 * \return the success of the file close operation.
 */
 
int gzip_close(cfile *fp) {
    cfile_gzip *cfzp = (cfile_gzip *)fp;
    return gzclose(cfzp->gp);
}

/*! \brief The function dispatch table for gzip files */

static const cfile_vtable gzip_cfile_table = {
    sizeof(gzip_cfile_table),
    &gzip_size,
    &gzip_eof,
    &gzip_gets,
    &gzip_vprintf,
    &gzip_read,
    &gzip_write,
    &gzip_flush,
    &gzip_close,
    "GZip file"
};


