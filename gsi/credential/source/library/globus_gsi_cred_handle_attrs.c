#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file globus_gsi_cred_handle_attrs.c
 * @author Sam Lang, Sam Meder
 *
 * $RCSfile$
 * $Revision$
 * $Date$
 */
#endif

#include "globus_i_gsi_credential.h"
#include "globus_gsi_system_config.h"
#include "globus_error_generic.h"
#include "openssl/pem.h"
#include "openssl/x509.h"

#define GLOBUS_I_GSI_CRED_HANDLE_ATTRS_MALLOC_ERROR(_RESULT_) \
        _RESULT_ = globus_error_put(globus_error_wrap_errno_error( \
            GLOBUS_GSI_CREDENTIAL_MODULE, \
            errno, \
            GLOBUS_GSI_CRED_ERROR_WITH_CRED_HANDLE_ATTRS, \
            __FILE__":__LINE__:%s:%s", \
            _function_name_, \
            globus_l_gsi_cred_error_strings[ \
                GLOBUS_GSI_CRED_ERROR_WITH_CRED_HANDLE_ATTRS]))

/**
 * Initialize Credential Handle Attributes
 * @ingroup globus_gsi_cred_handle_attrs
 */
/* @{ */
/**
 * Initializes the immutable Credential Handle Attributes
 * The handle attributes are initialized as follows:
 * 
 * @param handle_attrs
 *        the attributes to be initialized
 * @return
 *        GLOBUS_SUCESS if initialization was successful,
 *        otherwise an error is returned
 */
globus_result_t 
globus_gsi_cred_handle_attrs_init(
    globus_gsi_cred_handle_attrs_t *    handle_attrs)
{
    globus_result_t                     result;

    static char *                       _function_name_ =
        "globus_gsi_cred_handle_attrs_init";

    GLOBUS_I_GSI_CRED_DEBUG_ENTER;

    if(handle_attrs == NULL)
    {
        GLOBUS_GSI_CRED_ERROR_RESULT(
            result,
            GLOBUS_GSI_CRED_ERROR_WITH_CRED_HANDLE_ATTRS,
            ("NULL handle attributes passed to function: %s", 
             _function_name_));
        goto exit;
    }

    if((*handle_attrs = (globus_gsi_cred_handle_attrs_t)
        malloc(sizeof(globus_i_gsi_cred_handle_attrs_t))) == NULL)
    {
        GLOBUS_I_GSI_CRED_HANDLE_ATTRS_MALLOC_ERROR(result);
        goto exit;
    }

    /* initialize all the handle attributes to NULL */
    memset(*handle_attrs, 
           (int) NULL, 
           sizeof(globus_i_gsi_cred_handle_attrs_t));
    
    (*handle_attrs)->search_order = 
        (globus_gsi_cred_type_t *) 
        malloc(sizeof(globus_gsi_cred_type_t) * 5);

    (*handle_attrs)->search_order[0] = GLOBUS_SERVICE;
    (*handle_attrs)->search_order[1] = GLOBUS_HOST;
    (*handle_attrs)->search_order[2] = GLOBUS_PROXY;
    (*handle_attrs)->search_order[3] = GLOBUS_USER;
    (*handle_attrs)->search_order[4] = GLOBUS_SO_END;

    result = GLOBUS_SUCCESS;
    goto exit;

 exit:

    GLOBUS_I_GSI_CRED_DEBUG_EXIT;
    return result;
}
/* globus_gsi_cred_handle_attrs_init */
/* @} */


/**
 * Destroy Credential Handle Attributes
 * @ingroup globus_gsi_cred_handle_attrs
 */
/* @{ */
/**
 * Destroy the Credential Handle Attributes.  This function
 * does some cleanup and deallocation of the handle attributes.
 * The attributes should be set to NULL after this function is called.
 * 
 * @param handle_attrs
 *        The handle attributes to destroy
 *
 * @return 
 *        GLOBUS_SUCCESS
 */
globus_result_t globus_gsi_cred_handle_attrs_destroy(
    globus_gsi_cred_handle_attrs_t     handle_attrs)
{
    static char *                       _function_name_ =
        "globus_gsi_cred_handle_attrs_destroy";

    GLOBUS_I_GSI_CRED_DEBUG_ENTER;
    
    if(handle_attrs != NULL)
    {
        if(handle_attrs->search_order != NULL)
        {
            globus_libc_free(handle_attrs->search_order);
        }

        globus_libc_free(handle_attrs);
    }
    
    GLOBUS_I_GSI_CRED_DEBUG_EXIT;
    
    return GLOBUS_SUCCESS;
}

/**
 * Copy Credential Handle Attributes
 * @ingroup globus_gsi_cred_handle_attrs
 */
/* @{ */
/**
 * Copy the Credential Handle Attributes. 
 *
 * @param a 
 *        The handle attribute to be copied
 * @param b
 *        The copy
 * @return
 *        GLOBUS_SUCESS unless there was an error, in which
 *        case an error object is returned.
 */
