#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file globus_error_openssl.c
 * @author Sam Lang
 * 
 * $RCSfile$
 * $Revision$
 * $Date$
 *
 */
#endif

#include "globus_i_error_openssl.h"
#include "globus_common.h"
#include "version.h"
#include "openssl/err.h"
#include "openssl/ssl.h"

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL

int                         globus_i_gsi_openssl_error_debug_level = 0;
FILE *                      globus_i_gsi_openssl_error_debug_fstream = NULL;

static int globus_l_gsi_openssl_error_activate(void);
static int globus_l_gsi_openssl_error_deactivate(void);

/**
 * Module descriptor static initializer.
 */
globus_module_descriptor_t              globus_i_gsi_openssl_error_module =
{
    "globus_gsi_openssl_error",
    globus_l_gsi_openssl_error_activate,
    globus_l_gsi_openssl_error_deactivate,
    GLOBUS_NULL,
    GLOBUS_NULL,
    &local_version
};

/**
 * Module activation
 */
static
int
globus_l_gsi_openssl_error_activate(void)
{
    globus_bool_t                       result = (int) GLOBUS_SUCCESS;
    char *                              tmp_string;
    static char *                       _function_name_ =
        "globus_l_gsi_openssl_error_activate";

    tmp_string = globus_module_getenv("GLOBUS_GIS_OPENSSL_ERROR_DEBUG_LEVEL");
    if(tmp_string != GLOBUS_NULL)
    {
        globus_i_gsi_openssl_error_debug_level = atoi(tmp_string);

        if(globus_i_gsi_openssl_error_debug_level < 0)
        {
            globus_i_gsi_openssl_error_debug_level = 0;
        }
    }

    tmp_string = globus_module_getenv("GLOBUS_GSI_OPENSSL_ERROR_DEBUG_FILE");
    if(tmp_string != GLOBUS_NULL)
    {
        globus_i_gsi_openssl_error_debug_fstream = fopen(tmp_string, "w");
        if(globus_i_gsi_openssl_error_debug_fstream == NULL)
        {
            result = (int) GLOBUS_FAILURE;
            goto exit;
        }
    }
    else
    {
        globus_i_gsi_openssl_error_debug_fstream = stderr;
    }

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_ENTER;

    /* initializes arrays for the error handling library containing
     * function names, library names, and reasons for errors
     */

    SSL_load_error_strings();

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_EXIT;

 exit:
    return result;
}

/**
 * Module deactivation
 */
static
int
globus_l_gsi_openssl_error_deactivate(void)
{
    static char *                       _function_name_ =
        "globus_l_gsi_openssl_error_deactivate";

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_ENTER;

    /* frees all error strings loaded into the static error table
     * that maps library/function/reason codes to text strings
     */
    ERR_free_strings();

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_EXIT;

    if(globus_i_gsi_openssl_error_debug_fstream != stderr)
    {
        fclose(globus_i_gsi_openssl_error_debug_fstream);
    }

    return GLOBUS_SUCCESS;
}

#endif

/**
 * @name Get Error Code
 * @ingroup globus_openssl_error_object
 */
/* @{ */
/**
 * Get the openssl error code which represents the openssl error
 * from the openssl error handle
 *
 * @param handle
 *        The openssl error handle
 * @return
 *        The error code
 */
unsigned long
globus_openssl_error_handle_get_error_code(
    globus_openssl_error_handle_t       handle)
{
    unsigned long                        error_code;
    static char *                       _function_name_ =
        "globus_openssl_error_handle_get_error_code";

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_ENTER;

    if(handle != NULL)
    {
        error_code = handle->error_code;
        goto done;
    }
    
    error_code = 0;

 done:

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_EXIT;
    return error_code;
}
/* @} */

/**
 * @name Get Error Data
 * @ingroup globus_openssl_error_object
 */
/* @{ */
/**
 * Get the openssl error data which contains additional data about the error
 * from the openssl error handle 
 *
 * @param handle
 *        The openssl error handle
 * @return
 *        The error data
 */
