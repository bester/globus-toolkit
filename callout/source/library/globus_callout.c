#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file globus_callout.c
 * Globus Callout Infrastructure
 * @author Sam Meder
 *
 * $RCSfile$
 * $Revision$
 * $Date$
 */

#include "globus_common.h"
#include "globus_callout_constants.h"
#include "globus_i_callout.h"
#ifndef WIN32
#include "ltdl.h"
#else
#include "win_ltdl.h"
#endif
#include "version.h"

/* ToDo: HACK! This is undefined on the Windows side so do this for now */
#ifdef WIN32
#define flavor "win32dbg"
#endif
#define GLOBUS_I_CALLOUT_HASH_SIZE 64

static void
globus_l_callout_library_table_element_free(
    void *                              element);

static void
globus_l_callout_data_free(
    globus_i_callout_data_t *           data);


static int globus_l_callout_activate(void);
static int globus_l_callout_deactivate(void);

int                              globus_i_callout_debug_level   = 0;
FILE *                           globus_i_callout_debug_fstream = NULL;

/**
 * Module descriptor static initializer.
 * @ingroup globus_callout_activation
 */
globus_module_descriptor_t globus_i_callout_module =
{
    "globus_callout_module",
    globus_l_callout_activate,
    globus_l_callout_deactivate,
    GLOBUS_NULL,
    GLOBUS_NULL,
    &local_version
};

/**
 * Module activation
 * @ingroup globus_callout_activation
 */
static
int
globus_l_callout_activate(void)
{
    int                                 result = (int) GLOBUS_SUCCESS;
    char *                              tmp_string;
    static char *                       _function_name_ =
        "globus_l_callout_activate";

    tmp_string = globus_module_getenv("GLOBUS_CALLOUT_DEBUG_LEVEL");
    if(tmp_string != GLOBUS_NULL)
    {
        globus_i_callout_debug_level = atoi(tmp_string);
        
        if(globus_i_callout_debug_level < 0)
        {
            globus_i_callout_debug_level = 0;
        }
    }

    tmp_string = globus_module_getenv("GLOBUS_CALLOUT_DEBUG_FILE");
    if(tmp_string != GLOBUS_NULL)
    {
        globus_i_callout_debug_fstream = fopen(tmp_string, "a");
        if(globus_i_callout_debug_fstream == NULL)
        {
            result = (int) GLOBUS_FAILURE;
            goto exit;
        }
    }
    else
    {
        globus_i_callout_debug_fstream = stderr;
    }

    GLOBUS_I_CALLOUT_DEBUG_ENTER;

    result = globus_module_activate(GLOBUS_COMMON_MODULE);

    if(result != GLOBUS_SUCCESS)
    {
        goto exit;
    }

    if((result = lt_dlinit()) != 0)
    {
        goto exit;
    }
    
    GLOBUS_I_CALLOUT_DEBUG_EXIT;

 exit:

    return result;
}

/**
 * Module deactivation
 * @ingroup globus_callout_activation
 */
static
int
globus_l_callout_deactivate(void)
{
    int                                 result = (int) GLOBUS_SUCCESS;
    static char *                       _function_name_ =
        "globus_l_callout_deactivate";

    GLOBUS_I_CALLOUT_DEBUG_ENTER;

    globus_module_deactivate(GLOBUS_COMMON_MODULE);

    GLOBUS_I_CALLOUT_DEBUG_EXIT;

    if(globus_i_callout_debug_fstream != stderr)
    {
        fclose(globus_i_callout_debug_fstream);
    }

    result = lt_dlexit();

    return result;
}

#endif

/**
 * @name Initialize Handle
 */
/*@{*/
/**
 * Initialize a Globus Callout Handle
 * @ingroup globus_callout_handle
 *
 * @param handle
 *        Pointer to the handle that is to be initialized
 * @return
 *        GLOBUS_SUCCESS if successful
 *        A Globus error object on failure:
 *            GLOBUS_CALLOUT_ERROR_WITH_HASHTABLE
 */
