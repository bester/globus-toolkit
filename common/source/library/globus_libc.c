/*****************************************************************************
globus_libc.c

Description:
   Thread-safe libc macros, function prototypes

CVS Information:
   $Source$
   $Date$
   $Revision$
   $State$
   $Author$
******************************************************************************/

/******************************************************************************
			     Include header files
******************************************************************************/
#include "globus_libc.h"
#include "globus_thread_common.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

# if !defined(alloca)
/* AIX requires this to be the first thing in the file.  */
#ifdef __GNUC__
# define alloca __builtin_alloca
#else
# if HAVE_ALLOCA_H
#  include <alloca.h>
# else
#  ifdef _AIX
#pragma alloca
#  else
#   ifndef alloca /* predefined by HP cc +Olibcalls */
#     ifndef _CRAYT3E
char *alloca ();
#     endif
#   endif
#  endif
# endif
#endif
#endif

#if !defined(MAXPATHLEN) 
#   include <sys/param.h>
#   define MAXPATHLEN PATH_MAX
#endif

/* HPUX 10.20 headers do not define this */
#if defined(TARGET_ARCH_HPUX)
extern int h_errno;
#endif

extern globus_bool_t globus_i_module_initialized;
/******************************************************************************
		       Define module specific variables
******************************************************************************/
/* mutex to make globus_libc reentrant */
globus_mutex_t globus_libc_mutex;

/******************************************************************************
		      Module specific function prototypes
******************************************************************************/
static void globus_l_libc_copy_hostent_data_to_buffer(struct hostent *h,
					              char *buffer,
					              size_t buflen);
static void globus_l_libc_copy_pwd_data_to_buffer(struct passwd *pwd,
						  char *buffer,
						  size_t buflen);

/******************************************************************************
Function: globus_libc_lock()

Description: 

Parameters: 

Returns:
******************************************************************************/
#undef globus_libc_lock
int
globus_libc_lock(void)
{
    if(globus_i_module_initialized==GLOBUS_TRUE)
    {
        return globus_macro_libc_lock();
    }
    return GLOBUS_FAILURE;
} /* globus_libc_lock() */

/******************************************************************************
Function: globus_libc_unlock()

Description: 

Parameters: 

Returns:
******************************************************************************/
#undef globus_libc_unlock
int
globus_libc_unlock(void)
{
    if(globus_i_module_initialized==GLOBUS_TRUE)
    { 
        return globus_macro_libc_unlock();
    }
    return GLOBUS_FAILURE;
} /* globus_libc_unlock() */



/******************************************************************************
Function: globus_libc_strncasecmp
 
Description: 

Parameters: 

Returns:
 ******************************************************************************/
int
globus_libc_strncasecmp(
	const char *                            s1,
	const char *                            s2,
	globus_size_t                           n)
{
    int                                     rc;
    int                                     save_errno;

    globus_libc_lock();

#   if HAVE_STRNCASECMP    
    {
        rc = strncasecmp(s1, s2, n);
    }
#   else
    {
        char ch1;
        char ch2;
        int  ctr;

        for(ctr = 0; ctr < n; ctr++)
        {
	        if(s2[ctr] == '\0' && s1[ctr] == '\0')
	        {
	            rc = 0;
	            goto exit;
	        } 
        	else if(s2[ctr] == '\0')
	        {
	            rc = -1;
	            goto exit;
	        }
	        else if(s1[ctr] == '\0')
	        {
	            rc = 1;
	            goto exit;
	        }
            else
	        {
                ch1 = toupper(s2[ctr]);
                ch2 = toupper(s1[ctr]);
                if(ch2 > ch1)
		        {
                    rc = 1;
                    goto exit;
		        }
                else if(ch2 < ch1)
		        {
                    rc = -1;
                    goto exit;
		        }
            }
        }
        rc = 0;
    }
#   endif

  exit:
    save_errno = errno;

    globus_libc_unlock();
    errno = save_errno;

    return(rc);
}


#if !defined(HAVE_THREAD_SAFE_SELECT) && !defined(BUILD_LITE)

/******************************************************************************
Function: globus_libc_open()

Description: 

Parameters: 

Returns:
******************************************************************************/
#undef globus_libc_open
int
globus_libc_open(char *path,
		 int flags,
		 ... /*int mode*/)
{
    va_list ap;
    int rc;
    int save_errno;
    int mode=0;

    globus_libc_lock();
    

    if(flags & O_CREAT)
    {
#       ifdef HAVE_STDARG_H
        {
            va_start(ap, flags);
	    }
#       else
	    {
            va_start(ap);
	    }
#       endif
        mode = va_arg(ap, int);
        va_end(ap);
    }

    rc = open(path, flags, mode);
    save_errno = errno;
    /* Should set the fd to non-blocking here */
    globus_libc_unlock();
    errno = save_errno;
    return(rc);
} /* globus_libc_open() */

/******************************************************************************
Function: globus_libc_close()

Description: 

Parameters: 

Returns:
******************************************************************************/
#undef globus_libc_close
int
globus_libc_close(int fd)
{
    int rc;
    int save_errno;
    globus_libc_lock();
    rc = close(fd);
    save_errno = errno;
    /* Should convert EWOULDBLOCK to EINTR */
    globus_libc_unlock();
    errno = save_errno;
    return(rc);
} /* globus_libc_close() */
 
 
/******************************************************************************
Function: globus_libc_read()

Description: 

Parameters: 

Returns:
******************************************************************************/
#undef globus_libc_read
int
globus_libc_read(int fd,
		 char *buf,
		 int nbytes)
{
    int rc;
    int save_errno;
    globus_libc_lock();
    rc = read(fd, buf, nbytes);
    save_errno = errno;
    /* Should convert EWOULDBLOCK to EINTR */
    globus_libc_unlock();
    errno = save_errno;
    return(rc);
} /* globus_libc_read() */
   
/******************************************************************************
Function: globus_libc_writev()

Description: 

Parameters: 

Returns:
******************************************************************************/
#undef globus_libc_writev
int
globus_libc_writev(
    int					fd,
    struct iovec *			iov,
    int					iovcnt)
{
    int					rc;
    int					save_errno;

#if defined(HAVE_WRITEV)
    globus_libc_lock();
    rc = writev(fd, iov, iovcnt);
    save_errno = errno;


    globus_libc_unlock();

    errno = save_errno;

    return rc;
#else
    return globus_libc_write(fd,
		             iov[0].iov_base,
		             iov[0].iov_len);
#endif
} /* globus_libc_writev() */
 
/******************************************************************************
Function: globus_libc_write()

Description: 

Parameters: 

Returns:
******************************************************************************/
#undef globus_libc_write
int
globus_libc_write(int fd,
		  char *buf,
		  int nbytes)
{
    int rc;
    int save_errno;
    globus_libc_lock();
    rc = write(fd, buf, nbytes);
    save_errno = errno;
    /* Should convert EWOULDBLOCK to EINTR */
    globus_libc_unlock();
    errno = save_errno;
    return(rc);
} /* globus_libc_write() */
 
/******************************************************************************
Function: globus_libc_fstat()

Description: 

Parameters: 

Returns:
******************************************************************************/
#undef globus_libc_fstat
int
globus_libc_fstat(int fd,
		  struct stat *buf)
{
    int rc;
    int save_errno;
    globus_libc_lock();
    rc = fstat(fd, buf);
    save_errno = errno;
    /* Should convert EWOULDBLOCK to EINTR */
    globus_libc_unlock();
    errno = save_errno;
    return(rc);
} /* globus_libc_fstat() */
 
#endif /* !defined(HAVE_THREAD_SAFE_SELECT) && !defined(BUILD_LITE) */

 
#if !defined(BUILD_LITE)
/******************************************************************************
Function: globus_libc_malloc()

Description: 

Parameters: 

Returns:
******************************************************************************/
#undef globus_libc_malloc
void *
globus_libc_malloc(
    size_t					bytes)
{
    globus_bool_t				done;
    int						save_errno;
    void *					ptr;

    do
    {
		globus_libc_lock();
		{
			ptr = (void *) malloc(bytes);
			save_errno = errno;
		}
		globus_libc_unlock();

		if (ptr == GLOBUS_NULL &&
			(save_errno == EINTR ||
			save_errno == EAGAIN ||
			save_errno == EWOULDBLOCK))
		{
			done = GLOBUS_FALSE;
			globus_thread_yield();
		}
		else
		{
			done = GLOBUS_TRUE;
		}
    }
	while (!done);
    
    errno = save_errno;
    return(ptr);
}
/* globus_libc_malloc() */