const char *
globus_openssl_error_handle_get_data(
    globus_openssl_error_handle_t       handle)
{
    const char *                        data;
    static char *                       _function_name_ =
        "globus_openssl_error_handle_get_error_data";

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_ENTER;

    if(handle != NULL)
    {
        data = handle->data;
        goto done;
    }
    
    data = NULL;

 done:

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_EXIT;
    return data;
}
/* @} */
/**
 * @name Get Error Data Flags
 * @ingroup globus_openssl_error_object
 */
/* @{ */
/**
 * Get the openssl error data flags from the openssl error handle
 *
 * @param handle
 *        The openssl error handle
 * @return
 *        The error data flags
 */
int
globus_openssl_error_handle_get_data_flags(
    globus_openssl_error_handle_t       handle)
{
    int                                 flags;
    static char *                       _function_name_ =
        "globus_openssl_error_handle_get_data_flags";

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_ENTER;

    if(handle != NULL)
    {
        flags = handle->flags;
        goto done;
    }
    
    flags = 0;

 done:

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_EXIT;
    return flags;
}
/* @} */

/**
 * @name Get Filename
 * @ingroup globus_openssl_error_object
 */
/* @{ */
/**
 * Get the filename where the openssl error occurred
 *  from the openssl error handle
 *
 * @param handle
 *        The openssl error handle
 * @return
 *        The filename
 */
const char *
globus_openssl_error_handle_get_filename(
    globus_openssl_error_handle_t       handle)
{
    const char *                        filename;
    static char *                       _function_name_ =
        "globus_openssl_error_handle_get_filename";

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_ENTER;

    if(handle != NULL)
    {
        filename = handle->filename;
        goto done;
    }
    
    filename = NULL;

 done:

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_EXIT;
    return filename;
}
/* @} */

/**
 * @name Get Linenumber
 * @ingroup globus_openssl_error_object
 */
/* @{ */
/**
 * Get the linenumber on which the openssl error occurred
 * from the openssl error handle
 *
 * @param handle
 *        The openssl error handle
 * @return
 *        The linenumber
 */
int
globus_openssl_error_handle_get_linenumber(
    globus_openssl_error_handle_t       handle)
{
    int                                 linenumber;
    static char *                       _function_name_ =
        "globus_openssl_error_handle_get_linenumber";

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_ENTER;

    if(handle != NULL)
    {
        linenumber = handle->linenumber;
        goto done;
    }
    
    linenumber = -1;

 done:

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_EXIT;
    return linenumber;
}
/* @} */

/**
 * @name Get Library
 * @ingroup globus_openssl_error_object
 */
/* @{ */
/**
 * Get the library name where the openssl error occurred in
 * from the openssl error handle
 *
 * @param handle
 *        The openssl error handle
 * @return
 *        The library name
 */
const char *
globus_openssl_error_handle_get_library(
    globus_openssl_error_handle_t       handle)
{
    const char *                        library;
    static char *                       _function_name_ =
        "globus_openssl_error_handle_get_library";

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_ENTER;

    if(handle != NULL)
    {
        library = ERR_lib_error_string(handle->error_code);
        goto done;
    }
    
    library = NULL;

 done:

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_EXIT;
    return library;
}
/* @} */


/**
 * @name Get Function
 * @ingroup globus_openssl_error_object
 */
/* @{ */
/**
 * Get the function name where the openssl error occurred 
 * from the openssl error handle
 *
 * @param handle
 *        The openssl error handle
 * @return
 *        The function name
 */
const char *
globus_openssl_error_handle_get_function(
    globus_openssl_error_handle_t       handle)
{
    const char *                        function;
    static char *                       _function_name_ =
        "globus_openssl_error_handle_get_function";

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_ENTER;

    if(handle != NULL)
    {
        function = ERR_func_error_string(handle->error_code);
        goto done;
    }
    
    function = NULL;

done:

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_EXIT;
    return function;
}
/* @} */

/**
 * @name Get Reason
 * @ingroup globus_openssl_error_object
 */
