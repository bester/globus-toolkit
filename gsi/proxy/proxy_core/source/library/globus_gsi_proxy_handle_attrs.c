#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file globus_gsi_proxy_handle_attrs.c
 *
 * $RCSfile$
 * $Revision$
 * $Date$
 */
#endif

#include "globus_i_gsi_proxy.h"
#include <errno.h>

#define DEFAULT_KEY_BITS                512
#define DEFAULT_PUB_EXPONENT            RSA_F4  /* 65537 */
#define DEFAULT_SIGNING_ALGORITHM       EVP_md5()
#define DEFAULT_TIME_VALID              (12*60)   /* actually in minutes */
#define DEFAULT_CLOCK_SKEW              (5*60)    /* actually in seconds */

#define GLOBUS_GSI_PROXY_HANDLE_ATTRS_MALLOC_ERROR \
    globus_error_put(globus_error_wrap_errno_error( \
        GLOBUS_GSI_PROXY_MODULE, \
        errno, \
        GLOBUS_GSI_PROXY_ERROR_ERRNO, \
        "%s:%d: Could not allocate enough memory: %d bytes", \
        __FILE__, __LINE__, len))

/**
 * @name Initialize
 */
/*@{*/
/**
 * Initialize GSI Proxy Handle Attributes.
 * @ingroup globus_gsi_proxy_handle_attrs
 *
 * Initialize proxy handle attributes, which
 * can (and should) be associated with a proxy handle.
 * For most purposes, these attributes should primarily
 * be used by the proxy handle. 
 *
 * Currently, no attibute values are initialized.
 *
 * @param handle_attrs
 *        The handle attributes structure to be initialized
 * @return
 *        GLOBUS_SUCCESS
 *
 * @see globus_gsi_proxy_handle_attrs_destroy()
 */
globus_result_t
globus_gsi_proxy_handle_attrs_init(
    globus_gsi_proxy_handle_attrs_t *   handle_attrs)
{
    globus_result_t                     result;
    globus_gsi_proxy_handle_attrs_t     attrs;
    int                                 len;
    static char *                       _function_name_ =
        "globus_gsi_proxy_handle_attrs_init";

    GLOBUS_I_GSI_PROXY_DEBUG_ENTER;

    if(handle_attrs == NULL)
    {
        GLOBUS_GSI_PROXY_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE_ATTRS,
            ("NULL handle attributes passed to function: %s", 
             _function_name_));
        goto exit;
    }

    len = sizeof(globus_i_gsi_proxy_handle_attrs_t);
    if((*handle_attrs = (globus_gsi_proxy_handle_attrs_t)
       malloc(len)) == NULL)
    {
        result = GLOBUS_GSI_PROXY_HANDLE_ATTRS_MALLOC_ERROR;
        goto exit;
    }

    attrs = *handle_attrs;

    attrs->key_bits = DEFAULT_KEY_BITS;
    attrs->init_prime = DEFAULT_PUB_EXPONENT;
    attrs->signing_algorithm = DEFAULT_SIGNING_ALGORITHM;
    attrs->time_valid = DEFAULT_TIME_VALID;
    attrs->clock_skew = DEFAULT_CLOCK_SKEW;
    attrs->key_gen_callback = NULL;
    
    result = GLOBUS_SUCCESS;
   
 exit:

    GLOBUS_I_GSI_PROXY_DEBUG_EXIT;
    return result;
}
/* globus_gsi_proxy_handle_init() */
/*@}*/

/**
 * @name Destroy
 */
/* @{ */
/**
 * Destroy the GSI Proxy handle attributes
 * @ingroup globus_gsi_proxy_handle_attrs
 *
 * @param handle_attrs
 *        The handle attributes to be destroyed.
 * @return 
 *        GLOBUS_SUCCESS
 *
 * @see globus_gsi_proxy_handle_attrs_init()
 */
globus_result_t
globus_gsi_proxy_handle_attrs_destroy(
    globus_gsi_proxy_handle_attrs_t     handle_attrs)
{
    static char *                       _function_name_ =
        "globus_gsi_proxy_handle_attrs_destroy";
    
    GLOBUS_I_GSI_PROXY_DEBUG_ENTER;

    if(handle_attrs != NULL)
    {
        globus_libc_free(handle_attrs);
        handle_attrs = NULL;
    }

    GLOBUS_I_GSI_PROXY_DEBUG_EXIT;
    return GLOBUS_SUCCESS;
}
/* globus_gsi_proxy_handle_destroy() */
/*@}*/

