/******************************************************************************
globus_libc.h

Description:
   Thread-safe libc macros, function prototypes

******************************************************************************/
#ifndef GLOBUS_INCLUDE_GLOBUS_LIBC_H_
#define GLOBUS_INCLUDE_GLOBUS_LIBC_H_ 1

#include "globus_config.h"

#include <sys/stat.h>

#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h>
#endif

#ifdef HAVE_SYS_SIGNAL_H
#   include <sys/signal.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <errno.h>
#include <pwd.h>

#ifdef HAVE_NETDB_H
#   include <netdb.h>
#endif

#include <sys/socket.h>
#include <netinet/in.h>

#include <sys/param.h>

#include <stdarg.h>

#if defined(TIME_WITH_SYS_TIME)
#   include <sys/time.h>
#   include <time.h>
#else
#   if HAVE_SYS_TIME_H
#       include <sys/time.h>
#   else
#       include <time.h>
#   endif
#endif

#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_FCNTL_H
#   include <fcntl.h>
#endif

#if defined(HAVE_DIRENT_H)
#   include <dirent.h>
#   define NAMLEN(dirent) strlen((dirent)->d_name)
#else
#   define dirent direct
#   define NAMLEN(dirent) (dirent)->d_namlen
#   define HAVE_DIRENT_NAMELEN 1
#   if defined(HAVE_SYS_NDIR_H)
#       include <sys/ndir.h>
#   endif
#   if defined(HAVE_SYS_DIR_H)
#       include <sys/dir.h>
#   endif
#   if defined(HAVE_NDIR_H)
#       include <ndir.h>
#   endif
#endif

#if defined(HAVE_SYS_UIO_H)
#   include <sys/uio.h>
#endif

#include <limits.h>

#ifndef EXTERN_C_BEGIN
#ifdef __cplusplus
#define EXTERN_C_BEGIN extern "C" {
#define EXTERN_C_END }
#else
#define EXTERN_C_BEGIN
#define EXTERN_C_END
#endif
#endif
 
EXTERN_C_BEGIN

typedef size_t globus_size_t;

#if !defined(HAVE_STRUCT_IOVEC)
struct iovec {
    void *		iov_base;
    int			iov_len;
};
#endif


/*
 * Reentrant lock
 */
#ifdef BUILD_LITE

#define globus_macro_libc_lock() (0)
#define globus_macro_libc_unlock() (0)

#else  /* BUILD_LITE */

extern globus_mutex_t globus_libc_mutex;

#define globus_macro_libc_lock() \
    globus_mutex_lock(&globus_libc_mutex)
#define globus_macro_libc_unlock() \
    globus_mutex_unlock(&globus_libc_mutex)

#endif /* BUILD_LITE */

#ifdef USE_MACROS
#define globus_libc_lock()   globus_macro_libc_lock()
#define globus_libc_unlock() globus_macro_libc_unlock()
#else  /* USE_MACROS */
extern int globus_libc_lock(void);
extern int globus_libc_unlock(void);
#endif /* USE_MACROS */



#if !defined(HAVE_THREAD_SAFE_STDIO) && !defined(BUILD_LITE)
#   define globus_stdio_lock globus_libc_lock
#   define globus_stdio_unlock globus_libc_unlock
    extern int globus_libc_printf(const char *format, ...);
    extern int globus_libc_fprintf(FILE *strm, const char *format, ...);
    extern int globus_libc_sprintf(char *s, const char *format, ...);
    extern int globus_libc_vprintf(const char *format, va_list ap);
    extern int globus_libc_vfprintf(FILE *strm, const char *format,va_list ap);
    extern int globus_libc_vsprintf(char *s, const char *format,va_list ap);
#else
#   define globus_stdio_lock()
#   define globus_stdio_unlock()
#   define globus_libc_printf   printf
#   define globus_libc_fprintf  fprintf
#   define globus_libc_sprintf  sprintf
#   define globus_libc_vprintf  vprintf
#   define globus_libc_vfprintf vfprintf
#   define globus_libc_vsprintf vsprintf
#endif

/*
 * File I/O routines
 */
#if !defined(HAVE_THREAD_SAFE_SELECT) && !defined(BUILD_LITE)

extern int globus_libc_open(char *path, int flags, ... /*int mode*/);
extern int globus_libc_close(int fd);
extern int globus_libc_read(int fd, char *buf, int nbytes);
extern int globus_libc_write(int fd, char *buf, int nbytes);
extern int globus_libc_writev(int fd, struct iovec *iov, int iovcnt);
extern int globus_libc_fstat(int fd, struct stat *buf);