/* @{ */
/**
 * Get the reason string which caused the openssl error 
 * from the openssl error handle
 *
 * @param handle
 *        The openssl error handle
 * @return
 *        The reson string
 */
const char *
globus_openssl_error_handle_get_reason(
    globus_openssl_error_handle_t       handle)
{
    const char *                        reason;
    static char *                       _function_name_ =
        "globus_openssl_error_handle_get_reason";

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_ENTER;

    if(handle != NULL)
    {
        reason = _GOESL(ERR_reason_error_string(handle->error_code));
        goto done;
    }
    
    reason = NULL;

done:

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_EXIT;
    return reason;
}
/* @} */

/**
 * @name Construct Error
 * @ingroup globus_openssl_error_object
 */
/*@{*/
/**
 * Allocate and initialize an error of type GLOBUS_ERROR_TYPE_OPENSSL
 * This function, combined with @ref globus_error_initialize_openssl_error
 * will recursively generate globus error objects (of type globus_object_t)
 * from the errors on openssl's static error stack.  The errors will
 * be chained in a causal fashion to provide a path to the root cause
 * of the actual error.  
 *
 * NOTE:  the static stack openssl implements for its errors currently
 * only supports at most 16 errors, so if more are added, the errors
 * that were added first will be wiped out.  If 16 errors are counted
 * in the chain of openssl errors, its possible that some errors
 * (including the original error) are missing.
 *
 * @param base_source
 *        Pointer to the originating globus module.
 * @param base_cause
 *        The error object causing the error.  This parameter should
 *        be NULL in nearly all cases, as the root cause of an error
 *        will most likely be in the openssl code itself.  The 
 *        actual cause of the error is determined from the static 
 *        stack of openssl errors.
 *
 * @return
 *        The resulting error object. It is the user's responsibility
 *        to eventually free this object using globus_object_free(). A
 *        globus_result_t may be obtained by calling
 *        globus_error_put() on this object.        
 */
globus_object_t *
globus_error_construct_openssl_error(
    globus_module_descriptor_t *        base_source,
    globus_object_t *                   base_cause)
{
    globus_object_t *                   error = base_cause;
    globus_object_t *                   newerror;
    globus_openssl_error_handle_t       openssl_error_handle;

    static char *                       _function_name_ =
        "globus_error_construct_openssl_error";

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_ENTER;

    do
    {
        openssl_error_handle = globus_i_openssl_error_handle_init();
        openssl_error_handle->error_code =
            ERR_get_error_line_data(
                (const char **)&openssl_error_handle->filename, 
                &openssl_error_handle->linenumber,
                &openssl_error_handle->data,
                &openssl_error_handle->flags);
        
        if(openssl_error_handle->error_code != 0)
        {
            newerror = globus_object_construct(GLOBUS_ERROR_TYPE_OPENSSL);
            
            if((openssl_error_handle->flags & ERR_TXT_MALLOCED)
               && (openssl_error_handle->flags & ERR_TXT_STRING))
            {
                openssl_error_handle->data = strdup(
                    openssl_error_handle->data);
                assert(openssl_error_handle->data);
            }
            
            error = globus_error_initialize_openssl_error(
                newerror,
                base_source,
                error,
                openssl_error_handle);
        }
    } while(openssl_error_handle->error_code != 0);
    
    globus_i_openssl_error_handle_destroy(openssl_error_handle);        

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_EXIT;
    return error;
}/* globus_error_construct_openssl_error() */
/*@}*/

/**
 * @name Initialize Error
 */
/*@{*/
/**
 * Initialize a previously allocated error of type
 * GLOBUS_ERROR_TYPE_OPENSSL
 * @ingroup globus_openssl_error_object 
 *
 * @param error
 *        The previously allocated error object.
 * @param base_source
 *        Pointer to the originating module.
 * @param base_cause
 *        The error object causing the error. If this is the original
 *        error this paramater may be NULL.
 * @param openssl_error_handle
 *        The openssl error handle associated with this error, this
 *        parameter should already be initialized to contain the
 *        openssl error code associated with the error.
 * @return
 *        The resulting error object. You may have to call
 *        globus_error_put() on this object before passing it on.
 */