/******************************************************************************
Function: globus_libc_realloc()

Description: 

Parameters: 

Returns:
******************************************************************************/
#undef globus_libc_realloc
void *
globus_libc_realloc(void *ptr,
		    size_t bytes)
{
    int save_errno;
    
    globus_libc_lock();
    ptr = (void *) realloc(ptr, bytes);
    save_errno = errno;

    /* Should convert EWOULDBLOCK to EINTR */
    globus_libc_unlock();
    errno = save_errno;
    return(ptr);
} /* globus_libc_realloc() */

/******************************************************************************
Function: globus_libc_calloc()

Description: 

Parameters: 

Returns:
******************************************************************************/
#undef globus_libc_calloc
void *
globus_libc_calloc(size_t nelem,
		   size_t elsize)
{
    int save_errno;
    void *ptr;
    
    globus_libc_lock();
    ptr = (void *) calloc(nelem, elsize);
    save_errno = errno;

    /* Should convert EWOULDBLOCK to EINTR */
    globus_libc_unlock();
    errno = save_errno;
    return(ptr);    
} /* globus_libc_calloc() */

/******************************************************************************
Function: globus_libc_free()

Description: 

Parameters: 

Returns:
******************************************************************************/
#undef globus_libc_free
void
globus_libc_free(void *ptr)
{
    int save_errno;
    
    globus_libc_lock();
    free (ptr);
    save_errno = errno;

    /* Should convert EWOULDBLOCK to EINTR */
    globus_libc_unlock();
    errno = save_errno;

    return;
} /* globus_libc_free() */
    
/******************************************************************************
Function: globus_libc_alloca()

Description: 

Parameters: 

Returns:
******************************************************************************/
#undef globus_libc_alloca
void *
globus_libc_alloca(size_t bytes)
{
    int save_errno;
    void *ptr;
    
    globus_libc_lock();
    ptr = (void *) alloca(bytes);
    save_errno = errno;
    /* Should convert EWOULDBLOCK to EINTR */
    globus_libc_unlock();
    errno = save_errno;
    return(ptr);
} /* globus_libc_alloca() */

/******************************************************************************
Function: globus_libc_printf()

Description: 

Parameters: 

Returns:
******************************************************************************/
#undef globus_libc_printf
int
globus_libc_printf(const char *format, ...)
{
    va_list ap;
    int rc;
    int save_errno;

    globus_libc_lock();
#ifdef HAVE_STDARG_H
    va_start(ap, format);
#else
    va_start(ap);
#endif

    rc = vprintf(format, ap);
    save_errno=errno;

    globus_libc_unlock();

    errno=save_errno;
    return rc;
} /* globus_libc_printf() */

/******************************************************************************
Function: globus_libc_fprintf()

Description: 

Parameters: 

Returns:
******************************************************************************/
#undef globus_libc_fprintf
int
globus_libc_fprintf(FILE *strm, const char *format, ...)
{
    va_list ap;
    int rc;
    int save_errno;

    if(strm == GLOBUS_NULL)
    {
	return -1;
    }
    globus_libc_lock();

#ifdef HAVE_STDARG_H
    va_start(ap, format);
#else
    va_start(ap);
#endif
    
    rc = vfprintf(strm, format, ap);
    save_errno=errno;

    globus_libc_unlock();

    errno=save_errno;
    return rc;
} /* globus_libc_fprintf() */

/******************************************************************************
Function: globus_libc_sprintf()

Description: 

Parameters: 

Returns:
******************************************************************************/
#undef globus_libc_sprintf
int
globus_libc_sprintf(char *s, const char *format, ...)
{
    va_list ap;
    int rc;
    int save_errno;

    globus_libc_lock();

#ifdef HAVE_STDARG_H
    va_start(ap, format);
#else
    va_start(ap);
#endif
    
    rc = vsprintf(s, format, ap);
    save_errno=errno;

    globus_libc_unlock();

    errno=save_errno;
    return rc;
} /* globus_libc_sprintf() */

/******************************************************************************
Function: globus_libc_vprintf()

Description: 

Parameters: 

Returns:
******************************************************************************/
#undef globus_libc_vprintf
int
globus_libc_vprintf(const char *format, va_list ap)
{
    int rc;
    int save_errno;

    globus_libc_lock();

    rc = vprintf(format, ap);
    save_errno=errno;

    globus_libc_unlock();

    errno=save_errno;
    return rc;
} /* globus_libc_vprintf() */

/******************************************************************************
Function: globus_libc_vfprintf()

Description: 

Parameters: 

Returns:
******************************************************************************/
#undef globus_libc_vfprintf
extern int
globus_libc_vfprintf(FILE *strm, const char *format, va_list ap)
{
    int rc;
    int save_errno;
    
    if(strm == GLOBUS_NULL)
    {
	return -1;
    }
    globus_libc_lock();

    rc = vfprintf(strm, format, ap);
    save_errno=errno;

    globus_libc_unlock();

    errno=save_errno;
    return rc;
} /* globus_libc_vfprintf() */

/******************************************************************************
Function: globus_libc_vsprintf()

Description: 

Parameters: 

Returns:
******************************************************************************/
#undef globus_libc_vsprintf
int
globus_libc_vsprintf(char *s, const char *format, va_list ap)
{
    int rc;
    int save_errno;
    
    globus_libc_lock();

    rc = vsprintf(s, format, ap);
    save_errno=errno;

    globus_libc_unlock();

    errno=save_errno;
    return rc;
} /* globus_libc_vsprintf() */

#endif /* !defined(BUILD_LITE)*/

static
int
globus_l_libc_vsnprintf(char *s, size_t n, const char *format, va_list ap)
{
    int rc;
    int save_errno;
    va_list ap_copy;

    globus_libc_va_copy(ap_copy,ap);
    
    globus_libc_unlock();
    rc = globus_libc_vprintf_length( format, ap_copy);
    globus_libc_lock();

    va_end(ap_copy);

    if ( rc < 0 )
    {
	return rc;
    }
    else if ( rc < n )
    {
	return vsprintf( s, format, ap );
    }
    else
    {
	char *buf = malloc( rc + 1 );
	if (buf == NULL)
	{
	    return -1;
	}
	rc = vsprintf( buf, format, ap );
	save_errno = errno;
	strncpy( s, buf, n - 1 );
	s[n - 1] = '\0';
	free( buf );
	errno = save_errno;
	return rc;
    }
}

/******************************************************************************
Function: globus_libc_snprintf()

Description: 

Parameters: 

Returns:
******************************************************************************/
#undef globus_libc_snprintf
int
globus_libc_snprintf(char *s, size_t n, const char *format, ...)
{
    va_list ap;
    int rc;
    int save_errno;

    globus_libc_lock();

#ifdef HAVE_STDARG_H
    va_start(ap, format);
#else
    va_start(ap);
#endif

#if defined(HAVE_VSNPRINTF)
    rc = vsnprintf(s, n, format, ap);
#else
    rc = globus_l_libc_vsnprintf(s, n, format, ap);
#endif
    save_errno=errno;

    globus_libc_unlock();

    errno=save_errno;
    return rc;
} /* globus_libc_snprintf() */

/******************************************************************************
Function: globus_libc_vsnprintf()

Description: 

Parameters: 

Returns:
******************************************************************************/
#undef globus_libc_vsnprintf
int
globus_libc_vsnprintf(char *s, size_t n, const char *format, va_list ap)
{
    int rc;
    int save_errno;
    
    globus_libc_lock();

#if defined(HAVE_VSNPRINTF)
    rc = vsnprintf(s, n, format, ap);
#else
    rc = globus_l_libc_vsnprintf(s, n, format, ap);
#endif
    save_errno=errno;

    globus_libc_unlock();

    errno=save_errno;
    return rc;
} /* globus_libc_vsnprintf() */

/*
 * Print a globus_off_t to a string. The format for the off_t depends
 * on the size of the data type, which may vary with flavor and
 * architecture.
 */
int
globus_libc_sprint_off_t(char * s, globus_off_t off)
{
    return globus_libc_sprintf(s, "%" GLOBUS_OFF_T_FORMAT, off);
}