/**
 * @name Set Key Bits
 */
/* @{ */
/**
 * Set the length of the public key pair
 * used by the proxy certificate
 * @ingroup globus_gsi_proxy_handle_attrs
 *
 * @param handle_attrs 
 *        the attributes to set
 * @param bits
 *        the length to set it to (usually 1024)
 *
 * @return 
 *        GLOBUS_SUCCESS
 */
globus_result_t
globus_gsi_proxy_handle_attrs_set_keybits(
    globus_gsi_proxy_handle_attrs_t     handle_attrs,
    int                                 bits)
{
    static char *                       _function_name_ =
        "globus_gsi_proxy_handle_attrs_set_keybits";

    GLOBUS_I_GSI_PROXY_DEBUG_ENTER;

    handle_attrs->key_bits = bits;
    
    GLOBUS_I_GSI_PROXY_DEBUG_EXIT;
    return GLOBUS_SUCCESS;
}
/* @} */


/**
 * @name Get Key Bits
 */
/* @{ */
/**
 * Gets the length of the public key pair used by
 * the proxy certificate
 * @ingroup globus_gsi_proxy_handle_attrs
 *
 * @param handle_attrs
 *        the attributes to get the key length from
 * @param bits
 *        the length of the key pair in bits
 * @return
 *        GLOBUS_SUCCESS
 */
globus_result_t
globus_gsi_proxy_handle_attrs_get_keybits(
    globus_gsi_proxy_handle_attrs_t     handle_attrs,
    int *                               bits)
{
    static char *                       _function_name_ =
        "globus_gsi_proxy_handle_attrs_get_keybits";

    GLOBUS_I_GSI_PROXY_DEBUG_ENTER;

    *bits = handle_attrs->key_bits;

    GLOBUS_I_GSI_PROXY_DEBUG_EXIT;
    return GLOBUS_SUCCESS;
}
/* @} */

/**
 * @name Set Initial Prime Number
 */
/* @{ */
/**
 * Set the initial prime number used for
 * generating public key pairs in the RSA
 * algorithm
 * @ingroup globus_gsi_proxy_handle_attrs
 *
 * @param handle_attrs
 *        The attributes to set
 * @param prime
 *        The prime number to set it to
 *        This value needs to be a prime number
 * @return 
 *        GLOBUS_SUCCESS
 */
globus_result_t
globus_gsi_proxy_handle_attrs_set_init_prime(
    globus_gsi_proxy_handle_attrs_t     handle_attrs,
    int                                 prime)
{
    static char *                       _function_name_ =
        "globus_gsi_proxy_handle_attrs_set_init_prime";

    GLOBUS_I_GSI_PROXY_DEBUG_ENTER;

    handle_attrs->init_prime = prime;

    GLOBUS_I_GSI_PROXY_DEBUG_EXIT;
    return GLOBUS_SUCCESS;
};
/* @} */


/**
 * @name Get Initial Prime Number
 */
/* @{ */
/**
 * Get the initial prime number used for
 * generating the public key pair in the
 * RSA algorithm
 * @ingroup globus_gsi_proxy_handle_attrs
 *
 * @param handle_attrs
 *        The attributes to get the initial
 *        prime number from
 * @param prime
 *        The initial prime number taken from the
 *        attributes
 * @return
 *        GLOBUS_SUCCESS
 */
globus_result_t
globus_gsi_proxy_handle_attrs_get_init_prime(
    globus_gsi_proxy_handle_attrs_t     handle_attrs,
    int *                               prime)
{
    static char *                       _function_name_ =
        "globus_gsi_proxy_handle_attrs_get_init_prime";

    GLOBUS_I_GSI_PROXY_DEBUG_ENTER;

    *prime = handle_attrs->init_prime;
    
    GLOBUS_I_GSI_PROXY_DEBUG_EXIT;
    return GLOBUS_SUCCESS;
};
/* @} */

/**
 * @name Set Signing Algorithm
 */