globus_object_t *
globus_error_initialize_openssl_error(
    globus_object_t *                   error,
    globus_module_descriptor_t *        base_source,
    globus_object_t *                   base_cause,
    globus_openssl_error_handle_t       openssl_error_handle)
{
    static char *                       _function_name_ =
        "globus_error_initialize_openssl_error";

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_ENTER;

    globus_object_set_local_instance_data(error, 
                                          (void *) openssl_error_handle);
    globus_error_initialize_base(error, base_source, base_cause);

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_EXIT;
    return error;
}/* globus_error_initialize_openssl_error() */
/*@}*/

/**
 * @name Get OpenSSL Filename
 * @ingroup globus_openssl_error_object
 */
/* @{ */
/**
 * Get the OpenSSL filename where the error occurred 
 *
 * @param error
 *        The globus object that represents the error
 *
 * @return
 *        The filename where the openssl error occurred
 */
const char *
globus_error_openssl_error_get_filename(
    globus_object_t *                   error)
{
    const char *                        filename;
    const globus_object_type_t *        type;
    static char *                       _function_name_ =
        "globus_error_openssl_error_get_filename";
    
    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_ENTER;

    if(error == NULL)
    {
        filename = NULL;
        goto done;
    }

    type = globus_object_get_type(error);
    
    if(globus_object_type_match(
           type, 
           GLOBUS_ERROR_TYPE_OPENSSL) != GLOBUS_TRUE)
    {
        filename = NULL;
        goto done;
    }
    
    filename = globus_openssl_error_handle_get_filename(
        (globus_openssl_error_handle_t)
        globus_object_get_local_instance_data(error));

done:

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_EXIT;
    return filename;
}
/* @} */


/**
 * @name Get OpenSSL Linenumber
 * @ingroup globus_openssl_error_object
 */
/* @{ */
/**
 * Get the OpenSSL linenumber where the error occurred 
 *
 * @param error
 *        The globus object that represents the error
 *
 * @return
 *        The linenumber where the openssl error occurred
 */
int
globus_error_openssl_error_get_linenumber(
    globus_object_t *                   error)
{
    int                                 linenumber;
    const globus_object_type_t *        type;
    static char *                       _function_name_ =
        "globus_error_openssl_error_get_linenumber";
    
    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_ENTER;

    if(error == NULL)
    {
        linenumber = -1;
        goto done;
    }

    type = globus_object_get_type(error);
    
    if(globus_object_type_match(
           type, 
           GLOBUS_ERROR_TYPE_OPENSSL) != GLOBUS_TRUE)
    {
        linenumber = -1;
        goto done;
    }
    
    linenumber = globus_openssl_error_handle_get_linenumber(
        (globus_openssl_error_handle_t)
        globus_object_get_local_instance_data(error));

done:

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_EXIT;
    return linenumber;
}
/* @} */

/**
 * @name Get OpenSSL Library
 * @ingroup globus_openssl_error_object
 */
/* @{ */
/**
 * Get the OpenSSL libraray the error occurred in
 *
 * @param error
 *        The globus object that represents the error
 *
 * @return 
 *        The library name where the openssl error occurred
 */
const char *
globus_error_openssl_error_get_library(
    globus_object_t *                   error)
{
    const char *                        library;
    const globus_object_type_t *        type;
    static char *                       _function_name_ =
        "globus_error_openssl_error_get_library";
    
    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_ENTER;

    if(error == NULL)
    {
        library = NULL;
        goto done;
    }

    type = globus_object_get_type(error);
    
    if(globus_object_type_match(
           type, 
           GLOBUS_ERROR_TYPE_OPENSSL) != GLOBUS_TRUE)
    {
        library = NULL;
        goto done;
    }
    
    library = globus_openssl_error_handle_get_library(
        (globus_openssl_error_handle_t)
        globus_object_get_local_instance_data(error));

done:

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_EXIT;
    return library;
}
/* @} */

/**
 * @name Get OpenSSL Function
 * @ingroup globus_openssl_error_object
 */