/*
 * Scan a globus_off_t from a string. Equivalent to 
 * sscanf("%d%n", off, consumed) (with %d replaced with the
 * appropriately-sized integer type.
 */
int
globus_libc_scan_off_t(char * s, globus_off_t * off, int * consumed)
{
    int rc;
    int dummy;

    if(consumed == GLOBUS_NULL)
    {
	consumed = &dummy;
    }
    globus_libc_lock();

    rc = sscanf(s, "%" GLOBUS_OFF_T_FORMAT "%n", off, consumed);
    globus_libc_unlock();
    return rc;
}

/******************************************************************************
Function: globus_libc_gethostname()

Description: 

Parameters: 

Returns:
******************************************************************************/
int
globus_libc_gethostname(char *name, int len)
{
    static char                         hostname[MAXHOSTNAMELEN];
    static size_t                       hostname_length = 0;
    static globus_mutex_t               gethostname_mutex;
    static int                          initialized = GLOBUS_FALSE;
    char *                              env;
    
    globus_libc_lock();
    if(initialized == GLOBUS_FALSE)
    {
        globus_mutex_init(&gethostname_mutex,
                          (globus_mutexattr_t *) GLOBUS_NULL);
        initialized = GLOBUS_TRUE;
    }
    globus_libc_unlock();
    
    globus_mutex_lock(&gethostname_mutex);
    
    if (hostname_length == 0U &&
        (env = globus_libc_getenv("GLOBUS_HOSTNAME")) != GLOBUS_NULL)
    {
        strncpy(hostname, env, MAXHOSTNAMELEN);
        hostname_length = strlen(hostname);
    }
    if (hostname_length == 0U)
    {
        struct hostent *                hp_ptr = GLOBUS_NULL;
        struct hostent                  hp2;
        char                            hp_tsdbuffer[500];
        int                             hp_errnop;

        if (gethostname(hostname, MAXHOSTNAMELEN) < 0)
        {
            globus_mutex_unlock(&gethostname_mutex);
            return(-1);
        }
        
        hostname_length = strlen(hostname);
        if(strchr(hostname, '.') != GLOBUS_NULL)
        {
            unsigned int                i = 0;
            for (i=0; i<hostname_length; i++)
            {
                hostname[i] = tolower(hostname[i]);
            }
            strncpy(name, hostname, len);
            globus_mutex_unlock(&gethostname_mutex);
            return 0;
        }
        
        hp_ptr = globus_libc_gethostbyname_r(hostname,
                                             &hp2,
                                             hp_tsdbuffer,
                                             500,
                                             &hp_errnop);
        if (hp_ptr != NULL)
        {
            struct in_addr              addr;
            struct hostent              hostent_by_addr;
            char                        buf[500];
            int                         errno_by_addr;
            
#           if defined (TARGET_ARCH_CRAYT3E) \
                        || defined (TARGET_ARCH_CRAYT90)
            {
                memcpy(&(addr.s_da),
                       hp_ptr->h_addr,
                       hp_ptr->h_length);
            }
#           else
            {
                memcpy(&(addr.s_addr), hp_ptr->h_addr, hp_ptr->h_length);
            }
#           endif
            hp_ptr = globus_libc_gethostbyaddr_r((void *) &addr,
                                                 sizeof(addr),
                                                 AF_INET,
                                                 &hostent_by_addr,
                                                 buf,
                                                 500,
                                                 &errno_by_addr);

            if (hp_ptr != NULL && strcmp(hp_ptr->h_name, hostname) != 0)
            {
                strcpy(hostname, hp_ptr->h_name);
            }
            else
            {
                if(strchr(hostname, '.') == GLOBUS_NULL &&
                   hp_ptr != GLOBUS_NULL)
                {
                    int                 i;
                    for(i = 0; hp_ptr->h_aliases[i] != GLOBUS_NULL; i++)
                    {
                        if(strchr(hp_ptr->h_aliases[i], '.') != GLOBUS_NULL)
                        {
                            strcpy(hostname, hp_ptr->h_aliases[i]);
                            hp_ptr = NULL;
                            break;
                        }
                    }
                }
            }
        }
    }

    if(strchr(hostname, '.') == GLOBUS_NULL &&
       (env = globus_libc_getenv("GLOBUS_DOMAIN_NAME")) != GLOBUS_NULL)
    {
        if(strlen(hostname) +
           strlen(env) + 2 < MAXHOSTNAMELEN)
        {
            strcat(hostname, ".");
            strcat(hostname,
                   globus_libc_getenv("GLOBUS_DOMAIN_NAME"));
        }
    }

    hostname_length = strlen(hostname);
    if (hostname_length < (size_t) len)
    {
        size_t i;
        for (i=0; i<hostname_length; i++)
           hostname[i] = tolower(hostname[i]);
        strcpy(name, hostname);
    }
    else
    {
        globus_mutex_unlock(&gethostname_mutex);
        errno=EFAULT;
        return(-1);
    }

    globus_mutex_unlock(&gethostname_mutex);
    return(0);
} /* globus_libc_gethostname() */


/*
 *  The windows definition of the following funtions differs
 */
#if defined(TARGET_ARCH_WIN32)

int
globus_libc_system_memory(
    globus_off_t *                  mem)
{
    MEMORYSTATUSEX                      statex;

    if(mem == GLOBUS_NULL)
    {
        return -1;
    }

    statex.dwLength = sizeof(statex);
    GlobalMemoryStatusEx(&statex);

    *mem = statex.ullTotalPhys;

    return 0;
}

int
globus_libc_free_memory(
    globus_off_t *                  mem)
{
    MEMORYSTATUSEX                      statex;

    if(mem == GLOBUS_NULL)
    {
        return -1;
    }

    statex.dwLength = sizeof(statex);
    GlobalMemoryStatusEx(&statex);

    *mem = statex.ullAvailPhys;
    
    return 0;
}

int
globus_libc_usleep(long usec)
{
	globus_libc_lock();
	Sleep(usec/1000);
	globus_libc_unlock();

	return 0;
}

int
globus_libc_getpid(void)
{
    int pid;
    
    globus_libc_lock();

    pid = (int) _getpid();

    globus_libc_unlock();

    return(pid);
} /* globus_libc_getpid() */

int
globus_libc_fork(void)
{
    return -1;
}

#else /* TARGET_ARCH_WIN32 */

int
globus_libc_system_memory(
    globus_size_t *                  mem)
{
    return -1;
}

int
globus_libc_free_memory(
    globus_size_t *                  mem)
{
    return -1;
}

/******************************************************************************
Function: globus_libc_getpid()

Description: 

Parameters: 

Returns:
******************************************************************************/
int
globus_libc_getpid(void)
{
    int pid;
    
    globus_libc_lock();

    pid = (int) getpid();

    globus_libc_unlock();

    return(pid);
} /* globus_libc_getpid() */

/******************************************************************************
Function: globus_libc_fork()

Description: 

Parameters: 

Returns:
******************************************************************************/
int
globus_libc_fork(void)
{
    int child;

    globus_thread_prefork();

#   if defined(HAVE_FORK1)
    {
	child = fork1();
    }
#   else
    {
	child = fork();
    }
#   endif

    globus_thread_postfork();

    return child;
} /* globus_libc_fork() */

/******************************************************************************
Function: globus_libc_usleep()

Description: 

Parameters: 

Returns:
******************************************************************************/
int
globus_libc_usleep(long usec)
{
    struct timeval timeout;
    
    timeout.tv_sec = usec/1000000;
    timeout.tv_usec = usec%1000000;

#   if !defined(HAVE_THREAD_SAFE_SELECT)
    {
	    globus_libc_lock();
    }
#   endif
    
    select(0, NULL, NULL, NULL, &timeout);
    
#   if !defined(HAVE_THREAD_SAFE_SELECT)
    {
	     globus_libc_unlock();
    }
#   endif
	
    return GLOBUS_SUCCESS;
} /* globus_libc_usleep() */
#endif /* TARGET_ARCH_WIN32 */

/******************************************************************************
Function: globus_libc_wallclock()

Description: 

Parameters: 

Returns:
******************************************************************************/
double
globus_libc_wallclock(void)
{
    globus_abstime_t now;
    long sec;
    long usec;

    GlobusTimeAbstimeGetCurrent(now);
    GlobusTimeAbstimeGet(now, sec, usec);
    return (((double) sec) + ((double) usec) / 1000000.0);
} /* globus_libc_wallclock() */


