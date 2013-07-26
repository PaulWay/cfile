/*
 * cfile - compressed file read/write library
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
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <cfile_null.h>


static off_t
cfile_null_size(cfile *fp)
{
    (void)fp;
    return 0;
}


static bool
cfile_null_eof(cfile *fp)
{
    (void)fp;
    return 1;
}


static char *
cfile_null_gets(cfile *fp, char *buf, size_t bufsiz)
{
    (void)fp;
    (void)buf;
    (void)bufsiz;
    return 0;
}


static int
cfile_null_vprintf(cfile *fp, const char *fmt, va_list ap)
    __attribute ((format (printf, 2, 0)));

static int
cfile_null_vprintf(cfile *fp, const char *fmt, va_list ap)
{
    (void)fp;
    return vsnprintf(0, 0, fmt, ap);
}


static ssize_t
cfile_null_read(cfile *fp, void *ptr, size_t size, size_t num)
{
    (void)fp;
    (void)ptr;
    (void)size;
    (void)num;
    return 0;
}


static ssize_t
cfile_null_write(cfile *fp, const void *ptr, size_t size, size_t num)
{
    (void)fp;
    (void)ptr;
    (void)size;
    return num;
}


static int
cfile_null_flush(cfile *fp)
{
    (void)fp;
    return 0;
}


static int
cfile_null_close(cfile *fp)
{
    (void)fp;
    return 0;
}


static const cfile_vtable cfile_null_vtable =
{
    sizeof(cfile),
    cfile_null_size,
    cfile_null_eof,
    cfile_null_gets,
    cfile_null_vprintf,
    cfile_null_read,
    cfile_null_write,
    cfile_null_flush,
    cfile_null_close,
    "Null file"
};


cfile *
cfile_null_open(const char *pathname, const char *mode)
{
    return cfile_alloc(&cfile_null_vtable, pathname, mode);
}


int
cfile_null_candidate(const char *pathname)
{
    return !strcmp(pathname, "/dev/null");
}


/* vim: set ts=8 sw=4 et : */
