#include "config.h"
#include "globus_common.h"
#include "globus_error_string.h"
#include <string.h>

/**
 * Allocate and initialize an error of type GLOBUS_ERROR_TYPE_STRING
 *
 * @param base_source
 * @param base_cause
 * @param fmt
 * @param ...
 */ 
extern
globus_object_t *
globus_error_construct_string(
    globus_module_descriptor_t *	base_source,
    globus_object_t *			base_cause,
    const char *			fmt,
    ...)
{
    globus_object_t *			error;
    globus_object_t *			newerror;
    va_list				ap;

    va_start(ap, fmt);

    newerror = globus_object_construct(GLOBUS_ERROR_TYPE_STRING);
    error = globus_error_initialize_string(
	    newerror,
	    base_source,
	    base_cause,
	    fmt,
	    ap);

    va_end(ap);
    if (error == NULL)
    {
	globus_object_free(newerror);
    }

    return error;
}
/* globus_error_construct_string() */

/* initialize and return an error of type
 * GLOBUS_ERROR_TYPE_STRING
 */
extern globus_object_t *
globus_error_initialize_string(
    globus_object_t *			error,
    globus_module_descriptor_t *	base_source,
    globus_object_t *			base_cause,
    const char *			fmt,
    va_list				ap)
{
    char * instance_data;
    static FILE * f = NULL;
    int len;

    globus_libc_lock();
    if(f == NULL)
    {
	f = fopen("/dev/null", "w");
    }

    len = vfprintf(f, fmt, ap) + 1;

    instance_data = malloc(len);

    vsprintf(instance_data, fmt, ap);

    globus_libc_unlock();

    globus_object_set_local_instance_data(error, instance_data);

    return globus_error_initialize_base(error,
					base_source,
					base_cause);
}

static
void
globus_l_error_string_copy(
    void *				src,
    void **				dst)
{
    if(src == NULL || dst == NULL) return;
    (*dst) = (void *) globus_libc_strdup((char *)src);
}

static
void
globus_l_error_string_free(
    void *				data)
{
    globus_libc_free(data);
}

static
char *
globus_l_error_string_printable(
    globus_object_t *			error)
{
    return globus_libc_strdup(globus_object_get_local_instance_data(error));
}

const globus_object_type_t GLOBUS_ERROR_TYPE_STRING_DEFINITION
= globus_error_type_static_initializer (
	GLOBUS_ERROR_TYPE_BASE,
	globus_l_error_string_copy,
	globus_l_error_string_free,
	globus_l_error_string_printable);