/* @{ */
/**
 * Sets the Signing Algorithm to be used to sign
 * the certificate request.  In most cases, the
 * signing party will ignore this value, and sign
 * with an algorithm of its choice.
 * @ingroup globus_gsi_proxy_handle
 *
 * @param handle_attrs
 *        The proxy handle to set the signing algorithm of
 * @param algorithm
 *        The signing algorithm to set 
 * @return
 *        Returns 
 *        GLOBUS_SUCCESS if the handle is valid, otherwise
 *        an error object is returned.
 */
globus_result_t
globus_gsi_proxy_handle_attrs_set_signing_algorithm(
    globus_gsi_proxy_handle_attrs_t     handle_attrs,
    EVP_MD *                            algorithm)
{
    globus_result_t                     result;
    static char *                       _function_name_ =
        "globus_gsi_proxy_handle_attrs_set_signing_algorithm";

    GLOBUS_I_GSI_PROXY_DEBUG_ENTER;

    if(handle_attrs == NULL)
    {
        GLOBUS_GSI_PROXY_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE_ATTRS,
            ("NULL handle attributes passed to function: %s", 
             _function_name_));
        goto exit;
    }

    handle_attrs->signing_algorithm = algorithm;

    result = GLOBUS_SUCCESS;

 exit:

    GLOBUS_I_GSI_PROXY_DEBUG_EXIT;
    return result;
};
/* @} */


/**
 * @name Set Signing Algorithm
 */
/* @{ */
/**
 * Sets the Signing Algorithm to be used to sign
 * the certificate request.  In most cases, the
 * signing party will ignore this value, and sign
 * with an algorithm of its choice.
 * @ingroup globus_gsi_proxy_handle
 *
 * @param handle_attrs
 *        The proxy handle_attrs to set the signing algorithm of
 * @param algorithm
 *        The signing algorithm to set 
 * @return
 *        Returns 
 *        GLOBUS_SUCCESS if the handle is valid, otherwise
 *        an error object is returned.
 */
globus_result_t
globus_gsi_proxy_handle_attrs_get_signing_algorithm(
    globus_gsi_proxy_handle_attrs_t     handle_attrs,
    EVP_MD **                           algorithm)
{
    globus_result_t                     result;
    static char *                       _function_name_ =
        "globus_gsi_proxy_handle_attrs_get_signing_algorithm";

    GLOBUS_I_GSI_PROXY_DEBUG_ENTER;

    if(handle_attrs == NULL)
    {
        GLOBUS_GSI_PROXY_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE_ATTRS,
            ("NULL handle attributes passed to function: %s",
             _function_name_));
        goto exit;
    }

    *algorithm = handle_attrs->signing_algorithm;

    result = GLOBUS_SUCCESS;

 exit:
    
    GLOBUS_I_GSI_PROXY_DEBUG_EXIT;
    return result;
};
/* @} */


/**
 * @name Set Minutes Valid
 */
/* @{ */
/**
 * Set the number of minutes the proxy certificate
 * is valid for.  This is only a suggestion
 * for the signer, who can accept or reject
 * @ingroup globus_gsi_proxy_handle
 *
 * @param handle_attrs
 *        The handle_attrs containing the minutes valid field to be set
 * @param minutes
 *        The valid minutes the proxy cert has before expiring.
 * @return 
 *        GLOBUS_SUCCESS if the handle is valid, otherwise 
 *        an error is returned.
 */
globus_result_t
globus_gsi_proxy_handle_attrs_set_time_valid(
    globus_gsi_proxy_handle_attrs_t     handle_attrs,
    int                                 time_valid)
{
    globus_result_t                     result;
    static char *                       _function_name_ =
        "globus_gsi_proxy_handle_attrs_set_time_valid";

    GLOBUS_I_GSI_PROXY_DEBUG_ENTER;

    if(handle_attrs == NULL)
    {
        GLOBUS_GSI_PROXY_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE_ATTRS,
            ("NULL handle attributes passed to function: %s", 
             _function_name_));
        goto exit;
    }
    handle_attrs->time_valid = time_valid;
    
    result = GLOBUS_SUCCESS;

 exit:

    GLOBUS_I_GSI_PROXY_DEBUG_EXIT;
    return result;
};
/* @} */


/**
 * @name Get Minutes Valid
 */
/* @{ */
/**
 * Get the number of minutes this proxy certificate
 * will be valid for when signed, assuming the
 * signer accepts that length of time.
 * @ingroup globus_gsi_proxy_handle
 *
 * @param handle_attrs
 *        The handle attributes containing the valid minutes to get
 * @param minutes
 *        The number of minutes this certificate will be
 *        valid for when signed
 * @return
 *        GLOBUS_SUCCESS if handle is valid, otherwise
 *        an error is returned
 */