/******************************************************************************
Function: globus_libc_getbyhostname_r()

Description: 

Parameters: 

Returns:
******************************************************************************/
struct hostent *
globus_libc_gethostbyname_r(
    char *                              hostname,
    struct hostent *                    result,
    char *                              buffer,
    int                                 buflen,
    int *                               h_errnop)
{
    struct hostent *                    hp = GLOBUS_NULL;
#   if defined(GLOBUS_HAVE_GETHOSTBYNAME_R_3)
    struct hostent_data                 hp_data;
    int                                 rc;
#   endif
#   if defined(GLOBUS_HAVE_GETHOSTBYNAME_R_6)
    int                                 rc;
#   endif

    globus_libc_lock();

#   if !defined(HAVE_GETHOSTBYNAME_R)
    {
        hp = gethostbyname(hostname);
	if(hp != GLOBUS_NULL)
	{
            memcpy(result, hp, sizeof(struct hostent));
            globus_l_libc_copy_hostent_data_to_buffer(result, buffer, (size_t) buflen);
	    hp = result;
	    if (h_errnop != GLOBUS_NULL)
	    {
		*h_errnop = 0;
	    }
	}
	else
	{
	    if (h_errnop != GLOBUS_NULL)
	    {
	        *h_errnop = h_errno;
	    }
	} 
    }
#   elif defined(GLOBUS_HAVE_GETHOSTBYNAME_R_3)
    {
	    rc = gethostbyname_r(hostname,
			     result,
			     &hp_data);
        if(rc == 0)
	    {
            globus_l_libc_copy_hostent_data_to_buffer(result, buffer, (size_t) buflen);
	        hp = result;
	        if(h_errnop != GLOBUS_NULL)
	        {
		        *h_errnop = h_errno;
	        }
        }
	    else
	    {
	        hp = GLOBUS_NULL;
	        if(h_errnop != GLOBUS_NULL)
	        {
		        *h_errnop = h_errno;
	        }
        }
    }
#   elif defined(GLOBUS_HAVE_GETHOSTBYNAME_R_5)
    {
        hp = gethostbyname_r(hostname,
			     result,
			     buffer,
			     buflen,
			     h_errnop);
    }
#   elif defined(GLOBUS_HAVE_GETHOSTBYNAME_R_6)
    {
        rc = gethostbyname_r(hostname,
			     result,
			     buffer,
			     buflen,
			     &hp,
			     h_errnop);
    }
#   else
    {
	    GLOBUS_HAVE_GETHOSTBYNAME symbol must be defined!!!;
    }
#   endif

    globus_libc_unlock();

    /*
     * gethostbyname() on many machines does the right thing for IP addresses
     * (e.g., "140.221.7.13").  But on some machines (e.g., SunOS 4.1.x) it
     * doesn't.  So hack it in this case.
     */
    if (hp == GLOBUS_NULL)
    {
	    if(isdigit(hostname[0]))
	    {
	        struct in_addr			addr;

	        addr.s_addr = inet_addr(hostname);
	        if ((int) addr.s_addr != -1)
	        {
		        hp = globus_libc_gethostbyaddr_r(
		        (void *) &addr,
		        sizeof(addr),
		        AF_INET,
		        result,
		        buffer,
		        buflen,
		        h_errnop);
	        }
	    }
    }

    return hp;
} /* globus_libc_gethostbyname_r() */


/******************************************************************************
Function: globus_libc_gethostbyaddr_r()

Description: 

Parameters: 

Returns:
******************************************************************************/
struct hostent *
globus_libc_gethostbyaddr_r(char *addr,
			    int length,
			    int type,
			    struct hostent *result,
			    char *buffer,
			    int buflen,
			    int *h_errnop)
{
    struct hostent *hp=GLOBUS_NULL;
#   if defined(GLOBUS_HAVE_GETHOSTBYADDR_R_5)
        struct hostent_data hp_data;
	int rc;
#   endif

#   if defined(GLOBUS_HAVE_GETHOSTBYADDR_R_8)
	int rc;
#   endif
#   if defined(GLOBUS_HAVE_GETHOSTBYADDR_R_7)
	int rc;
#   endif


    globus_libc_lock();

#   if !defined(HAVE_GETHOSTBYADDR_R)
    {
        hp = gethostbyaddr(addr, length, type);
	if(hp != GLOBUS_NULL)
	{
            memcpy(result, hp, sizeof(struct hostent));
            globus_l_libc_copy_hostent_data_to_buffer(result, buffer, buflen);

	    hp = result;
	    if (h_errnop != GLOBUS_NULL)
	    {
		*h_errnop = h_errno;
	    }
	}
	else
	{
	    if (h_errnop != GLOBUS_NULL)
	    {
		*h_errnop = h_errno;
	    }
	}
    }
#   elif defined(GLOBUS_HAVE_GETHOSTBYADDR_R_5)
    {
	rc = gethostbyaddr_r(addr,
			     length,
			     type,
			     result,
			     &hp_data);
        if(rc == 0)
	{
            globus_l_libc_copy_hostent_data_to_buffer(result, buffer, buflen);

	    hp = result;
	    if (h_errnop != GLOBUS_NULL)
	    {
		*h_errnop = h_errno;
	    }
        }
	else
	{
	    hp = GLOBUS_NULL;
	    if (h_errnop != GLOBUS_NULL)
	    {
		*h_errnop = h_errno;
	    }
        }
    }
#   elif defined(GLOBUS_HAVE_GETHOSTBYADDR_R_7)
    {
        hp = gethostbyaddr_r(addr,
			     length,
			     type,
			     result,
			     buffer,
			     buflen,
			     h_errnop);
    }
#   elif defined(GLOBUS_HAVE_GETHOSTBYADDR_R_8)
    {
        rc = gethostbyaddr_r(addr,
			     length,
			     type,
			     result,
			     buffer,
			     buflen,
			     &hp,
			     h_errnop);
    }
#   else
    {
	GLOBUS_HAVE_GETHOSTBYADDR symbol must be defined!!!;
    }
#   endif
    
    globus_libc_unlock();

    return hp;    
} /* globus_libc_gethostbyaddr_r() */

/******************************************************************************
Function: globus_libc_ctime_r()

Description: 

Parameters: 

Returns:
******************************************************************************/
char *
globus_libc_ctime_r(time_t *clock,
		    char *buf,
		    int buflen)
{
    char *tmp_buf;
    
#   if !defined(HAVE_CTIME_R)
    {
	globus_libc_lock();
	tmp_buf = ctime(clock);
	
	if(tmp_buf != GLOBUS_NULL)
	{
	    strncpy(buf,tmp_buf,buflen);
	}
	globus_libc_unlock();

	tmp_buf = buf;
    }
#   endif
	
#   if defined(GLOBUS_HAVE_CTIME_R_2)
    {
	tmp_buf = ctime_r(clock, buf);
    }
#   endif

#   if defined(GLOBUS_HAVE_CTIME_R_3)
    {
	tmp_buf = ctime_r(clock, buf, buflen);
    }
#   endif

    return tmp_buf;
} /* globus_libc_ctime_r() */

/*
 * These functions are not defined on win32
 */
#if !defined(TARGET_ARCH_WIN32)
/******************************************************************************
Function: globus_libc_getpwnam_r()

Description: 

Parameters: 

Returns:
******************************************************************************/
int
globus_libc_getpwnam_r(char *name,
		       struct passwd *pwd,
		       char *buffer,
		       int buflen,
		       struct passwd **result)
{
    int rc=GLOBUS_SUCCESS;
    
#   if !defined(HAVE_GETPWNAM_R)
    {
	struct passwd *tmp_pwd;
	
	globus_libc_lock();
	tmp_pwd = getpwnam(name);
	if(tmp_pwd != GLOBUS_NULL)
	{
	    memcpy(pwd, tmp_pwd, sizeof(struct passwd));

	    globus_l_libc_copy_pwd_data_to_buffer(pwd,
						  buffer,
						  (size_t) buflen);
	    (*result) = pwd;
	}
	else
	{
	    rc = -1;
	}
	globus_libc_unlock();
    }
#   elif defined(GLOBUS_HAVE_GETPWNAM_R_4)
    {
#       if defined(TARGET_ARCH_AIX)
        {
            rc = getpwnam_r(name,
                            pwd,
                            buffer,
			    buflen);
	    if(rc == 0)
	    {
		(*result) = pwd;
	    }
	    else
	    {
		(*result) = GLOBUS_NULL;
	    }
	}
#       else
        {
	    (*result) = getpwnam_r(name,
			           pwd,
			           buffer,
			           buflen);
	    if((*result) == GLOBUS_NULL)
	    {
	        rc = -1;
	    }
        }
#       endif
    }
#   elif defined(GLOBUS_HAVE_GETPWNAM_R_5)
    {
	rc = getpwnam_r(name,
			pwd,
			buffer,
			(size_t) buflen,
			result);
    }
#   endif

