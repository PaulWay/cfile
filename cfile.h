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
 *  \brief The CFile library headers and public definitions.
 */

#ifndef __CFILE_H__
#define __CFILE_H__

#include <sys/types.h>

/** \typedef CFile The file handle type definition */
typedef struct cfile_struct CFile;

/* set cfile's owned context - allows parents to close all cfiles */
void cf_set_context(void *parent_context);

/* open a file, be it compressed or uncompressed */
CFile *cfopen(const char *name, const char *mode);
/* open a file descriptor; it is treated as uncompressed */
CFile *cfdopen(int filedesc, const char *mode);
/* return the size of the (uncompressed) file in bytes, or 0 if a
 * determination cannot be made */
off_t cfsize(CFile *fp);
/* Returns true if the file is at the end-of-file marker */
int cfeof(CFile *fp);
/* Reads at most len characters, or up to the newline character, from
 * the file into str and returns that pointer */
char *cfgets(CFile *fp, char *str, int len);
/* A more sensible way to get a line: create a new line buffer as a
 * char * and have an integer variable to contain the current length
 * of the buffer.  cfgetline will read the file until it hits a
 * newline character or end-of-file, reallocating the line buffer to
 * be larger if necessary.  */
char *cfgetline(CFile *fp, char *line, int *maxline);
/* print a formatted string to the file */
int cfprintf(CFile *fp, const char *fmt, ...)
  __attribute((format (printf, 2, 3)));
/* print a formatted string to the file, from another function */
int cvfprintf(CFile *fp, const char *fmt, va_list ap);
/* read num structures of size bytes into the memory at ptr */
int cfread(CFile *fp, void *ptr, size_t size, size_t num);
/* write num structures of size bytes from the memory at ptr */
/* no attempt at endianness conversion is made with cfread and cfwrite */
int cfwrite(CFile *fp, const void *ptr, size_t size, size_t num);
/* flush the file's output buffer. */
int cfflush(CFile *fp);
/* close the file.  talloc_free'ing the pointer will also close the
 * file.  It will not be closed automatically if the pointer goes out
 * of scope. */
int cfclose(CFile *fp);
/* more functions as necessary */

#endif /* __CFILE_H__ */
