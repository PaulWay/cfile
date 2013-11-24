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

#ifndef CFILE_NULL_H
#define CFILE_NULL_H

#include "cfile_private.h"

/**
  * The cfile_null function is used to open a file that is always empty,
  * and discards all output written to it.
  *
  * @param pathname
  *     The name of the file of interest (ignored).
  * @param mode
  * 	The mode to use for file operations (ignored).
  * @returns
  *     The new file handle
  */
cfile *cfile_null_open(const char *pathname, const char *mode);

/**
  * The cfile_null_candidate function is used to determine whether or
  * not the given file looks like the /dev/null device.
  *
  * @param pathname
  *     The pat of the file to examine.
  * @returns
  *     true (non-zero) if the file is a candidate, or
  *     false (zero) if it is not.
  */
int cfile_null_candidate(const char *pathname);

/* vim: set ts=8 sw=4 et : */
#endif /* CFILE_NULL_H */
