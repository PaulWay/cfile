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

#ifndef CFILE_LZO_H
#define CFILE_LZO_H

#include "cfile_private.h"

/**
  * The cfile_lzo function is used to open LZO files.
  *
  * @param pathname
  *     The name of the file to open.
  * @param mode
  * 	The mode to use for file operations (read or write).
  * @returns
  *     The new file handle
  */
cfile *cfile_lzo_open(const char *pathname, const char *mode);

/* vim: set ts=8 sw=4 et : */
#endif /* CFILE_LZO_H */