    return rc;
} /* globus_libc_getpwnam_r */

/******************************************************************************
Function: globus_libc_getpwuid_r()

Description: 

Parameters: 

Returns:
******************************************************************************/
int
globus_libc_getpwuid_r(uid_t uid,
		       struct passwd *pwd,
		       char *buffer,
		       int buflen,
		       struct passwd **result)
{
    int rc=GLOBUS_SUCCESS;
    
#   if !defined(HAVE_GETPWUID_R)
    {
	struct passwd *tmp_pwd;
	
	globus_libc_lock();
	
	tmp_pwd = getpwuid(uid);
	if(tmp_pwd != GLOBUS_NULL)
	{
	    memcpy(pwd, tmp_pwd, sizeof(struct passwd));

	    globus_l_libc_copy_pwd_data_to_buffer(pwd,
						  buffer,
						  (size_t) buflen);
	    *result = pwd;
	}
	else
	{
	    rc = -1;
	}
	
	globus_libc_unlock();
    }
#   elif defined(GLOBUS_HAVE_GETPWUID_R_4)
    {
#       if defined(TARGET_ARCH_AIX)
        {
            rc = getpwuid_r(uid,
			    pwd,
			    buffer,
			    buflen);
	    if(rc == 0)
	    {
		(*result) = pwd;
	    }
        }
#       else
        {
	    (*result) = getpwuid_r(uid,
			           pwd,
			           buffer,
			           buflen);
	    if((*result) == GLOBUS_NULL)
	    {
	        rc = -1;
	    }
        }
#       endif
    }
#   elif defined(GLOBUS_HAVE_GETPWUID_R_5)
    {
	rc = getpwuid_r(uid,
			pwd,
			buffer,
			(size_t) buflen,
			result);
    }
#   endif

    return rc;
} /* globus_libc_getpwuid_r */

#endif /* TARGET_ARCH_WIN32 */

/******************************************************************************
Function: globus_l_libc_copy_hostent_data_to_buffer()

Description: 

Parameters: 

Returns:
******************************************************************************/
static void
globus_l_libc_copy_hostent_data_to_buffer(struct hostent *h,
					  char *buffer,
					  size_t buflen)
{
    size_t offset=0U;
    char **ptr;
    char **ptr_buffer = (char **) buffer;
    int num_ptrs=0;

   /* list of addresses from name server */
    if(h->h_addr_list != GLOBUS_NULL)
    {
	for(ptr = h->h_addr_list; (*ptr) != GLOBUS_NULL; ptr++)
	{
	    num_ptrs++;
	}
	num_ptrs++;
    }

    if(h->h_aliases != GLOBUS_NULL)
    {
	/* host aliases */
	for(ptr = h->h_aliases; *ptr != GLOBUS_NULL; ptr++)
	{
	    num_ptrs++;
	}
	num_ptrs++;
    }
    
    offset += num_ptrs * sizeof(char *);

    /* official hostname of host */
    if(offset < buflen)
    {
	size_t     cp_len;
	size_t     name_len;
	
	name_len = strlen(h->h_name);
	if(name_len < buflen-offset)
	{
	    cp_len = name_len;
	}
	else
	{
	    cp_len = buflen - offset;
	}
        strncpy(&buffer[offset], h->h_name, cp_len);
	buffer[offset + cp_len] = '\0';
	h->h_name = &buffer[offset];
        offset += cp_len + 1;
    }

   /* list of addresses from name server */
    if(h->h_addr_list != GLOBUS_NULL)
    {
	/* expect all addresses to be inet addresses */
	size_t addrsize = sizeof(struct in_addr);

	ptr = h->h_addr_list;
	h->h_addr_list = ptr_buffer;
	
	for(; (*ptr) != GLOBUS_NULL; ptr++)
	{
	    if(addrsize >= buflen - offset)
	    {
		(*ptr)[offset] = '\0';
	    }
	    else if(offset < buflen)
	    {
		memcpy(&buffer[offset], *ptr, addrsize);
		*ptr_buffer = &buffer[offset];
		ptr_buffer++;
		offset += strlen(*ptr) + addrsize;
	    }
	}
	*ptr_buffer = GLOBUS_NULL;
	ptr_buffer++;
   }

    if(h->h_aliases != GLOBUS_NULL)
    {
	ptr = h->h_aliases;
	h->h_aliases = ptr_buffer;
	
	/* host aliases */
	for(; *ptr != GLOBUS_NULL; ptr++)
	{
	    if(strlen(*ptr) > buflen - offset)
	    {
		(*ptr)[buflen-offset-1] = '\0';
	    }
	    if(offset < buflen)
	    {
		strcpy(&buffer[offset], *ptr);
		*ptr_buffer = &buffer[offset];
		ptr_buffer++;
		offset += strlen(*ptr) + 1;
	    }
	}
	*ptr_buffer = GLOBUS_NULL;
	ptr_buffer++;
    }
} /* globus_l_libc_copy_hostent_data_to_buffer() */


/*
 * globus_libc_system_error_string()
 *
 * Return the string for the current errno.
 */
char *
globus_libc_system_error_string(int the_error)
{
#if !defined HAVE_STRERROR
#if ! defined(TARGET_ARCH_LINUX) & ! defined(TARGET_ARCH_FREEBSD) & \
    ! defined(TARGET_ARCH_DARWIN)
    extern char *sys_errlist[];
#endif
    return ((char *)sys_errlist[the_error]);
#else
    return strerror(the_error);
#endif
} /* globus_libc_system_error_string() */


/*
 *  these functions are not defined on win32
 */
#if !defined(TARGET_ARCH_WIN32)
/******************************************************************************
Function: globus_l_libc_copy_pwd_data_to_buffer()

Description: 

Parameters: 

Returns:
******************************************************************************/
static void
globus_l_libc_copy_pwd_data_to_buffer(struct passwd *pwd,
				      char *buffer,
				      size_t buflen)
{
    size_t offset = 0;

    /* all platforms do not make use of all the fields in the passwd
       struct, so check whether null or not before we copy */

    /* pw_name */
    if (pwd->pw_name)
    {
	if(strlen(pwd->pw_name) > buflen-offset)
	{
	    pwd->pw_name[buflen-offset-1] = '\0';
	}
	if(offset < buflen)
	{
	    strcpy(&buffer[offset], pwd->pw_name);
	    pwd->pw_name = &buffer[offset];
	    offset += strlen(pwd->pw_name) + 1;
	}
    }
    /* pw_passwd */
    if (pwd->pw_passwd)
    {
	if(strlen(pwd->pw_passwd) > buflen-offset)
	{
	    pwd->pw_passwd[buflen-offset-1] = '\0';
	}
	if(offset < buflen)
	{
	    strcpy(&buffer[offset], pwd->pw_passwd);
	    pwd->pw_passwd = &buffer[offset];
	    offset += strlen(pwd->pw_passwd) + 1;
	}
    }

#   if defined(GLOBUS_HAVE_PW_AGE)
    {
	/* pw_age */
	if (pwd->pw_age)
	{
	    if(strlen(pwd->pw_age) > buflen-offset)
	    {
		pwd->pw_age[buflen-offset-1] = '\0';
	    }
	    if(offset < buflen)
	    {
		strcpy(&buffer[offset], pwd->pw_age);
		pwd->pw_age = &buffer[offset];
		offset += strlen(pwd->pw_age) + 1;
	    }
	}
    }
#   endif

#   if defined(GLOBUS_HAVE_PW_COMMENT)
    {
	/* pw_comment */
	if (pwd->pw_comment)
	{
	    if(strlen(pwd->pw_comment) > buflen-offset)
	    {
		pwd->pw_comment[buflen-offset-1] = '\0';
	    }
	    if(offset < buflen)
	    {
		strcpy(&buffer[offset], pwd->pw_comment);
		pwd->pw_comment = &buffer[offset];
		offset += strlen(pwd->pw_comment) + 1;
	    }
	}
    }
#   endif

    /* pw_gecos */
    if (pwd->pw_gecos)
    {
	if(strlen(pwd->pw_gecos) > buflen-offset)
	{
	    pwd->pw_gecos[buflen-offset-1] = '\0';
	}
	if(offset < buflen)
	{
	    strcpy(&buffer[offset], pwd->pw_gecos);
	    pwd->pw_gecos = &buffer[offset];
	    offset += strlen(pwd->pw_gecos) + 1;
	}
    }
    /* pw_dir */
    if (pwd->pw_dir)
    {
	if(strlen(pwd->pw_dir) > buflen-offset)
	{
	    pwd->pw_dir[buflen-offset-1] = '\0';
	}
	if(offset < buflen)
	{
	    strcpy(&buffer[offset], pwd->pw_dir);
	    pwd->pw_dir = &buffer[offset];
	    offset += strlen(pwd->pw_dir) + 1;
	}
    }
    /* pw_shell */
    if (pwd->pw_shell)
    {
	if(strlen(pwd->pw_shell) > buflen-offset)
	{
	    pwd->pw_shell[buflen-offset-1] = '\0';
	}
	if(offset < buflen)
	{
	    strcpy(&buffer[offset], pwd->pw_shell);
	    pwd->pw_shell = &buffer[offset];
	    offset += strlen(pwd->pw_shell) + 1;
	}
    }
} /* globus_l_libc_copy_pwd_data_to_buffer() */




