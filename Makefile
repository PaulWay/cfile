#
# cfile - compressed file read/write library
# Copyright (C) 2006 Paul Wayper
# Copyright (C) 2012 Peter Miller
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or (at
# your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#

# took out -Wcast-qual for now
CFLAGS =  \
        -Wall \
        -Wshadow \
        -Werror-implicit-function-declaration \
        -Wmissing-prototypes \
        -Wstrict-prototypes \
        -Wpointer-arith \
        -Wcast-align \
        -Wwrite-strings \
        -Wmissing-format-attribute \
        -Wformat=2 \
        -Wno-format-y2k \
        -Wdeclaration-after-statement \
        -Wextra \
        -Werror \
	-g3 -ggdb -Wold-style-definition -Wmissing-declarations -Wundef
CPPFLAGS = \
	-I.

all: libcfile.a all-bin
all-bin: test-cat test-xz test_prelude get_raw_size

cfile_normal.o: cfile.h cfile_normal.h cfile_private.h

cfile_gzip.o: cfile.h cfile_gzip.h cfile_private.h

cfile_bzip2.o: cfile.h cfile_bzip2.h cfile_private.h cfile_buffer.h

cfile_xz.o: cfile.h cfile_xz.h cfile_private.h cfile_buffer.h

cfile.o: cfile.h cfile_private.h cfile_normal.h cfile_gzip.h cfile_bzip2.h cfile_buffer.h

cfile_buffer.o: cfile.h cfile_buffer.h

libfiles = \
	cfile.o \
	cfile_bzip2.o \
	cfile_gzip.o \
	cfile_lzo.o \
	cfile_normal.o \
	cfile_xz.o \
	cfile_null.o \
	cfile_buffer.o

#	cfile_ebadf.o \

libcfile.a: $(libfiles)
	-test -f $@ && rm $@ || true
	ar cq $@ $(libfiles)

test-cat: test-cat.o libcfile.a
	gcc ${CFLAGS} -g test-cat.o -o $@ -L. -lcfile -lz -lbz2 -ltalloc -llzma

test-xz: libcfile.a test-xz.c
	gcc ${CFLAGS} test-xz.c -o $@ -L. -lcfile -lz -lbz2 -ltalloc -llzma

get_raw_size: libcfile.a test-xz.c
	gcc ${CFLAGS} get_raw_size.c -o $@ -L. -lcfile -lz -lbz2 -ltalloc -llzma

_info: _info.o
	gcc -g _info.o -o $@

clean:
	rm -f *.o *.a test-cat test-xz

test_prelude: test/prelude.sh
	cat test/prelude.sh > $@
	chmod a+rx $@

check_sources = ${wildcard test/*/*.sh }
check_files = ${patsubst %.sh,%.es,${check_sources} }

%.es: %.sh all-bin
	PATH=`pwd`:$$PATH sh $*.sh

check: $(check_files)

# vim: set ts=8 sw=8 noet :