globus_result_t
globus_callout_handle_init(
    globus_callout_handle_t *           handle)
{
    int                                 rc;
    globus_result_t                     result = GLOBUS_SUCCESS;

    static char *                       _function_name_ =
        "globus_callout_handle_init";

    GLOBUS_I_CALLOUT_DEBUG_ENTER;

    *handle = malloc(sizeof(globus_i_callout_handle_t));

    if(*handle == NULL)
    {
        GLOBUS_CALLOUT_MALLOC_ERROR(result);
        goto exit;
    }
    
    if((rc = globus_hashtable_init(&((*handle)->symbol_htable),
                                   GLOBUS_I_CALLOUT_HASH_SIZE,
                                   globus_hashtable_string_hash,
                                   globus_hashtable_string_keyeq)) < 0)
    {
        free(*handle);
        GLOBUS_CALLOUT_ERROR_RESULT(
            result,
            GLOBUS_CALLOUT_ERROR_WITH_HASHTABLE,
            ("globus_hashtable_init retuned %d", rc));
        goto exit;
    }    

    if((rc = globus_hashtable_init(&((*handle)->library_htable),
                                   GLOBUS_I_CALLOUT_HASH_SIZE,
                                   globus_hashtable_string_hash,
                                   globus_hashtable_string_keyeq)) < 0)
    {
        globus_hashtable_destroy(&((*handle)->symbol_htable));
        free(*handle);
        GLOBUS_CALLOUT_ERROR_RESULT(
            result,
            GLOBUS_CALLOUT_ERROR_WITH_HASHTABLE,
            ("globus_hashtable_init retuned %d", rc));
        goto exit;
    }    

    GLOBUS_I_CALLOUT_DEBUG_EXIT;

 exit:
    
    return result;
}/*globus_callout_handle_init*/
/*@}*/

/**
 * @name Destroy Handle
 */
/*@{*/
/**
 * Destroy a Globus Callout Handle
 * @ingroup globus_callout_handle
 *
 * @param handle
 *        The handle that is to be destroyed
 * @return
 *        GLOBUS_SUCCESS
 */
globus_result_t
globus_callout_handle_destroy(
    globus_callout_handle_t             handle)
{
    globus_result_t                     result = GLOBUS_SUCCESS;
    static char *                       _function_name_ =
        "globus_callout_handle_destroy";
    GLOBUS_I_CALLOUT_DEBUG_ENTER;
    
    /* free hashes */

    globus_hashtable_destroy_all(
        &(handle->library_htable),
        globus_l_callout_library_table_element_free);

    globus_hashtable_destroy_all(
        &(handle->symbol_htable),
        globus_l_callout_data_free);

    free(handle);
    
    GLOBUS_I_CALLOUT_DEBUG_EXIT;

    return result;
}/*globus_callout_handle_destroy*/
/*@}*/

/**
 * @name Configure Callouts
 */
/*@{*/
/**
 * Read callout configuration from file.
 * @ingroup globus_callout_config
 *
 * This function read a configuration file with the following format:
 *    - Anything after a '#' is assumed to be a comment
 *    - Blanks lines are ignored
 *    - Lines specifying callouts have the format
 *      abstract type           library         symbol
 *      where "abstract type" denotes the type of callout,
 *      e.g. globus_gram_jobmanager_authz, "library" denotes the library the
 *      callout can be found in and "symbol" denotes the function name of the
 *      callout. The library argument can be specified in two forms, libfoo or
 *      libfoo_<flavor>. When using the former version the current flavor will
 *      automatically be added to the library name. 
 *
 * @param handle
 *        The handle that is to be configured
 * @param filename
 *        The file to read configuration from
 * @return
 *        GLOBUS_SUCCESS
 *        A Globus error object on failure:
 *            GLOBUS_CALLOUT_ERROR_OPENING_CONF_FILE
 *            GLOBUS_CALLOUT_ERROR_PARSING_CONF_FILE
 *            GLOBUS_CALLOUT_ERROR_WITH_HASHTABLE
 *            GLOBUS_CALLOUT_ERROR_OUT_OF_MEMORY
 */