globus_result_t 
globus_gsi_cred_handle_attrs_copy(
    globus_gsi_cred_handle_attrs_t      a,
    globus_gsi_cred_handle_attrs_t *    b)
{
    int                                 size;
    int                                 index;
    globus_result_t                     result;

    static char *                       _function_name_ =
        "globus_gsi_cred_handle_attrs_copy";

    GLOBUS_I_GSI_CRED_DEBUG_ENTER;

    if(a == NULL || b == NULL)
    {
        GLOBUS_GSI_CRED_ERROR_RESULT(
            result,
            GLOBUS_GSI_CRED_ERROR_WITH_CRED_HANDLE_ATTRS,
            ("NULL parameter passed to function: %s", _function_name_));
        goto exit;
    }

    result = globus_gsi_cred_handle_attrs_init(b);
    if(result != GLOBUS_SUCCESS)
    {
        *b = NULL;
        GLOBUS_GSI_CRED_ERROR_CHAIN_RESULT(
            result,
            GLOBUS_GSI_CRED_ERROR_WITH_CRED_HANDLE_ATTRS);
        goto exit;
    }
    
    size = -1;
    while(a->search_order[++size] != GLOBUS_SO_END);

    if((!(*b)->search_order) && ((*b)->search_order = 
        (globus_gsi_cred_type_t *) malloc(sizeof(globus_gsi_cred_type_t) 
                                          * (size + 1))) == NULL)
    {
        GLOBUS_I_GSI_CRED_HANDLE_ATTRS_MALLOC_ERROR(result);
        goto exit;
    }        

    for(index = 0; index <= size; ++index)
    {
        (*b)->search_order[index] = a->search_order[index];
    }

    result = GLOBUS_SUCCESS;

 exit:

    if(result != GLOBUS_SUCCESS &&
       *b != NULL)
    {
        globus_gsi_cred_handle_attrs_destroy(*b);
    }
    
    GLOBUS_I_GSI_CRED_DEBUG_EXIT;
    return result;
}
/* globus_gsi_cred_handle_attrs_copy */
/* @} */
    
/** 
 * Set CA Cert Dir
 * @ingroup globus_gsi_cred_handle_attrs
 */
/* @{ */
/**
 * Set the Trusted CA Certificate Directory Location
 *
 * @param handle_attrs
 *        the credential handle attributes to set
 * @param ca_cert_dir
 *        the trusted ca certificates directory
 * @return
 *        GLOBUS_SUCCESS if no errors occurred.  In case of
 *        a null handle_attrs, an error object id is returned
 */
globus_result_t globus_gsi_cred_handle_attrs_set_ca_cert_dir(
    globus_gsi_cred_handle_attrs_t      handle_attrs,
    char *                              ca_cert_dir)
{
    globus_result_t                     result;
    static char *                       _function_name_ =
        "globus_gsi_cred_handle_attrs_set_ca_cert_dir";

    GLOBUS_I_GSI_CRED_DEBUG_ENTER;
    
    if(handle_attrs == NULL)
    {
        GLOBUS_GSI_CRED_ERROR_RESULT(
            result,
            GLOBUS_GSI_CRED_ERROR_WITH_CRED_HANDLE_ATTRS,
            ("NULL handle attributes passed to function: %s", 
             _function_name_));
        goto exit;
    }

    /* NOTE: This function has been turned into a no-op */
    
    result = GLOBUS_SUCCESS;

 exit:

    GLOBUS_I_GSI_CRED_DEBUG_EXIT;
    return result;
}
/* @} */

/**
 * Get CA Cert Dir
 * @ingroup globus_gsi_cred_handle_attrs
 */
/* @{ */
/** 
 * Get the trusted ca cert directory
 *
 * @param handle_attrs
 *        the credential handle attributes to get the trusted ca cert 
 *        directory from
 * @param ca_cert_dir
 *        the trusted ca certificates directory
 * @return
 *        GLOBUS_SUCCESS if no errors occurred.  In case of
 *        a null handle_attrs or pointer to ca_cert_dir, 
 *        an error object id is returned
 */
