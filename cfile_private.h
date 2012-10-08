#include "cfile.h"

extern void *pwlib_context;

/*! \brief The cfile virtual function table structure
 *
 * This structure primarily stores the series of pointers to the functions
 * that do the actual work for this file type.  cfopen then uses the
 * implementation's own opener to initialise these pointers.  Each
 * implementation then implements its own structure which includes this
 * structure as the first element, and adds whatever instance variables it
 * needs.  All those things are therefore in the implementations, rather
 * than stored here.  We do store the name of the file (or equivalent) here.
 * We also provide space for the implementation name at the end of the
 * structure, so that if this structure ever changes the compiler will break
 * the implementation using the old type.
 */

typedef struct cfile_vtable {
	/*! size of the implementation's private structure */
    size_t struct_size;
    /*! return the (uncompressed) size of this file */
    off_t (*size)(cfile *fp);
    /*! are we at the end of the file? */
    int   (*eof)(cfile *fp);     
    /*! get a string of given length */
    char *(*gets)(cfile *fp, char *str, int len);
    /*! print a line of variable args */
    int   (*vprintf)(cfile *fp, const char *fmt, va_list ap);
    /*! read bytes from the file */
    ssize_t (*read)(cfile *fp, void *ptr, size_t size, size_t num);
    /*! write bytes to the file */
    ssize_t (*write)(cfile *fp, const void *ptr, size_t size, size_t num); 
    /*! flush the contents to the disk */                            
    int   (*flush)(cfile *fp);
    /*! close the file, flushing the contents to disk */
    int   (*close)(cfile *fp); 
    /*! what implementation are we using here? */  
    char *implementation_name;
} cfile_vtable;

/*! \brief The cfile 'class'
 *
 */

typedef struct cfile {
    cfile_vtable *vptr; /*< pointer to virtual function table */
    char *filename;     /*< the name of this file, since we always have one */
} cfile;

cfile *cfile_alloc(
    const cfile_vtable *vptr, 
    const char *name, 
    const char *mode
);
