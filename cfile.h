/*
 * cfile.h
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

/** \file cfile.h
 *  \brief The cfile library headers and public definitions.
 */

#ifndef __CFILE_H__
#define __CFILE_H__

#include <sys/types.h>

/**
  * \typedef cfile
  *     The file handle type definition.
  */
typedef struct cfile cfile;

/**
  * The cf_set_context function is used to set cfile's owned context.
  * This allows parents to close all cfiles.
  */
void cf_set_context(void *parent_context);

/**
  * The cfopen function is used to open a file, be it compressed or
  * uncompressed.
  *
  * @param name
  *     The pathname of the file to be opened, possibly with transparent
  *     gzip or bzip2 processing.
  * @oaram mode
  *     The open mode to use, see fopen(3) for more information.
  */
cfile *cfopen(const char *name, const char *mode);

/**
  * The cfdopen function is used to open a file descriptor.
  *
  * @param fildesc
  *     The file descriptor to access, it is treated as uncompressed.
  * @param mode
  *     The open mode to use, see fopen(3) for more information.
  */
cfile *cfdopen(int filedesc, const char *mode);

/**
  * The cfsize function is used to obtain the size of the (uncompressed)
  * file.
  *
  * @param fp
  *     The cfile handle of interest, returned by one of the #cfopen or
  *     #cfdopen functions.
  * @returns
  *     The size of the (uncompressed) file in bytes, or 0 if a
  *     determination cannot be made.
  */
off_t cfsize(cfile *fp);

/**
  * The cfeof functions is used to determine whether or not the file is
  * at the end-of-file marker.
  *
  * @param fp
  *     The cfile handle of interest, returned by one of the #cfopen or
  *     #cfdopen functions.
  * @returns
  *     true (non-zero) if position is at end-of-file marker,
  *     false (zero) if the file is not.
  */
int cfeof(cfile *fp);

/**
  * The cfgets function is used to read in at most one less than size
  * characters from #cf and stores them into the buffer pointed to by
  * #str.  Reading stops after an EOF, or a newline, or running out of
  * buffer space, as indicated by #len.  If a newline is read, it is
  * stored into the buffer.  A terminating NUL byte ('\0') is stored
  * after the last character in the #str buffer.
  *
  * @param fp
  *     The cfile handle of interest, returned by one of the #cfopen or
  *     #cfdopen functions.
  * @param str
  *     The buffer to receive the incoming data.
  * @param len
  *     The length of the buffer, in bytes.
  * @returns
  *     On success, returns the @a str pointer.
  *     On failure, returns the NULL pointer.
  */
char *cfgets(cfile *fp, char *str, int len);

/**
  * The cfgetline function is a more sensible way to get a line: create
  * a new line buffer as a char * and have an integer variable to
  * contain the current length of the buffer.  The cfgetline function
  * will read the file until it hits a newline character or end-of-file,
  * reallocating the line buffer to be larger if necessary.
  *
  * @param fp
  *     The cfile handle of interest, returned by one of the #cfopen or
  *     #cfdopen functions.
  * @param str
  *     The buffer to receive the incoming data.
  * @param maxline
  *     The currently allocated size of the #str buffer, in bytes.
  *     This will be enlarged as required to contin the whole line.
  * @returns
  *     On success, returns the @a str pointer.
  *     On failure, returns the NULL pointer.
  */
char *cfgetline(cfile *fp, char *line, int *maxline);

/**
  * The cfprintf function is used to
  * print a formatted string to the file.
  *
  * @param fp
  *     The cfile handle of interest, returned by one of the #cfopen or
  *     #cfdopen functions.
  * @param fmt
  *     The fornat control string.  It defined the order and types of
  *     the remaining arguments.  See fprintf(3) for more information.
  * @returns
  *     On success, returns the number of characters printed.
  *     On failure, returns a negative value.
  */
int cfprintf(cfile *fp, const char *fmt, ...)
  __attribute((format (printf, 2, 3)));

/**
  * The cvfprintf function is used to
  * print a formatted string to the file, from another function.
  *
  * @param fp
  *     The cfile handle of interest, returned by one of the #cfopen or
  *     #cfdopen functions.
  * @param fmt
  *     The fornat control string.  It defined the order and types of
  *     the remaining arguments.  See fprintf(3) for more information.
  * @param ap
  *     The rest of the arguments.
  * @returns
  *     On success, returns the number of characters printed.
  *     On failure, returns a negative value.
  */
int cvfprintf(cfile *fp, const char *fmt, va_list ap)
  __attribute((format (printf, 2, 0)));

/**
  * The cfread function is used to read #num structures of #size bytes
  * into the memory at #ptr.  No attempt at endianness conversion is
  * made.
  *
  * @param fp
  *     The cfile handle of interest, returned by one of the #cfopen or
  *     #cfdopen functions.
  * @param ptr
  *     Pointer to the base of the array to receive the data.
  * @param size
  *     The size, in bytes, of a single item in the array.
  * @param num
  *     The number of array elements to be read.
  * @returns
  *     the number of items successfully read (i.e., not the number of
  *     characters).  If an error occurs, or the end-of-file is reached,
  *     the return value is a short item count (or zero).
  */
int cfread(cfile *fp, void *ptr, size_t size, size_t num);

/**
  * The cfwrite function is used to write #num structures of #size bytes
  * from the memory at #ptr.  No attempt at endianness conversion is
  * made.
  *
  * @param fp
  *     The cfile handle of interest, returned by one of the #cfopen or
  *     #cfdopen functions.
  * @param ptr
  *     Pointer to the base of the array from which to take the data to
  *     be written.
  * @param size
  *     The size, in bytes, of a single item in the array.
  * @param num
  *     The number of array elements to be read.
  * @returns
  *     the number of items successfully read (i.e., not the number of
  *     characters).  If an error occurs, or the end-of-file is reached,
  *     the return value is a short item count (or zero).
  */
int cfwrite(cfile *fp, const void *ptr, size_t size, size_t num);

/**
  * The cfflush function is used to
  * flush the file's output buffer.
  *
  * @param fp
  *     The cfile handle of interest, returned by one of the #cfopen or
  *     #cfdopen functions.
  * @returns
  *     On success, 0 is returned.
  *     On failure, EOF is returned.
  */
int cfflush(cfile *fp);

/**
  * The cfclose function is used to close the file.
  * talloc_free'ing the pointer will also close the file.
  *
  * @param fp
  *     The cfile handle of interest, returned by one of the #cfopen or
  *     #cfdopen functions.
  * @returns
  *     On success, 0 is returned.
  *     On failure, EOF is returned.
  * @note
  *     The file will <b>not</b> be closed automatically if the pointer
  *     goes out of scope.
  * @note
  *     The behaviour of the cfclose function is undefined if the #fp
  *     parameter is an illegal pointer, or is a descriptor already
  *     passed to a previous invocation of the cfclose function.
  */
int cfclose(cfile *fp);

/* more functions as necessary */

#endif /* __CFILE_H__ */