globus_result_t
globus_callout_read_config(
    globus_callout_handle_t             handle,
    char *                              filename)
{
    FILE *                              conf_file;
    char                                buffer[512];
    char                                type[128];
    char                                library[256];
    char                                symbol[128];
    char *                              flavor_start;
    char *                              pound;
    int                                 index;
    int                                 rc;
    globus_result_t                     result;
    globus_i_callout_data_t *           datum = NULL;
    globus_i_callout_data_t *           existing_datum;
    

    static char *                       _function_name_ =
        "globus_callout_read_config";

    GLOBUS_I_CALLOUT_DEBUG_ENTER;
    
    conf_file = fopen(filename, "r");

    if(conf_file == NULL)
    {
        GLOBUS_CALLOUT_ERRNO_ERROR_RESULT(
            result,
            GLOBUS_CALLOUT_ERROR_OPENING_CONF_FILE,
            ("filename %s", filename));
        goto error_exit;
    }
    
    while(fgets(buffer,512,conf_file))
    {
        /* strip any comments */

        pound = strchr(buffer, '#');

        if(pound != NULL)
        { 
            *pound = '\0';
        }

        /* strip white space from start */
        
        index = 0;

        while(buffer[index] == '\t' || buffer[index] == ' ')
        {
            index++;
        }

        /* if blank line continue */
        
        if(buffer[index] == '\0' || buffer[index] == '\n')
        { 
            continue;
        }
        
        if(sscanf(&buffer[index],"%127s%255s%127s",type,library,symbol) < 3)
        {
            GLOBUS_CALLOUT_ERROR_RESULT(
                result,
                GLOBUS_CALLOUT_ERROR_PARSING_CONF_FILE,
                ("malformed line: %s", &buffer[index]));
            goto error_exit;
        }
        
        /* push values into hash */

        datum = malloc(sizeof(globus_i_callout_data_t));

        if(datum == NULL)
        {
            GLOBUS_CALLOUT_MALLOC_ERROR(result);
            goto error_exit;
        }

        memset(datum,'\0',sizeof(globus_i_callout_data_t));

        datum->next = datum;

        /* check if library is flavored already */

        if((flavor_start = strrchr(library,'_')) &&
           (strstr(flavor_start, "32") || strstr(flavor_start, "64")))
        {
            datum->file = strdup(library);
            
            if(datum->file == NULL)
            {
                GLOBUS_CALLOUT_MALLOC_ERROR(result);
                goto error_exit;
            }
        }
        else
        { 
            datum->file = malloc(strlen(library) + 2 + strlen(flavor));
            if(datum->file == NULL)
            {
                GLOBUS_CALLOUT_MALLOC_ERROR(result);
                goto error_exit;
            }
            datum->file[0] = '\0';
            strcat(datum->file, library);
            strcat(datum->file, "_");
            strcat(datum->file, flavor);
        }
        
        datum->symbol = strdup(symbol);

        if(datum->symbol == NULL)
        {
            GLOBUS_CALLOUT_MALLOC_ERROR(result);
            goto error_exit;
        }
        
        datum->type = strdup(type);

        if(datum->type == NULL)
        {
            GLOBUS_CALLOUT_MALLOC_ERROR(result);
            goto error_exit;
        }

        if((rc = globus_hashtable_insert(&handle->symbol_htable,
                                         datum->type,
                                         datum)) == -1)
        {
            existing_datum = globus_hashtable_lookup(&handle->symbol_htable,
                                                     datum->type);
            datum->next = existing_datum->next;
            existing_datum->next = datum;
        }
        else if(rc < 0)
        {
            GLOBUS_CALLOUT_ERROR_RESULT(
                result,
                GLOBUS_CALLOUT_ERROR_WITH_HASHTABLE,
                ("globus_hashtable_insert retuned %d", rc));
            goto error_exit;
        }
    }

    fclose(conf_file);
    
    GLOBUS_I_CALLOUT_DEBUG_EXIT;

    return GLOBUS_SUCCESS;

 error_exit:

    if(datum != NULL)
    {
        globus_l_callout_data_free(datum);
    }

    if(conf_file != NULL)
    {
        fclose(conf_file);
    }

    return result;
}/*globus_callout_read_config*/

/**
 * Register callout configuration
 * @ingroup globus_callout_config
 *
 * This function registers a callout type in the given handle.
 *
 * @param handle
 *        The handle that is to be configured
 * @param type
 *        The abstract type of the callout
 * @param library
 *        The location of the library containing the callout
 * @param symbol
 *        The symbol (ie function name) for the callout
 * @return
 *        GLOBUS_SUCCESS
 *        A Globus error object on failure:
 *            GLOBUS_CALLOUT_ERROR_WITH_HASHTABLE
 *            GLOBUS_CALLOUT_ERROR_OUT_OF_MEMORY
 */
