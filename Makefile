CFLAGS    = -Wall -Wshadow -Werror-implicit-function-declaration -Wstrict-prototypes -Wpointer-arith -Wcast-qual -Wcast-align -Wwrite-strings -Wmissing-format-attribute -Wformat=2 -Wno-format-y2k -Wno-declaration-after-statement -Wextra -Werror

all: libcfile.a

cfile_normal.o: cfile_normal.c cfile_normal.h
	gcc -c ${CFLAGS} -c cfile_normal.c -o cfile_normal.o

cfile_gzip.o: cfile_gzip.c cfile_gzip.h
	gcc -c ${CFLAGS} -lz -c cfile_gzip.c -o cfile_gzip.o

cfile_bzip2.o: cfile_bzip2.c cfile_bzip2.h
	gcc -c ${CFLAGS} -lbz2 -ltalloc -c cfile_bzip2.c -o cfile_bzip2.o

libcfile.a: cfile.c cfile.h cfile_normal.o cfile_gzip.o cfile_bzip2.o
	gcc -g ${CFLAGS} -c cfile.c -o cfile.o
	ar rv libcfile.a cfile.o cfile_normal.o cfile_gzip.o cfile_bzip2.o

test-cat: test-cat.c libcfile.a
	gcc -g ${CFLAGS} -I/usr/local/include -L/usr/local/lib -L. -lz -lbz2 -ltalloc -lcfile test-cat.c -o test-cat

clean:
	rm -f *.o *.a test-cat