/******************************************************************************
Function: globus_libc_gethomedir()

Description: wrapper around globus_libc_getpwuid_r(getuid()).

Parameters: 

Returns:
******************************************************************************/
int
globus_libc_gethomedir(char *result, int bufsize)
{
    static globus_mutex_t   gethomedir_mutex;
    static int              initialized = GLOBUS_FALSE;
    static struct passwd    pw;
    static char             homedir[MAXPATHLEN];
    static int              homedir_len = 0;
    static char             buf[1024];
    int                     rc;
    int                     len;
    char *                  p;
    struct passwd *         pwres;
    
    globus_libc_lock();
    if (!initialized)
    {
	globus_mutex_init(&gethomedir_mutex,
			  (globus_mutexattr_t *) GLOBUS_NULL);
	initialized = GLOBUS_TRUE;
    }
    globus_libc_unlock();    
    
    globus_mutex_lock(&gethomedir_mutex);	
    {    
	rc = 0;

	if (homedir_len == 0)
	{
	    p = globus_libc_getenv("HOME");
	    if (!p || strlen(p)==0)
	    {
		p = GLOBUS_NULL;
		rc = globus_libc_getpwuid_r(geteuid(),
					    &pw,
					    buf,
					    1024,
					    &pwres);
		
		if (!rc && pwres && pwres->pw_dir)
		    p = pwres->pw_dir;
	    }

	    if (!rc && p)
	    {
		len = strlen(p);
		if (len+1 < MAXPATHLEN)
		{
		    memcpy(homedir, p, len);
		    homedir[len] = '\0';
		    homedir_len = strlen(homedir);
		}
		else
		    rc = -1;
	    }
	}
	
	if (homedir_len > bufsize)
	    rc = -1;

	if (!rc)
	{
	    memcpy(result, homedir, homedir_len);
	    result[homedir_len] = '\0';
	}
    }
    globus_mutex_unlock(&gethomedir_mutex);	

    return rc;
} /* globus_libc_gethomedir() */

#endif /* TARGET_ARCH_WIN32 */

char *
globus_libc_strdup(const char * string)
{
    static globus_mutex_t   strdup_mutex;
    static int              initialized = GLOBUS_FALSE;
    char *                  ns;
    int                     i, l;

    globus_libc_lock();
    if (!initialized)
    {
	globus_mutex_init(&strdup_mutex, (globus_mutexattr_t *) GLOBUS_NULL);
	initialized = GLOBUS_TRUE;
    }
    globus_libc_unlock();

    globus_mutex_lock(&strdup_mutex);

    ns = GLOBUS_NULL;

    if (string)
    {
	l = strlen (string);

	ns = globus_malloc (sizeof(char *) * (l + 1));

	if (ns)
	{
	    for (i=0; i<l; i++)
		ns[i] = string[i];

	    ns[l] = '\0';
	}
    }
    
    globus_mutex_unlock(&strdup_mutex);
    return ns;
}
/* globus_libc_strdup */


/*
 * not defined on win32
 */
#if !defined(TARGET_ARCH_WIN32)

/******************************************************************************
Function: globus_libc_lseek()

Description: 

Parameters: 

Returns:
******************************************************************************/
#undef globus_libc_lseek

int
globus_libc_lseek(int fd,
		  globus_off_t offset,
		  int whence)
{
    int rc;
    int save_errno;
    globus_libc_lock();
    rc = lseek(fd, offset, whence);
    save_errno = errno;
    /* Should convert EWOULDBLOCK to EINTR */
    globus_libc_unlock();
    errno = save_errno;
    return(rc);
} /* globus_libc_lseek() */
 


#undef globus_libc_opendir
extern DIR *
globus_libc_opendir(char *filename)
{
    DIR *dirp;
    int save_errno;
    
    globus_libc_lock();

    dirp = opendir(filename);
    save_errno=errno;

    globus_libc_unlock();

    errno=save_errno;
    return dirp;
}

#if defined(HAVE_TELLDIR)
#undef globus_libc_telldir
extern long
globus_libc_telldir(DIR *dirp)
{
    long pos=-1;
    int save_errno;
    

    if(dirp != GLOBUS_NULL)
    {
	globus_libc_lock();

	pos = telldir(dirp);
	save_errno=errno;

	globus_libc_unlock();
	errno = save_errno;

	return pos;
    }
    else
    {
	return pos;
    }
}
#endif /* defined(HAVE_TELLDIR) */

#if defined(HAVE_SEEKDIR)
#undef globus_libc_seekdir
extern void
globus_libc_seekdir(DIR *dirp,
		    long loc)
{
    int save_errno;

    if(dirp != GLOBUS_NULL)
    {
	globus_libc_lock();

	seekdir(dirp, loc);

	save_errno = errno;
	
	globus_libc_unlock();
	errno = save_errno;
	return;
    }
}
#endif /* defined(HAVE_SEEKDIR) */

#undef globus_libc_rewinddir
extern void
globus_libc_rewinddir(DIR *dirp)
{
    int save_errno;

    if(dirp != GLOBUS_NULL)
    {
	globus_libc_lock();

	rewinddir(dirp);

	save_errno = errno;
	
	globus_libc_unlock();
	errno = save_errno;
	return;
    }
}

#undef globus_libc_closedir
extern void
globus_libc_closedir(DIR *dirp)
{
        int save_errno;

    if(dirp != GLOBUS_NULL)
    {
	globus_libc_lock();

	closedir(dirp);

	save_errno = errno;
	
	globus_libc_unlock();
	errno = save_errno;
	return;
    }
}

