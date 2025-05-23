# CFile

CFile is a library to allow you to read and write files independent of
what their compression method is.  It determines the correct set of
routines based on the name of the file, and it can use libMagic to
work the type out if that's available.

## How to use it

I've tried to make CFile a drop-in replacement for the standard stdio
file handling routines.  For example:

```c
cfile *fp = cfile_open("hello.txt", "w");
cfprintf(fp, "Hello, %s\n", "world");
cfclose(fp);
```

## What it provides

CFile provides the following functions:

Opening a file:

 * `cfile *cfile_open(char name[], char mode[])`
   - open a given file for reading or writing.
 * `cfile *cfile_dopen(int fd char mode[])`
   - open a file from a file descriptor.  This does not try to detect the
     type of file being read - all files are treated as uncompressed.

Reading from a file:

 * `bool cfeof(cfile *fp)`
   - return whether we have read past the end of the underlying file.
 * `off_t cfsize(cfile *fp)`
   - return the size of the uncompressed content of the file in bytes.  This
     is useful for knowing how far you have currently read through the file.
 * `char *cfgets(cfile *fp, char *str, size_t len)`
   - read at most len-1 bytes from the file, or until a newline - whichever
     happens first.  Terminates the string with \0.  Similar to fgets.
 * `bool cfgetline(cfile *fp, char **line)`
   - read an entire line of text from the file, until a newline or end of
     file.  Uses talloc_realloc to resize the line pointer behind the scenes,
     so you always get a complete line of text no matter how long it was.
     If you allocated the string, it will still be owned by your talloc
     context.  You can supply *line = NULL here, in which case you will get
     a string allocated under the cfile's context.
 * `int cfread(cfile *fp, void *ptr, size_t size, size_t num)`
   - Read at most num elements of size bytes into ptr.  Returns the number
     of elements (not bytes) read, which may be less than requested if we
     ran out of file to read.  Similar to fread.

Writing to a file:

 * `int cfprintf(cfile *fp, const char *fmt, ...)`
 * `int cvfprintf(cfile *fp, const char *fmt, va_list ap)`
   - print a string, formatted using the standard printf formatting syntax,
     to the file.  Similar to fprintf and vfprintf.
 * `int cfwrite(cfile *fp, const void *ptr, size_t size, size_t num)`
   - Write at most num elements of size bytes from ptr into the file.
     Returns the number of elements (not bytes) written, which may be less
     than the number requested if we ran out of space or a similar file
     error occurred.  Similar to fwrite.
 * `int cfflush(cfile *fp)`
   - Attempt to flush any current buffered data to disk.  Some compression
     libraries may clear internal state at this point, resulting in reduced
     compression.  Similar to fflush.

Closing the file:

 * `int cfclose(cfile *fp)`
   - close the given cfile.

Other utilities:

 * `void cfile_set_context(void *context)`
   - Set the talloc context for all Cfile allocations.  If you don't set one,
     cfile will automatically create its own.  If you supply a context and
     have already opened files, talloc_steal will be used to give your
     context the ownership of all the current cfile pointers.

# Building CFile

## Required libraries:

To compile CFile, you'll need the following:

on a Fedora system, install the packages:

```
libtalloc-devel
libattr-devel
libmagic-devel
zlib-devel
bzip2-devel
xz-devel
```

## How to compile it

CFile has fairly minimal build requirements:

```bash
$ make clean
rm -f *.o *.a test-cat
$ make
cc -Wall -Wshadow -Werror-implicit-function-declaration -Wmissing-prototypes -W
strict-prototypes -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-format
-attribute -Wformat=2 -Wno-format-y2k -Wdeclaration-after-statement -Wextra -We
rror -I.  -c -o cfile.o cfile.c
cc -Wall -Wshadow -Werror-implicit-function-declaration -Wmissing-prototypes -W
strict-prototypes -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-format
-attribute -Wformat=2 -Wno-format-y2k -Wdeclaration-after-statement -Wextra -We
rror -I.  -c -o cfile_bzip2.o cfile_bzip2.c
cc -Wall -Wshadow -Werror-implicit-function-declaration -Wmissing-prototypes -W
strict-prototypes -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-format
-attribute -Wformat=2 -Wno-format-y2k -Wdeclaration-after-statement -Wextra -We
rror -I.  -c -o cfile_gzip.o cfile_gzip.c
cc -Wall -Wshadow -Werror-implicit-function-declaration -Wmissing-prototypes -W
strict-prototypes -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-format
-attribute -Wformat=2 -Wno-format-y2k -Wdeclaration-after-statement -Wextra -We
rror -I.  -c -o cfile_normal.o cfile_normal.c
cc -Wall -Wshadow -Werror-implicit-function-declaration -Wmissing-prototypes -W
strict-prototypes -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-format
-attribute -Wformat=2 -Wno-format-y2k -Wdeclaration-after-statement -Wextra -We
rror -I.  -c -o cfile_null.o cfile_null.c
test -f libcfile.a && rm libcfile.a || true
ar cq libcfile.a cfile.o cfile_bzip2.o cfile_gzip.o cfile_normal.o cfile_null.o
cc -Wall -Wshadow -Werror-implicit-function-declaration -Wmissing-prototypes -W
strict-prototypes -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-format
-attribute -Wformat=2 -Wno-format-y2k -Wdeclaration-after-statement -Wextra -We
rror -I.  -c -o test-cat.o test-cat.c
gcc -g test-cat.o -o test-cat -L. -lcfile -lz -lbz2 -ltalloc
$
```

This will compile the library (libcfile.a) and the test programs 'test-cat',
'test-xz' and 'get_raw_size'.
