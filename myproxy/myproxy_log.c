/*
 * myproxy_log.c
 *
 * See myproxy_log.h for documentation.
 */

#include "myproxy_log.h"

#include "verror.h"
#include "string_funcs.h"

#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

/**********************************************************************
 *
 * Internal Variables
 *
 */

struct myproxy_log_context 
{
    int syslog_facility;
    char *syslog_name;
    int debug_level;
    FILE *log_stream;
};

static struct myproxy_log_context my_context = 
{
    0,
    NULL,
    0,
    NULL
};



/**********************************************************************
 *
 * Internal Functions
 *
 */

/*
 * do_log()
 *
 * Do the actual logging of the given string.
 */
static void
do_log(const char *string, int level)
{
    /*
     * We always want to use '"%s", string' when logging in case
     * string itself contains a '%s".
     */
    if (my_context.syslog_facility != 0) 
    {
	/* syslog() seems to automatically prepend process name */
	syslog(my_context.syslog_facility|level, "<%d> %s",
	       getpid(), string);
    }
    
    if (my_context.log_stream != NULL)
    {
	fprintf(my_context.log_stream, "%s\n", string);
    }
	       
    return;
}



/**********************************************************************
 *
 * API Functions
 *
 */

void
myproxy_log_use_syslog(const int facility,
		       const char *name)
{
    my_context.syslog_facility = facility;
    my_context.syslog_name = (name == NULL) ? NULL : strdup(name);
}

void
myproxy_log_use_stream(FILE *stream)
{
    my_context.log_stream = stream;
}


void
myproxy_log(int dbg_level, int current_debug_level, const char *format, ...)
{
	// this dbg_level is only for myproxy_log. my_context.debug_level shouldbe 1 in order for this to log

    char *string = NULL;
    va_list ap;

    if (current_debug_level < dbg_level)
	return;

    va_start(ap, format);
    
    string = my_vsnprintf(format, ap);
    
    va_end(ap);
    
    if (string == NULL)
    {
	/* Punt */
	goto error;
    }
    
    do_log(string, LOG_NOTICE);
    
  error:
    if (string != NULL)
    {
	free(string);
    }
    
    return;
}

void
myproxy_log_verror()
{
    char *string;
    
    string = verror_get_string();
    
    if (string != NULL)
    {
	do_log(verror_get_string(), LOG_ERR);
    }

    if (verror_get_errno() != 0)
    {
	do_log(verror_strerror(), LOG_ERR);
    }

    verror_clear();
    
    return;
}

void
myproxy_log_perror(const char *format, ...)
{
    char *string = NULL;
    va_list ap;
    
    va_start(ap, format);
    
    string = my_vsnprintf(format, ap);
    
    va_end(ap);
    
    if (string == NULL)
    {
	/* Punt */
	goto error;
    }
    
    do_log(string, LOG_ERR);
    do_log(strerror(errno), LOG_ERR);
    
  error:
    if (string != NULL)
    {
	free(string);
    }
    
    return;
}

void
myproxy_log_close()
{
    my_context.syslog_facility = 0;
    
    if (my_context.syslog_name != NULL)
    {
	free(my_context.syslog_name);
	my_context.syslog_name = NULL;
    }
    
    my_context.debug_level = 0;
    
    my_context.log_stream = NULL;
}


int
myproxy_debug_set_level(int level)
{
    int old_level = my_context.debug_level;

    my_context.debug_level = level;

    return old_level;
}


void
myproxy_debug(const char *format, ...)
{
    char *string = NULL;
    va_list ap;

    if (my_context.debug_level == 0)
    {
	return;
    }
	
    va_start(ap, format);
    
    string = my_vsnprintf(format, ap);
    
    va_end(ap);
    
    if (string == NULL)
    {
	/* Punt */
	goto error;
    }
    
    do_log(string, LOG_NOTICE);
    
  error:
    if (string != NULL)
    {
	free(string);
    }
    
    return;
}