globus_result_t
globus_gsi_proxy_handle_attrs_get_time_valid(
    globus_gsi_proxy_handle_attrs_t     handle_attrs,
    int *                               time_valid)
{
    globus_result_t                     result;
    static char *                       _function_name_ =
        "globus_gsi_proxy_handle_attrs_get_time_valid";

    GLOBUS_I_GSI_PROXY_DEBUG_ENTER;

    if(handle_attrs == NULL)
    {
        GLOBUS_GSI_PROXY_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE_ATTRS,
            ("NULL handle attributes passed to function: %s",
             _function_name_));
        goto exit;
    }
    *time_valid = handle_attrs->time_valid;

    result = GLOBUS_SUCCESS;

 exit:

    GLOBUS_I_GSI_PROXY_DEBUG_EXIT;
    return result;
}
/* @} */


/**
 * @name Set Clock Skew Allowable
 */
/* @{ */
/**
 * Sets the clock skew in minutes of the proxy cert request
 * so that time differences between hosts won't
 * cause problems.  This value defaults to 5 minutes.
 * @ingroup globus_gsi_proxy_handle
 *
 * @param handle_attrs
 *        the handle_attrs containing the clock skew to be set
 * @param skew
 *        the amount to skew by (in seconds)
 * @return 
 *        GLOBUS_SUCCESS if the handle_attrs is valid - otherwise an
 *        error is returned.
 */
globus_result_t
globus_gsi_proxy_handle_attrs_set_clock_skew_allowable(
    globus_gsi_proxy_handle_attrs_t     handle_attrs,
    int                                 skew)
{
    globus_result_t                     result;
    static char *                       _function_name_ =
        "globus_gsi_proxy_handle_attrs_set_clock_skew_allowable";

    GLOBUS_I_GSI_PROXY_DEBUG_ENTER;

    if(handle_attrs == NULL)
    {
        GLOBUS_GSI_PROXY_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE_ATTRS,
            ("NULL handle attributes passed to function: %s",
             _function_name_));
        goto exit;
    }
    handle_attrs->clock_skew = skew;
    result = GLOBUS_SUCCESS;

 exit:

    GLOBUS_I_GSI_PROXY_DEBUG_EXIT;
    return result;
};
/* @} */

/**
 * @name Get Clock Skew Allowable
 */
/* @{ */
/**
 * Get the allowable clock skew for the proxy certificate
 * @ingroup globus_gsi_proxy_handle
 *
 * @param handle_attrs
 *        The handle_attrs to get the clock skew from
 * @param skew
 *        The allowable clock skew (in seconds)
 *        to get from the proxy certificate
 *        request.  This value gets set by the function, so it needs
 *        to be a pointer.
 * @return
 *        GLOBUS_SUCCESS if the handle_attrs is valid, otherwise an error
 *        is returned
 */
globus_result_t
globus_gsi_proxy_handle_attrs_get_clock_skew_allowable(
    globus_gsi_proxy_handle_attrs_t     handle_attrs,
    int *                               skew)
{
    globus_result_t                     result;
    static char *                       _function_name_ =
        "globus_gsi_proxy_handle_attrs_get_clock_skew_allowable";

    GLOBUS_I_GSI_PROXY_DEBUG_ENTER;

    if(handle_attrs == NULL)
    {
        GLOBUS_GSI_PROXY_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE_ATTRS,
            ("NULL handle attributes passed to function: %s",
             _function_name_));
        goto exit;
    }
    *skew = handle_attrs->clock_skew;
    result = GLOBUS_SUCCESS;

 exit:
    
    GLOBUS_I_GSI_PROXY_DEBUG_EXIT;
    return result;
};
/* @} */

/**
 * @name Get Key Gen Callback 
 */
/* @{ */
/**
 * Get the public/private key generation callback that provides status
 * during the generation of the keys
 * 
 * @ingroup globus_gsi_proxy_handle
 *
 * @param handle_attrs
 *        The handle_attrs to get the callback from
 * @param callback
 *        The callback from the handle attributes
 *
 * @return
 *        GLOBUS_SUCCESS if the handle_attrs is valid, otherwise an error
 *        is returned
 */