#undef globus_libc_readdir_r
extern int
globus_libc_readdir_r(DIR *dirp,
		      struct dirent **result)
{
#if !defined(HAVE_READDIR_R)
    {
	struct dirent *tmpdir, *entry;
	int save_errno;

	entry = (struct dirent *) globus_malloc(sizeof(struct dirent)
						+ MAXPATHLEN
						+ 1);
	globus_libc_lock();

	tmpdir = readdir(dirp);
	save_errno = errno;

	if(tmpdir == GLOBUS_NULL)
	{
	    *result = GLOBUS_NULL;

	    globus_libc_unlock();

            globus_free(entry);

	    errno = save_errno;

	    return -1;
	}

	/* copy returned buffer into data structure */
	entry->d_ino = tmpdir->d_ino;
#       if defined(GLOBUS_HAVE_DIRENT_OFF)
	{
	    entry->d_off = tmpdir->d_off;
	}
#       endif
#       if defined(GLOBUS_HAVE_DIRENT_OFFSET)
	{
	    entry->d_offset = tmpdir->d_offset;
	}
#       endif
#       if defined(GLOBUS_HAVE_DIRENT_TYPE)
	{
	    entry->d_type = tmpdir->d_type;
	}
#       endif
#	if defined(GLOBUS_HAVE_DIRENT_RECLEN)
	{
	    entry->d_reclen = tmpdir->d_reclen;
	}
#       endif	
	strcpy(&entry->d_name[0], &tmpdir->d_name[0]);

#       if defined(HAVE_DIRENT_NAMELEN)
	{
	    entry->d_namlen = tmpdir->d_namlen;
	}
#       endif
	
	*result = entry;
	globus_libc_unlock();
	errno = save_errno;
	return 0;
    }
#   else
    {
	int errno;

#       if defined(GLOBUS_HAVE_READDIR_R_3)
	{
	    int rc = 0;
	    struct dirent *entry = globus_malloc(sizeof(struct dirent)
						 + MAXPATHLEN
						 + 1);
	    
	    rc = readdir_r(dirp, entry, result);

            if(rc != 0 || *result == NULL)
            { 
		globus_free(entry);
		*result = GLOBUS_NULL;
            }
            return rc;
	}
#       elif defined(GLOBUS_HAVE_READDIR_R_2)
	{
	    struct dirent *entry = globus_malloc(sizeof(struct dirent)
						 + MAXPATHLEN
						 + 1);
	    int rc=0;

#           if defined(TARGET_ARCH_SOLARIS)
	    {
		*result = readdir_r(dirp, entry);
		if(*result == GLOBUS_NULL)
		{
		    rc = -1;
		}
	    }
#           elif defined(TARGET_ARCH_HPUX)
	    {
		rc = readdir_r(dirp, entry);
		*result = entry;
	    }
#           endif

	    if(rc != GLOBUS_SUCCESS)
	    {
		globus_free(entry);
		*result = GLOBUS_NULL;
		return rc;
	    }
	    else
	    {
		return 0;
	    }
	}
#       endif
    }
#   endif
}

#endif /* TARGET_ARCH_WIN32 */

int
globus_libc_vprintf_length(const char * fmt, va_list ap)
{
    static FILE *			devnull = GLOBUS_NULL;
    int save_errno;

    globus_libc_lock();
    if(devnull == GLOBUS_NULL)
    {
#ifndef TARGET_ARCH_WIN32
	devnull = fopen("/dev/null", "w");

        if(devnull == GLOBUS_NULL)
        {
            save_errno = errno;
            globus_libc_unlock();
            errno = save_errno;
            return -1;
        }
        fcntl(fileno(devnull), F_SETFD, FD_CLOEXEC);
#else
	devnull = fopen("NUL", "w");
        if(devnull == GLOBUS_NULL)
        {
            save_errno = errno;
            globus_libc_unlock();
            errno = save_errno;
            return -1;
        }
#endif
    }
    globus_libc_unlock();

    return globus_libc_vfprintf(devnull, fmt, ap);
}

int
globus_libc_printf_length(const char * fmt, ...)
{
    int                                 length;
    va_list                             ap;

    va_start(ap,fmt);

    length = globus_libc_vprintf_length(fmt,ap);
    
    va_end(ap);

    return length;
}


char *
globus_common_create_string(
    const char *                        format,
    ...)
{
    va_list                             ap;
    char *                              new_string;

    va_start(ap, format);

    new_string = globus_common_v_create_string(format, ap);

    va_end(ap);

    return new_string;
}

char *
globus_common_create_nstring(
    int                                 length,
    const char *                        format,
    ...)
{
    va_list                             ap;
    char *                              new_string;

    va_start(ap, format);

    new_string = globus_common_v_create_nstring(length, format, ap);

    va_end(ap);

    return new_string;
}

char *
globus_common_v_create_string(
    const char *                        format,
    va_list                             ap)
{
    int                                 len;
    char *                              new_string = NULL;
    va_list                             ap_copy;

    globus_libc_va_copy(ap_copy,ap);
    
    len = globus_libc_vprintf_length(format,ap_copy);

    va_end(ap_copy);

    len++;

    if((new_string = malloc(len)) == NULL)
    {
        return NULL;
    }
    
    globus_libc_vsnprintf(new_string,
                          len,
                          format,
                          ap);
    
    return new_string;
}

char *
globus_common_v_create_nstring(
    int                                 length,
    const char *                        format,
    va_list                             ap)
{
    char *                              new_string = NULL;

    if((new_string = malloc(length + 1)) == NULL)
    {
        return NULL;
    }

    globus_libc_vsnprintf(new_string, length + 1, format, ap);

    return new_string;
}


#ifdef TARGET_ARCH_CRAYT3E
/* for alloca on T3E */
#if !defined (__GNUC__) || __GNUC__ < 2
#if defined (CRAY) && defined (CRAY_STACKSEG_END)
    static long globus_l_libc_i00afunc ();
#   define ADDRESS_FUNCTION(arg) (char *) globus_l_libc_i00afunc (&(arg))
#else
#   define ADDRESS_FUNCTION(arg) &(arg)
#endif

/* Define STACK_DIRECTION if you know the direction of stack
   growth for your system; otherwise it will be automatically
   deduced at run-time.

   STACK_DIRECTION > 0 => grows toward higher addresses
   STACK_DIRECTION < 0 => grows toward lower addresses
   STACK_DIRECTION = 0 => direction of growth unknown  */

#ifndef STACK_DIRECTION
#define	STACK_DIRECTION	0	/* Direction unknown.  */
#endif

#if STACK_DIRECTION != 0

#define	STACK_DIR	STACK_DIRECTION	/* Known at compile-time.  */

#else /* STACK_DIRECTION == 0; need run-time code.  */

static int stack_dir=0;		/* 1 or -1 once known.  */
#define	STACK_DIR	stack_dir

static void
find_stack_direction ()
{
  static char *addr = GLOBUS_NULL;	/* Address of first `dummy', once known.  */
  auto char dummy;		/* To get stack address.  */

  if (addr == GLOBUS_NULL)
    {				/* Initial entry.  */
      addr = ADDRESS_FUNCTION (dummy);

      find_stack_direction ();	/* Recurse once.  */
    }
  else
    {
      /* Second entry.  */
      if (ADDRESS_FUNCTION (dummy) > addr)
	stack_dir = 1;		/* Stack grew upward.  */
      else
	stack_dir = -1;		/* Stack grew downward.  */
    }
}

#endif /* STACK_DIRECTION == 0 */
/* An "alloca header" is used to:
   (a) chain together all alloca'ed blocks;
   (b) keep track of stack depth.

   It is very important that sizeof(header) agree with malloc
   alignment chunk size.  The following default should work okay.  */

#ifndef	ALIGN_SIZE
#define	ALIGN_SIZE	sizeof(double)
#endif

typedef union hdr
{
  char align[ALIGN_SIZE];	/* To force sizeof(header).  */
  struct
    {
      union hdr *next;		/* For chaining headers.  */
      char *deep;		/* For stack depth measure.  */
    } h;
} header;

static header *last_alloca_header = GLOBUS_NULL;	/* -> last alloca header.  */

/* Return a pointer to at least SIZE bytes of storage,
   which will be automatically reclaimed upon exit from
   the procedure that called alloca.  Originally, this space
   was supposed to be taken from the current stack frame of the
   caller, but that method cannot be made to work for some
   implementations of C, for example under Gould's UTX/32.  */

void *
alloca (size)
     unsigned size;
{
  auto char probe;		/* Probes stack depth: */
  register char *depth = ADDRESS_FUNCTION (probe);

#if STACK_DIRECTION == 0
  if (STACK_DIR == 0)		/* Unknown growth direction.  */
    find_stack_direction ();
#endif

  /* Reclaim garbage, defined as all alloca'd storage that
     was allocated from deeper in the stack than currently.  */

  {
    register header *hp;	/* Traverses linked list.  */

#ifdef emacs
    BLOCK_INPUT;
#endif

    for (hp = last_alloca_header; hp != GLOBUS_NULL;)
      if ((STACK_DIR > 0 && hp->h.deep > depth)
	  || (STACK_DIR < 0 && hp->h.deep < depth))
	{
	  register header *np = hp->h.next;

	  free ((void *) hp);	/* Collect garbage.  */

	  hp = np;		/* -> next header.  */
	}
      else
	break;			/* Rest are not deeper.  */

    last_alloca_header = hp;	/* -> last valid storage.  */

#ifdef emacs
    UNBLOCK_INPUT;
#endif
  }

  if (size == 0)
    return GLOBUS_NULL;		/* No allocation required.  */

  /* Allocate combined header + user data storage.  */

  {
    register void * new = malloc (sizeof (header) + size);
    /* Address of header.  */

    if (new == 0)
      abort();

    ((header *) new)->h.next = last_alloca_header;
    ((header *) new)->h.deep = depth;

    last_alloca_header = (header *) new;

    /* User storage begins just after header.  */

    return (void *) ((char *) new + sizeof (header));
  }
}

