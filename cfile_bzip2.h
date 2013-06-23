//
// cfile - compressed file read/write library
// Copyright (C) 2006 Paul Wayper
// Copyright (C) 2012 Peter Miller
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or (at
// your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//

#ifndef CFILE_BZIP2_H
#define CFILE_BZIP2_H

cfile *bzip2_open(const char *name, const char *mode);
off_t bzip2_size(cfile *fp);
int bzip2_eof(cfile *fp);
char *bzip2_gets(cfile *fp, char *str, int len);
ssize_t bzip2_read(cfile *fp, void *ptr, size_t size, size_t num);
ssize_t bzip2_write(cfile *fp, const void *ptr, size_t size, size_t num);
int bzip2_flush(cfile *fp);
int bzip2_close(cfile *fp);

#endif
