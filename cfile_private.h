/*
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

#ifndef CFILE_PRIVATE_H
#define CFILE_PRIVATE_H

#include "cfile.h"

extern void *pwlib_context;

/*! \brief The cfile virtual function table structure
 *
 * This structure primarily stores the series of pointers to the functions
 * that do the actual work for this file type.  cfopen then uses the
 * implementation's own opener to initialise these pointers.  Each
 * implementation then implements its own structure which includes this
 * structure as the first element, and adds whatever instance variables it
 * needs.  All those things are therefore in the implementations, rather
 * than stored here.  We do store the name of the file (or equivalent) here.
 * We also provide space for the implementation name at the end of the
 * structure, so that if this structure ever changes the compiler will break
 * the implementation using the old type.
 */

typedef struct cfile_vtable {
	/*! size of the implementation's private structure */
    size_t struct_size;
    /*! return the (uncompressed) size of this file */
    off_t (*size)(cfile *fp);
    /*! are we at the end of the file? */
    int   (*eof)(cfile *fp);
    /*! get a string of given length */
    char *(*gets)(cfile *fp, char *str, int len);
    /*! print a line of variable args */
    int (*vprintf)(cfile *fp, const char *fmt, va_list ap)
        __attribute ((format (printf, 2, 0)));
    /*! read bytes from the file */
    ssize_t (*read)(cfile *fp, void *ptr, size_t size, size_t num);
    /*! write bytes to the file */
    ssize_t (*write)(cfile *fp, const void *ptr, size_t size, size_t num);
    /*! flush the contents to the disk */
    int   (*flush)(cfile *fp);
    /*! close the file, flushing the contents to disk */
    int   (*close)(cfile *fp);
    /*! what implementation are we using here? */
    const char *implementation_name;
} cfile_vtable;

/*! \brief The cfile 'class'
 *
 */

typedef struct cfile {
    const cfile_vtable *vptr; /*< pointer to virtual function table */
    char *filename;     /*< the name of this file, since we always have one */
} cfile;

cfile *cfile_alloc(
    const cfile_vtable *vptr,
    const char *name,
    const char *mode
);

#endif /* CFILE_PRIVATE_H */
/* vim: set ts=8 sw=4 et : */