#if defined (CRAY) && defined (CRAY_STACKSEG_END)
#ifndef CRAY_STACK
#define CRAY_STACK
#ifndef CRAY2
/* Stack structures for CRAY-1, CRAY X-MP, and CRAY Y-MP */
struct stack_control_header
  {
    long shgrow:32;		/* Number of times stack has grown.  */
    long shaseg:32;		/* Size of increments to stack.  */
    long shhwm:32;		/* High water mark of stack.  */
    long shsize:32;		/* Current size of stack (all segments).  */
  };

/* The stack segment linkage control information occurs at
   the high-address end of a stack segment.  (The stack
   grows from low addresses to high addresses.)  The initial
   part of the stack segment linkage control information is
   0200 (octal) words.  This provides for register storage
   for the routine which overflows the stack.  */

struct stack_segment_linkage
  {
    long ss[0200];		/* 0200 overflow words.  */
    long sssize:32;		/* Number of words in this segment.  */
    long ssbase:32;		/* Offset to stack base.  */
    long:32;
    long sspseg:32;		/* Offset to linkage control of previous
				   segment of stack.  */
    long:32;
    long sstcpt:32;		/* Pointer to task common address block.  */
    long sscsnm;		/* Private control structure number for
				   microtasking.  */
    long ssusr1;		/* Reserved for user.  */
    long ssusr2;		/* Reserved for user.  */
    long sstpid;		/* Process ID for pid based multi-tasking.  */
    long ssgvup;		/* Pointer to multitasking thread giveup.  */
    long sscray[7];		/* Reserved for Cray Research.  */
    long ssa0;
    long ssa1;
    long ssa2;
    long ssa3;
    long ssa4;
    long ssa5;
    long ssa6;
    long ssa7;
    long sss0;
    long sss1;
    long sss2;
    long sss3;
    long sss4;
    long sss5;
    long sss6;
    long sss7;
  };

#else /* CRAY2 */
/* The following structure defines the vector of words
   returned by the STKSTAT library routine.  */
struct stk_stat
  {
    long now;			/* Current total stack size.  */
    long maxc;			/* Amount of contiguous space which would
				   be required to satisfy the maximum
				   stack demand to date.  */
    long high_water;		/* Stack high-water mark.  */
    long overflows;		/* Number of stack overflow ($STKOFEN) calls.  */
    long hits;			/* Number of internal buffer hits.  */
    long extends;		/* Number of block extensions.  */
    long stko_mallocs;		/* Block allocations by $STKOFEN.  */
    long underflows;		/* Number of stack underflow calls ($STKRETN).  */
    long stko_free;		/* Number of deallocations by $STKRETN.  */
    long stkm_free;		/* Number of deallocations by $STKMRET.  */
    long segments;		/* Current number of stack segments.  */
    long maxs;			/* Maximum number of stack segments so far.  */
    long pad_size;		/* Stack pad size.  */
    long current_address;	/* Current stack segment address.  */
    long current_size;		/* Current stack segment size.  This
				   number is actually corrupted by STKSTAT to
				   include the fifteen word trailer area.  */
    long initial_address;	/* Address of initial segment.  */
    long initial_size;		/* Size of initial segment.  */
  };

/* The following structure describes the data structure which trails
   any stack segment.  I think that the description in 'asdef' is
   out of date.  I only describe the parts that I am sure about.  */

struct stk_trailer
  {
    long this_address;		/* Address of this block.  */
    long this_size;		/* Size of this block (does not include
				   this trailer).  */
    long unknown2;
    long unknown3;
    long link;			/* Address of trailer block of previous
				   segment.  */
    long unknown5;
    long unknown6;
    long unknown7;
    long unknown8;
    long unknown9;
    long unknown10;
    long unknown11;
    long unknown12;
    long unknown13;
    long unknown14;
  };

#endif /* CRAY2 */
#endif /* not CRAY_STACK */

#ifdef CRAY2
/* Determine a "stack measure" for an arbitrary ADDRESS.
   I doubt that "lint" will like this much.  */

static long
globus_l_libc_i00afunc (long *address)
{
  struct stk_stat status;
  struct stk_trailer *trailer;
  long *block, size;
  long result = 0;

  /* We want to iterate through all of the segments.  The first
     step is to get the stack status structure.  We could do this
     more quickly and more directly, perhaps, by referencing the
     $LM00 common block, but I know that this works.  */

  STKSTAT (&status);

  /* Set up the iteration.  */

  trailer = (struct stk_trailer *) (status.current_address
				    + status.current_size
				    - 15);

  /* There must be at least one stack segment.  Therefore it is
     a fatal error if "trailer" is null.  */

  if (trailer == 0)
    abort ();

  /* Discard segments that do not contain our argument address.  */

  while (trailer != 0)
    {
      block = (long *) trailer->this_address;
      size = trailer->this_size;
      if (block == 0 || size == 0)
	abort ();
      trailer = (struct stk_trailer *) trailer->link;
      if ((block <= address) && (address < (block + size)))
	break;
    }

  /* Set the result to the offset in this segment and add the sizes
     of all predecessor segments.  */

  result = address - block;

  if (trailer == 0)
    {
      return result;
    }

  do
    {
      if (trailer->this_size <= 0)
	abort ();
      result += trailer->this_size;
      trailer = (struct stk_trailer *) trailer->link;
    }
  while (trailer != 0);

  /* We are done.  Note that if you present a bogus address (one
     not in any segment), you will get a different number back, formed
     from subtracting the address of the first block.  This is probably
     not what you want.  */

  return (result);
}

#else /* not CRAY2 */
/* Stack address function for a CRAY-1, CRAY X-MP, or CRAY Y-MP.
   Determine the number of the cell within the stack,
   given the address of the cell.  The purpose of this
   routine is to linearize, in some sense, stack addresses
   for alloca.  */

static long
globus_l_libc_i00afunc (long address)
{
  long stkl = 0;

  long size, pseg, this_segment, stack;
  long result = 0;

  struct stack_segment_linkage *ssptr;

  /* Register B67 contains the address of the end of the
     current stack segment.  If you (as a subprogram) store
     your registers on the stack and find that you are past
     the contents of B67, you have overflowed the segment.

     B67 also points to the stack segment linkage control
     area, which is what we are really interested in.  */

  stkl = CRAY_STACKSEG_END ();
  ssptr = (struct stack_segment_linkage *) stkl;

  /* If one subtracts 'size' from the end of the segment,
     one has the address of the first word of the segment.

     If this is not the first segment, 'pseg' will be
     nonzero.  */

  pseg = ssptr->sspseg;
  size = ssptr->sssize;

  this_segment = stkl - size;

  /* It is possible that calling this routine itself caused
     a stack overflow.  Discard stack segments which do not
     contain the target address.  */

  while (!(this_segment <= address && address <= stkl))
    {
#ifdef DEBUG_I00AFUNC
      fprintf (stderr, "%011o %011o %011o\n", this_segment, address, stkl);
#endif
      if (pseg == 0)
	break;
      stkl = stkl - pseg;
      ssptr = (struct stack_segment_linkage *) stkl;
      size = ssptr->sssize;
      pseg = ssptr->sspseg;
      this_segment = stkl - size;
    }

  result = address - this_segment;

  /* If you subtract pseg from the current end of the stack,
     you get the address of the previous stack segment's end.
     This seems a little convoluted to me, but I'll bet you save
     a cycle somewhere.  */

  while (pseg != 0)
    {
#ifdef DEBUG_I00AFUNC
      fprintf (stderr, "%011o %011o\n", pseg, size);
#endif
      stkl = stkl - pseg;
      ssptr = (struct stack_segment_linkage *) stkl;
      size = ssptr->sssize;
      pseg = ssptr->sspseg;
      result += size;
    }
  return (result);
}

#endif /* not CRAY2 */
#endif /* CRAY */
#endif /* !defined (__GNUC__) || __GNUC__ < 2 */
#endif /* TARGET_ARCH_CRAYT3E */



