/*
 * cfile_lzo - compressed file read/write library
 * Copyright (C) 2012 Peter Miller
 * Copyright (C) 2013-2014 Paul Wayper <paulway@mabula.net>
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
 */

#include <string.h>
#include <errno.h>
#include <talloc.h>
#include <lzo/lzo1.h>

#include "cfile_private.h"
#include "cfile_buffer.h"
#include "cfile_lzo.h"

/* Predeclare function calls */
static off_t   cfile_lzo_size(cfile *fp);
static bool    cfile_lzo_eof(cfile *fp);
static char   *cfile_lzo_gets(cfile *fp, char *str, size_t len);
static ssize_t cfile_lzo_read(cfile *fp, void *ptr, size_t size, size_t num);
static ssize_t cfile_lzo_write(cfile *fp, const void *ptr, size_t size, size_t num);
static int     cfile_lzo_flush(cfile *fp);
static int     cfile_lzo_close(cfile *fp);

/*! \brief The lzo file structure
 *
 * 
 */
typedef struct cfile_lzo {
    cfile inherited; /*< our inherited function table */
    FILE *lzo;       /*< the actual lzo file - just a standard handle */
					 /*< the LZO structure? */
    bool writing;    /*< are we writing this file (i.e. encoding it),
                         or reading (i.e. decoding)? */
    cfile_buffer *buffer; /*< our buffer structure */
} cfile_lzo;

static const cfile_vtable lzo_cfile_table;


static off_t cfile_lzo_size(cfile *fp) {
    (void)fp;
    return 0;
}

static bool cfile_lzo_eof(cfile *fp) {
    (void)fp;
    return 1;
}

static char *cfile_lzo_gets(cfile *fp, char *buf, size_t bufsiz) {
    (void)fp;
    (void)buf;
    (void)bufsiz;
    return 0;
}

static int cfile_lzo_vprintf(cfile *fp, const char *fmt, va_list ap)
    __attribute ((format (printf, 2, 0)));

static int cfile_lzo_vprintf(cfile *fp, const char *fmt, va_list ap) {
    (void)fp;
    return vsnprintf(0, 0, fmt, ap);
}

static ssize_t cfile_lzo_read(cfile *fp, void *ptr, size_t size, size_t num) {
    (void)fp;
    (void)ptr;
    (void)size;
    (void)num;
    return 0;
}

static ssize_t
cfile_lzo_write(cfile *fp, const void *ptr, size_t size, size_t num) {
    (void)fp;
    (void)ptr;
    (void)size;
    return num;
}

static int cfile_lzo_flush(cfile *fp) {
    (void)fp;
    return 0;
}

static int cfile_lzo_close(cfile *fp) {
    (void)fp;
    return 0;
}

static const cfile_vtable cfile_lzo_vtable = {
    sizeof(cfile),
    cfile_lzo_size,
    cfile_lzo_eof,
    cfile_lzo_gets,
    cfile_lzo_vprintf,
    cfile_lzo_read,
    cfile_lzo_write,
    cfile_lzo_flush,
    cfile_lzo_close,
    "LZO file"
};

cfile * cfile_lzo_open(const char *pathname, const char *mode)
{
    return cfile_alloc(&cfile_lzo_vtable, pathname, mode);
}


/* vim: set ts=8 sw=4 et : */