globus_result_t
globus_callout_register(
    globus_callout_handle_t             handle,
    char *                              type,
    char *                              library,
    char *                              symbol)
{
    int                                 rc;
    globus_result_t                     result;
    globus_i_callout_data_t *           datum = NULL;
    globus_i_callout_data_t *           existing_datum;
    char *                              flavor_start;
    
    static char *                       _function_name_ =
        "globus_callout_register";

    GLOBUS_I_CALLOUT_DEBUG_ENTER;
    
    
    /* push values into hash */

    datum = malloc(sizeof(globus_i_callout_data_t));
    
    if(datum == NULL)
    {
        GLOBUS_CALLOUT_MALLOC_ERROR(result);
        goto error_exit;
    }
    
    memset(datum,'\0',sizeof(globus_i_callout_data_t));

    datum->next = datum;


    if((flavor_start = strrchr(library,'_')) &&
       (strstr(flavor_start, "32") || strstr(flavor_start, "64")))
    {
        datum->file = strdup(library);
        
        if(datum->file == NULL)
        {
            GLOBUS_CALLOUT_MALLOC_ERROR(result);
            goto error_exit;
        }
    }
    else
    { 
        datum->file = malloc(strlen(library) + 2 + strlen(flavor));
        if(datum->file == NULL)
        {
            GLOBUS_CALLOUT_MALLOC_ERROR(result);
            goto error_exit;
        }
        datum->file[0] = '\0';
        strcat(datum->file, library);
        strcat(datum->file, "_");            
        strcat(datum->file, flavor);
    }
    
    datum->symbol = strdup(symbol);
    
    if(datum->symbol == NULL)
    {
        GLOBUS_CALLOUT_MALLOC_ERROR(result);
        goto error_exit;
    }
    
    datum->type = strdup(type);
    
    if(datum->type == NULL)
    {
        GLOBUS_CALLOUT_MALLOC_ERROR(result);
        goto error_exit;
    }
    
    if((rc = globus_hashtable_insert(&handle->symbol_htable,
                                     datum->type,
                                     datum)) == -1)
    {
        existing_datum = globus_hashtable_lookup(&handle->symbol_htable,
                                                 datum->type);
        datum->next = existing_datum->next;
        existing_datum->next = datum;
    }
    else if(rc < 0)
    {
        GLOBUS_CALLOUT_ERROR_RESULT(
            result,
            GLOBUS_CALLOUT_ERROR_WITH_HASHTABLE,
            ("globus_hashtable_insert retuned %d", rc));
        goto error_exit;
    }
    
    GLOBUS_I_CALLOUT_DEBUG_EXIT;

    return GLOBUS_SUCCESS;

 error_exit:

    GLOBUS_I_CALLOUT_DEBUG_EXIT;
    
    if(datum != NULL)
    {
        globus_l_callout_data_free(datum);
    }
 
    return result;
}/*globus_callout_register*/
/*@}*/


/**
 * @name Invoking Callouts
 */
/*@{*/
/**
 * Call a callout of specified abstract type
 * @ingroup globus_callout_call
 *
 * This function looks up the callouts corresponding to the given type and
 * invokes them with the passed arguments. If a invoked callout returns an
 * error it will be chained to a error of the type
 * GLOBUS_CALLOUT_ERROR_CALLOUT_ERROR and no more callouts will be called.
 *
 * @param handle
 *        A configured callout handle
 * @param type
 *        The abstract type of the callout that is to be invoked
 * @return
 *        GLOBUS_SUCCESS
 *        A Globus error object on failure:
 *            GLOBUS_CALLOUT_ERROR_TYPE_NOT_REGISTERED
 *            GLOBUS_CALLOUT_ERROR_CALLOUT_ERROR
 *            GLOBUS_CALLOUT_ERROR_WITH_DL
 *            GLOBUS_CALLOUT_ERROR_WITH_HASHTABLE
 *            GLOBUS_CALLOUT_ERROR_OUT_OF_MEMORY
 */