/* @{ */
/**
 * Get the OpenSSL filename where the error occurred 
 *
 * @param error
 *        The globus object that represents the error
 *
 * @return 
 *        The function name where the openssl error occurred
 */
const char *
globus_error_openssl_error_get_function(
    globus_object_t *                   error)
{
    const char *                        function;
    const globus_object_type_t *        type;
    static char *                       _function_name_ =
        "globus_error_openssl_error_get_function";
    
    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_ENTER;
    
    if(error == NULL)
    {
        function = NULL;
        goto done;
    }
    
    type = globus_object_get_type(error);
    
    if(globus_object_type_match(
        type, 
        GLOBUS_ERROR_TYPE_OPENSSL) != GLOBUS_TRUE)
    {
        function = NULL;
        goto done;
    }
    
    function = globus_openssl_error_handle_get_function(
        (globus_openssl_error_handle_t)
        globus_object_get_local_instance_data(error));
    
 done:
    
    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_EXIT;
    return function;
}
/* @} */

/**
 * @name Get OpenSSL Reason
 * @ingroup globus_openssl_error_object
 */
/* @{ */
/**
 * Get the OpenSSL reason for the error
 *
 * @param error
 *        The globus object that represents the error
 *
 * @return
 *        The reason for the openssl error
 */
const char *
globus_error_openssl_error_get_reason(
    globus_object_t *                   error)
{
    const char *                        reason;
    const globus_object_type_t *        type;
    static char *                       _function_name_ =
        "globus_error_openssl_error_get_reason";
    
    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_ENTER;

    if(error == NULL)
    {
        reason = NULL;
        goto done;
    }

    type = globus_object_get_type(error);
    
    if(globus_object_type_match(
           type, 
           GLOBUS_ERROR_TYPE_OPENSSL) != GLOBUS_TRUE)
    {
        reason = NULL;
        goto done;
    }
    
    reason = globus_openssl_error_handle_get_reason(
        (globus_openssl_error_handle_t)
        globus_object_get_local_instance_data(error));

done:

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_EXIT;
    return reason;
}
/* @} */

/**
 * @name Get OpenSSL Error Data
 * @ingroup globus_openssl_error_object
 */
/* @{ */
/**
 * Get the OpenSSL Error Data
 *
 * @param error
 *        The globus object that represents the error
 *
 * @return
 *        The error data for the openssl error
 */
const char *
globus_error_openssl_error_get_data(
    globus_object_t *                   error)
{
    const char *                        data;
    const globus_object_type_t *        type;
    static char *                       _function_name_ =
        "globus_error_openssl_error_get_data";
    
    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_ENTER;

    if(error == NULL)
    {
        data = NULL;
        goto done;
    }

    type = globus_object_get_type(error);
    
    if(globus_object_type_match(
           type, 
           GLOBUS_ERROR_TYPE_OPENSSL) != GLOBUS_TRUE)
    {
        data = NULL;
        goto done;
    }
    
    data = globus_openssl_error_handle_get_data(
        (globus_openssl_error_handle_t)
        globus_object_get_local_instance_data(error));

done:

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_EXIT;
    return data;
}
/* @} */

/**
 * @name Get OpenSSL Error Data Flags
 * @ingroup globus_openssl_error_object
 */
/* @{ */
/**
 * Get the OpenSSL Error Data Flags
 *
 * @param error
 *        The globus object that represents the error
 *
 * @return
 *        The error data flags for the openssl error
 */
int
globus_error_openssl_error_get_data_flags(
    globus_object_t *                   error)
{
    int                                 flags;
    const globus_object_type_t *        type;
    static char *                       _function_name_ =
        "globus_error_openssl_error_get_data_flags";
    
    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_ENTER;

    if(error == NULL)
    {
        flags = 0;
        goto done;
    }

    type = globus_object_get_type(error);
    
    if(globus_object_type_match(
           type, 
           GLOBUS_ERROR_TYPE_OPENSSL) != GLOBUS_TRUE)
    {
        flags = 0;
        goto done;
    }
    
    flags = globus_openssl_error_handle_get_data_flags(
        (globus_openssl_error_handle_t)
        globus_object_get_local_instance_data(error));

done:

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_EXIT;
    return flags;
}
/* @} */


