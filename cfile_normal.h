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

#ifndef CFILE_NORMAL_H
#define CFILE_NORMAL_H

/**
  * The normal_open function is used to open a regular file that is not
  * compressed.
  *
  * @param name
  *     The pathname of the file to be opened.
  * @param mode
  *     The file open mode, e.g. "r" or "w".
  * @returns
  *     pointer to new cfile, or NULL on failure
  * @note
  *     FIXME: This pollutes the global linker name space.
  *     Need to rename it cfile_normal_open instead.
  */
cfile *normal_open(const char *name, const char *mode);

/**
  * The normal_dopen function is used to open a file descriptor that is
  * not compressed.
  *
  * @param filedesc
  *     The file descriptor of interest.  From this point, the
  *     cfile-normal instance "owns" the file descriptor, and will close
  *     it at will.
  * @param mode
  *     The file open mode, e.g. "r" or "w".
  * @returns
  *     pointer to new cfile, or NULL on failure
  * @note
  *     FIXME: This pollutes the global linker name space.
  *     Need to rename it cfile_normal_dopen instead.
  */
cfile *normal_dopen(const int filedesc, const char *mode);

// vim: set ts=8 sw=4 et :
#endif // CFILE_NORMAL_H