globus_result_t
globus_gsi_proxy_handle_attrs_get_key_gen_callback(
    globus_gsi_proxy_handle_attrs_t     handle_attrs,
    void                                (**callback)(int, int, void *))
{
    globus_result_t                     result;
    static char *                       _function_name_ =
        "globus_gsi_proxy_handle_attrs_get_clock_skew_allowable";

    GLOBUS_I_GSI_PROXY_DEBUG_ENTER;

    if(handle_attrs == NULL)
    {
        GLOBUS_GSI_PROXY_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE_ATTRS,
            ("NULL handle attributes passed to function: %s",
             _function_name_));
        goto exit;
    }
    *callback = handle_attrs->key_gen_callback;
    result = GLOBUS_SUCCESS;

 exit:
    
    GLOBUS_I_GSI_PROXY_DEBUG_EXIT;
    return result;
};
/* @} */

/**
 * @name Set Key Gen Callback 
 */
/* @{ */
/**
 * Set the public/private key generation callback that provides status
 * during the generation of the keys
 * 
 * @ingroup globus_gsi_proxy_handle
 *
 * @param handle_attrs
 *        The handle_attrs to get the callback from
 * @param callback
 *        The callback from the handle attributes
 *
 * @return
 *        GLOBUS_SUCCESS if the handle_attrs is valid, otherwise an error
 *        is returned
 */
globus_result_t
globus_gsi_proxy_handle_attrs_set_key_gen_callback(
    globus_gsi_proxy_handle_attrs_t     handle_attrs,
    void                                (*callback)(int, int, void *))
{
    globus_result_t                     result;
    static char *                       _function_name_ =
        "globus_gsi_proxy_handle_attrs_set_clock_skew_allowable";

    GLOBUS_I_GSI_PROXY_DEBUG_ENTER;

    if(handle_attrs == NULL)
    {
        GLOBUS_GSI_PROXY_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE_ATTRS,
            ("NULL handle attributes passed to function: %s",
             _function_name_));
        goto exit;
    }
    handle_attrs->key_gen_callback = callback;
    result = GLOBUS_SUCCESS;

 exit:
    
    GLOBUS_I_GSI_PROXY_DEBUG_EXIT;
    return result;
};
/* @} */

/**
 * @name Copy Attributes
 */
/*@{*/
/**
 * Make a copy of GSI Proxy handle attributes
 * @ingroup globus_gsi_proxy_handle_attrs
 *
 * @param a 
 *        The handle attributes to copy
 * @param b 
 *        The copy
 * @return
 *        GLOBUS_SUCCESS
 */
globus_result_t
globus_gsi_proxy_handle_attrs_copy(
    globus_gsi_proxy_handle_attrs_t     a,
    globus_gsi_proxy_handle_attrs_t *   b)
{
    globus_result_t                     result;
    static char *                       _function_name_ =
        "globus_gsi_proxy_handle_attrs_copy";
    
    GLOBUS_I_GSI_PROXY_DEBUG_ENTER;

    if(a == NULL)
    {
        GLOBUS_GSI_PROXY_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE_ATTRS,
            ("NULL handle attributes passed to function: %s",
             _function_name_));
        goto error_exit;
    }
    if(b == NULL)
    {
        GLOBUS_GSI_PROXY_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE_ATTRS,
            ("NULL handle attributes passed to function: %s",
             _function_name_));
        goto error_exit;
    }

    result = globus_gsi_proxy_handle_attrs_init(b);
    if(result != GLOBUS_SUCCESS)
    {
        GLOBUS_GSI_PROXY_ERROR_CHAIN_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE_ATTRS);
        goto error_exit;
    }

    (*b)->key_bits = a->key_bits;
    (*b)->init_prime = a->init_prime;
    (*b)->signing_algorithm = a->signing_algorithm;
    (*b)->time_valid = a->time_valid;
    (*b)->clock_skew = a->clock_skew;
    (*b)->key_gen_callback = a->key_gen_callback;

    result = GLOBUS_SUCCESS;
    goto exit;

 error_exit:

    if(*b)
    {
        globus_gsi_proxy_handle_attrs_destroy(*b);
    }

 exit:

    GLOBUS_I_GSI_PROXY_DEBUG_EXIT;
    return result;
}
/* globus_gsi_proxy_handle_attrs_copy() */
/*@}*/