extern DIR *globus_libc_opendir(char *filename);
extern long globus_libc_telldir(DIR *dirp);
extern void globus_libc_seekdir(DIR *dirp, long loc);
extern void globus_libc_rewinddir(DIR *dirp);
extern void globus_libc_closedir(DIR *dirp);

#else  /* HAVE_THREAD_SAFE_SELECT */

#define globus_libc_open open
#define globus_libc_close close
#define globus_libc_read read
#define globus_libc_write write
#if defined(HAVE_WRITEV)
#define globus_libc_writev writev
#else
#define globus_libc_writev(fd,iov,iovcnt) \
	    write(fd,iov[0].iov_base,iov[0].iov_len)
#endif
#define globus_libc_fstat fstat

#define globus_libc_opendir opendir
#define globus_libc_telldir telldir
#define globus_libc_seekdir seekdir
#define globus_libc_rewinddir rewinddir
#define globus_libc_closedir closedir

#endif /* HAVE_THREAD_SAFE_SELECT */


/*
 * Memory allocation routines
 */
#define globus_malloc(bytes) globus_libc_malloc(bytes)
#define globus_realloc(ptr,bytes) globus_libc_realloc(ptr,bytes)
#define globus_calloc(nobjs,bytes) globus_libc_calloc(nobjs,bytes)
#define globus_free(ptr) globus_libc_free(ptr)
    
#if !defined(BUILD_LITE)

extern void *globus_libc_malloc(size_t bytes);
extern void *globus_libc_realloc(void *ptr,
				 size_t bytes);
extern void *globus_libc_calloc(size_t nobj, 
				size_t bytes);
extern void globus_libc_free(void *ptr);

extern void *globus_libc_alloca(size_t bytes);

#else  /* BUILD_LITE */

#define globus_libc_malloc	malloc
#define globus_libc_realloc	realloc
#define globus_libc_calloc	calloc
#define globus_libc_free	free

#define globus_libc_alloca	alloca

#endif /* BUILD_LITE */

#ifdef TARGET_ARCH_CRAYT3E
extern void *alloca(size_t bytes);
#endif /* TARGET_ARCH_CRAYT3E */

/* Never a macro because globus_off_t must match largefile definition */
extern int globus_libc_lseek(int fd, globus_off_t offset, int whence);

/* Miscellaneous libc functions (formerly md_unix.c) */
int globus_libc_gethostname(char *name, int len);
int globus_libc_getpid(void);
int globus_libc_fork(void);
int globus_libc_usleep(long usec);
double globus_libc_wallclock(void);

/* returns # of characters printed to s */
extern int globus_libc_sprint_off_t(char * s, globus_off_t off);
/* returns 1 if scanned succeeded */
extern int globus_libc_scan_off_t(char *s, globus_off_t *off, int *consumed);

/* single interface to reentrant libc functions we use*/
struct hostent *globus_libc_gethostbyname_r(char *name,
					    struct hostent *result,
					    char *buffer,
					    int buflen,
					    int *h_errnop);
struct hostent *globus_libc_gethostbyaddr_r(char *addr,
					    int length,
					    int type,
					    struct hostent *result,
					    char *buffer,
					    int buflen,
					    int *h_errnop);
char *globus_libc_ctime_r(time_t *clock,
			  char *buf,
			  int buflen);

int globus_libc_getpwnam_r(char *name,
			   struct passwd *pwd,
			   char *buffer,
			   int bufsize,
			   struct passwd **result);

int globus_libc_getpwuid_r(uid_t uid,
			   struct passwd *pwd,
			   char *buffer,
			   int bufsize,
			   struct passwd **result);
int globus_libc_setenv(register const char *name,
		       register const char *value,
		       int rewrite);
void globus_libc_unsetenv(register const char *name);
char *globus_libc_getenv(register const char *name);

int globus_libc_readdir_r(DIR *dirp,
			  struct dirent **result);

char *globus_libc_system_error_string(int the_error);

char *
globus_libc_strdup(const char * source);

int
globus_libc_vprintf_length(const char * fmt, va_list ap);

/* not really 'libc'... but a convenient place to put it in */
int globus_libc_gethomedir(char *result, int bufsize);

#ifndef HAVE_MEMMOVE
#  define memmove(d, s, n) bcopy ((s), (d), (n))
#  define HAVE_MEMMOVE
#endif

EXTERN_C_END

#endif


