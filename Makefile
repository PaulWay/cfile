# took out -Wcast-qual for now
CFLAGS =  \
        -Wall \
        -Wshadow \
        -Werror-implicit-function-declaration \
        -Wstrict-prototypes \
        -Wpointer-arith \
        -Wcast-align \
        -Wwrite-strings \
        -Wmissing-format-attribute \
        -Wformat=2 \
        -Wno-format-y2k \
        -Wno-declaration-after-statement \
        -Wextra \
        -Werror

all: libcfile.a test-cat

cfile_normal.o: cfile_normal.c cfile_normal.h
	gcc -c ${CFLAGS} -c cfile_normal.c -o cfile_normal.o

cfile_gzip.o: cfile_gzip.c cfile_gzip.h
	gcc -c ${CFLAGS} -c cfile_gzip.c -o cfile_gzip.o

cfile_bzip2.o: cfile_bzip2.c cfile_bzip2.h
	gcc -c ${CFLAGS} -c cfile_bzip2.c -o cfile_bzip2.o

cfile.o: cfile.c cfile.h
	gcc -c ${CFLAGS} -c cfile.c -o cfile.o

libcfile.a: cfile.o cfile_normal.o cfile_gzip.o cfile_bzip2.o
	rm -f $@
	ar rv $@ cfile.o cfile_normal.o cfile_gzip.o cfile_bzip2.o

test-cat.o: test-cat.c cfile.h
	gcc -c ${CFLAGS} -c test-cat.c -o test-cat.o

test-cat: test-cat.o libcfile.a
	gcc -g test-cat.o -o $@ -L. -lcfile -lz -lbz2 -ltalloc

clean:
	rm -f *.o *.a test-cat