globus_result_t globus_gsi_cred_handle_attrs_get_ca_cert_dir(
    globus_gsi_cred_handle_attrs_t      handle_attrs,
    char **                             ca_cert_dir)
{
    globus_result_t                     result;
    static char *                       _function_name_ =
        "globus_gsi_cred_handle_attrs_get_ca_cert_dir";

    GLOBUS_I_GSI_CRED_DEBUG_ENTER;

    if(handle_attrs == NULL)
    {
        GLOBUS_GSI_CRED_ERROR_RESULT(
            result,
            GLOBUS_GSI_CRED_ERROR_WITH_CRED_HANDLE_ATTRS,
            ("NULL handle attributes passed to function: %s", 
             _function_name_));
        goto exit;
    }
    
    if(ca_cert_dir == NULL)
    {
        GLOBUS_GSI_CRED_ERROR_RESULT(
            result,
            GLOBUS_GSI_CRED_ERROR_WITH_CRED_HANDLE_ATTRS,
            ("NULL handle attributes passed to function: %s",
             _function_name_));
        goto exit;
    }

    /* NOTE: This function has been turned into a no-op, please use
       GLOBUS_GSI_SYSCONFIG_GET_CERT_DIR to obtain the trusted certs
       directory
    */
    
    *ca_cert_dir = NULL;

    result = GLOBUS_SUCCESS;

 exit:

    GLOBUS_I_GSI_CRED_DEBUG_EXIT;
    return result;
}
/* @} */

/**
 * Set Search Order
 * @ingroup globus_gsi_cred_handle_attrs
 */
/* @{ */
/**
 * Set the search order for finding a user certificate.  The
 * default value is {PROXY, USER, HOST}
 *
 *
 * @param handle_attrs
 *        The handle attributes to set the search order of
 * @param search_order
 *        The search order.  Should be a three element array containing
 *        in some order PROXY, USER, HOST
 * @return 
 *        GLOBUS_SUCCESS unless handle_attrs is null
 */
globus_result_t globus_gsi_cred_handle_attrs_set_search_order(
    globus_gsi_cred_handle_attrs_t      handle_attrs,
    globus_gsi_cred_type_t              search_order[])
{
    int                                 size;
    int                                 index;
    globus_result_t                     result;

    static char *                       _function_name_ =
        "globus_gsi_cred_handle_attrs_set_search_order";

    GLOBUS_I_GSI_CRED_DEBUG_ENTER;

    if(handle_attrs == NULL)
    {
        GLOBUS_GSI_CRED_ERROR_RESULT(
            result,
            GLOBUS_GSI_CRED_ERROR_WITH_CRED_HANDLE_ATTRS,
            ("NULL handle attributes passed to function: %s",
             _function_name_));
        goto exit;
    }

    size = -1;
    while(search_order[++size] != GLOBUS_SO_END);

    if((handle_attrs->search_order = 
        (globus_gsi_cred_type_t *) malloc(sizeof(globus_gsi_cred_type_t) 
                                          * (size + 1))) == NULL)
    {
        GLOBUS_I_GSI_CRED_HANDLE_ATTRS_MALLOC_ERROR(result);
        goto exit;
    }        

    for(index = 0; index <= size; ++index)
    {
        handle_attrs->search_order[index] = search_order[index];
    }

    result = GLOBUS_SUCCESS;
 exit:

    GLOBUS_I_GSI_CRED_DEBUG_EXIT;
    return result;
}
/* @} */

/**
 * Get Search Order
 */
/* @{ */
/**
 * Get the search order of the handle attributes
 *
 * @param handle_attrs
 *        The handle attributes to get the search order from
 * @param search_order
 *        The search_order of the handle attributes
 * @return
 *        GLOBUS_SUCCESS unless handle_attrs is null
 */
globus_result_t globus_gsi_cred_handle_attrs_get_search_order(
    globus_gsi_cred_handle_attrs_t      handle_attrs,
    globus_gsi_cred_type_t **           search_order)
{
    int                                 size;
    int                                 index;
    globus_result_t                     result;
    static char *                       _function_name_ =
        "globus_gsi_cred_handle_attrs_get_search_order";

    GLOBUS_I_GSI_CRED_DEBUG_ENTER;

    if(handle_attrs == NULL)
    {
        GLOBUS_GSI_CRED_ERROR_RESULT(
            result,
            GLOBUS_GSI_CRED_ERROR_WITH_CRED_HANDLE_ATTRS,
            ("NULL handle attributes passed to function: %s",
             _function_name_));
        goto exit;
    }

    if(handle_attrs->search_order == NULL)
    {
        GLOBUS_GSI_CRED_ERROR_RESULT(
            result,
            GLOBUS_GSI_CRED_ERROR_WITH_CRED_HANDLE_ATTRS,
            ("The search order of the handle attributes is NULL"));
        goto exit;
    }

    size = -1;
    while(handle_attrs->search_order[++size] != GLOBUS_SO_END);

    if((*search_order = 
        (globus_gsi_cred_type_t *) malloc(sizeof(globus_gsi_cred_type_t) 
                                          * (size + 1))) == NULL)
    {
        GLOBUS_I_GSI_CRED_HANDLE_ATTRS_MALLOC_ERROR(result);
        goto exit;
    }        

    for(index = 0; index <= size; ++index)
    {
        (*search_order)[index] = handle_attrs->search_order[index];
    }

    result = GLOBUS_SUCCESS;

 exit:

    GLOBUS_I_GSI_CRED_DEBUG_EXIT;
    return result;
}
/* @} */