/**
 * @name OpenSSL Error Match
 */
/*@{*/
/**
 * Check whether the error originated from a specific library, from a specific
 * function and is of a specific type.
 * @ingroup globus_openssl_error_utility  
 *
 * This function checks whether the error or any of it's causative
 * errors originated from a specific library, specific function and is of a
 * specific type. 
 *
 * @param error
 *        The error object for which to perform the check
 * @param library
 *        The library to check for
 * @param function
 *        The function to check for
 * @param reason
 *        The type to check for
 * @return
 *        GLOBUS_TRUE - the error matched 
 *        GLOBUS_FALSE - the error failed to match 
 */
globus_bool_t
globus_error_match_openssl_error(
    globus_object_t *                   error,
    unsigned long                       library,
    unsigned long                       function,
    unsigned long                       reason)
{
    globus_openssl_error_handle_t       instance_data;

    if(error == NULL)
    {
        return GLOBUS_FALSE;
    }

    if(globus_object_get_type(error) != GLOBUS_ERROR_TYPE_OPENSSL)
    {
        /* not our type, skip it */
        return globus_error_match_openssl_error(
            globus_error_get_cause(error),
            library,
            function,
            reason);
    }

    instance_data = (globus_openssl_error_handle_t)
        globus_object_get_local_instance_data(error);
        
    
    if(library == ERR_GET_LIB(instance_data->error_code) &&
       function == ERR_GET_FUNC(instance_data->error_code) &&
       reason == ERR_GET_REASON(instance_data->error_code))
    {
        return GLOBUS_TRUE;
    }
    else
    {
        return globus_error_match_openssl_error(
            globus_error_get_cause(error),
            library,
            function,
            reason);
    }
}
/* globus_error_match_openssl_error */
/*@}*/

/**
 * @name Wrap OpenSSL Error
 */
/* @{ */
/**
 * Wrap the OpenSSL error and create a wrapped globus error object from the
 * error. 
 * @ingroup globus_openssl_error_utility
 *
 * This function gets all the openssl errors
 * from the error list, and chains them using the globus
 * error string object.  The resulting globus error object
 * is a wrapper to the openssl error at the end of the chain.
 *
 * @param base_source
 *        The module that the error was generated from
 * @param error_type
 *        The type of error encapsulating the openssl error
 * @param source_file
 *        Name of file.  Use __FILE__
 * @param source_func
 *        Name of function.  Use _globus_func_name and declare your func with
 *        GlobusFuncName(<name>)
 * @param source_line
 *        Line number.  Use __LINE__
 * @param format
 *        format string for the description of the error entry
 *        point where the openssl error occurred, should be followed
 *        by parameters to fill the format string (like in printf).
 * @return 
 *        The globus error object.  A globus_result_t
 *        object can be created using the globus_error_put
 *        function
 *
 * @see globus_error_put()
 */
globus_object_t *
globus_error_wrap_openssl_error(
    globus_module_descriptor_t *        base_source,
    int                                 error_type,
    const char *                        source_file,
    const char *                        source_func,
    int                                 source_line,
    const char *                        format,
    ...)
{
    va_list                             ap;
    globus_object_t *                   causal_error;
    globus_object_t *                   error;
    static char *                       _function_name_ =
        "globus_error_wrap_openssl_error";

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_ENTER;

    causal_error = globus_error_construct_openssl_error(base_source, NULL);

    va_start(ap, format);

    error = globus_error_v_construct_error(
        base_source,
        causal_error,
        error_type,
        source_file,
        source_func,
        source_line,
        format,
        ap);

    va_end(ap);

    if(error == GLOBUS_NULL)
    {
        globus_object_free(causal_error);
    }

    GLOBUS_I_GSI_OPENSSL_ERROR_DEBUG_EXIT;
    return error;
}
/* @} */