globus_result_t
globus_callout_call_type(
    globus_callout_handle_t             handle,
    char *                              type,
    ...)
{
    globus_i_callout_data_t *           datum;
    globus_i_callout_data_t *           current_datum;
    lt_ptr                              function;
    globus_result_t                     result = GLOBUS_SUCCESS;
    va_list                             ap;
    int                                 rc;
    lt_dlhandle *                       dlhandle;
    char *                              dlerror;
    static char *                       _function_name_ =
        "globus_callout_handle_call_type";
    GLOBUS_I_CALLOUT_DEBUG_ENTER;

    datum = globus_hashtable_lookup(&handle->symbol_htable,
                                    type);

    if(datum == NULL)
    {
        GLOBUS_CALLOUT_ERROR_RESULT(
            result,
            GLOBUS_CALLOUT_ERROR_TYPE_NOT_REGISTERED,
            ("unknown type: %s\n", type));
        goto exit;
    }

    current_datum = datum->next;

    do
    {
        dlhandle = globus_hashtable_lookup(&handle->library_htable,
                                           current_datum->file);

        if(dlhandle == NULL)
        {
            dlhandle = malloc(sizeof(lt_dlhandle));
            
            if(dlhandle == NULL)
            {
                GLOBUS_CALLOUT_MALLOC_ERROR(result);
            }
            
            *dlhandle = NULL;
            rc = globus_hashtable_insert(&handle->library_htable,
                                         datum->file,
                                         dlhandle);
            if(rc < 0)
            {
                free(dlhandle);
                GLOBUS_CALLOUT_ERROR_RESULT(
                    result,
                    GLOBUS_CALLOUT_ERROR_WITH_HASHTABLE,
                    ("globus_hashtable_insert retuned %d", rc));
                goto exit;
            }            
        }
    
        if(*dlhandle == NULL)
        {
            /* first time a symbol is referenced in this library ->
             * need to open it
             */
            
            *dlhandle = lt_dlopenext(datum->file);
            
            if(*dlhandle == NULL)
            {
                GLOBUS_CALLOUT_ERROR_RESULT(
                    result,
                    GLOBUS_CALLOUT_ERROR_WITH_DL,
                    ("couldn't dlopen %s: %s\n",
                     datum->file,
                     (dlerror = lt_dlerror()) ? dlerror : "(null)"));
                goto exit;
            }
        }

        function = lt_dlsym(*dlhandle, datum->symbol);

        if(function == NULL)
        {
            GLOBUS_CALLOUT_ERROR_RESULT(
                result,
                GLOBUS_CALLOUT_ERROR_WITH_DL,
                ("symbol %s could not be found in %s: %s\n",
                 datum->symbol,
                 datum->file,
                 (dlerror = lt_dlerror()) ? dlerror : "(null)"));
            goto exit;
        }

        va_start(ap,type);
    
        result = ((globus_callout_function_t) function)(ap);
        
        va_end(ap);
        
        if(result != GLOBUS_SUCCESS)
        {
            GLOBUS_CALLOUT_ERROR_CHAIN_RESULT(
                result,
                GLOBUS_CALLOUT_ERROR_CALLOUT_ERROR);
            goto exit;
        }
        current_datum = current_datum->next;
    }
    while(current_datum != datum->next);
    
 exit:
    GLOBUS_I_CALLOUT_DEBUG_EXIT;
    return result;
}/*globus_callout_call_type*/
/*@}*/

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL

static void
globus_l_callout_data_free(
    globus_i_callout_data_t *           data)
{
    static char *                       _function_name_ =
        "globus_l_callout_data_free";
    GLOBUS_I_CALLOUT_DEBUG_ENTER;

    if(data != NULL)
    {
        if(data->type != NULL)
        {
            free(data->type);
        }
        
        if(data->file != NULL)
        {
            free(data->file);
        }
        
        if(data->symbol != NULL)
        {
            free(data->symbol);
        }
        
        free(data);
    }
    
    GLOBUS_I_CALLOUT_DEBUG_EXIT;
}



static void
globus_l_callout_library_table_element_free(
    void *                              element)
{
    lt_dlhandle *                       dlhandle;
    globus_result_t                     result;
    static char *                       _function_name_ =
        "globus_l_callout_library_table_element_free";
    GLOBUS_I_CALLOUT_DEBUG_ENTER;

    dlhandle = (lt_dlhandle *) element;

    if(dlhandle != NULL)
    { 
        if(*dlhandle != NULL)
        {
            if(lt_dlclose(*dlhandle) < 0)
            {
                GLOBUS_CALLOUT_ERROR_RESULT(
                    result,
                    GLOBUS_CALLOUT_ERROR_WITH_DL,
                    ("failed to close library"));
            }
        }

        free(dlhandle);
    }
    
    GLOBUS_I_CALLOUT_DEBUG_EXIT;
    return;
}

#endif
