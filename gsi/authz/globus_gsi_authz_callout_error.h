#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file globus_gsi_authz_callout_error.h
 * @author Sam Meder
 *
 * $RCSfile$
 * $Revision$
 * $Date$
 */
#endif

#ifndef GLOBUS_GSI_AUTHZ_CALLOUT_ERROR_H
#define GLOBUS_GSI_AUTHZ_CALLOUT_ERROR_H

#ifndef EXTERN_C_BEGIN
#    ifdef __cplusplus
#        define EXTERN_C_BEGIN extern "C" {
#        define EXTERN_C_END }
#    else
#        define EXTERN_C_BEGIN
#        define EXTERN_C_END
#    endif
#endif

EXTERN_C_BEGIN

#include "globus_common.h"

/** 
 * @defgroup globus_gsi_authz_callout_error_activation Activation
 *
 * Globus GRAM Jobmanager Callout Error API uses standard Globus module
 * activation and deactivation.  Before any Globus GRAM Jobmanager Callout
 * Error API functions are called, the following function must be called:
 *
 * @code
 *      globus_module_activate(GLOBUS_GSI_AUTHZ_CALLOUT_ERROR_MODULE)
 * @endcode
 *
 *
 * This function returns GLOBUS_SUCCESS if the Globus GRAM Jobmanager Callout
 * Error API was successfully initialized, and you are therefore allowed to
 * subsequently call Globus GRAM Jobmanager Callout Error API functions.
 * Otherwise, an error code is returned, and Globus GSI Credential functions
 * should not be subsequently called. This function may be called multiple
 * times. 
 *
 * To deactivate Globus GRAM Jobmanager Callout Error API, the following
 * function must be called: 
 *
 * @code
 *    globus_module_deactivate(GLOBUS_GSI_AUTHZ_CALLOUT_ERROR_MODULE)
 * @endcode
 *
 * This function should be called once for each time Globus GRAM Jobmanager
 * Callout Error API was activated. 
 *
 */

/** Module descriptor
 * @ingroup globus_gsi_authz_callout_error_activation
 * @hideinitializer
 */
#define GLOBUS_GSI_AUTHZ_CALLOUT_ERROR_MODULE    (&globus_i_gsi_authz_callout_error_module)

extern 
globus_module_descriptor_t              globus_i_gsi_authz_callout_error_module;

/**
 * @defgroup globus_gsi_authz_callout_error_datatypes Datatypes
 */

/**
 * GRAM Jobmanager Callout Error codes
 * @ingroup globus_gsi_authz_callout_error_datatypes
 */
typedef enum
{
    /** Credentials not accepted */
    GLOBUS_GSI_AUTHZ_CALLOUT_BAD_CREDS = 0,
    /** Authorization system misconfigured */
    GLOBUS_GSI_AUTHZ_CALLOUT_AUTHZ_SYSTEM_ERROR = 1,
    /** Authorization denied */
    GLOBUS_GSI_AUTHZ_CALLOUT_AUTHZ_DENIED = 2,
    /** Authorization denied - invalid job id */
    GLOBUS_GSI_AUTHZ_CALLOUT_AUTHZ_DENIED_INVALID_JOB = 3,
    /** Authorization denied - executable not allowed */
    GLOBUS_GSI_AUTHZ_CALLOUT_AUTHZ_DENIED_BAD_EXECUTABLE = 4,
    /** Last marker - never used */
    GLOBUS_GSI_AUTHZ_CALLOUT_ERROR_LAST = 5
}
globus_gsi_authz_callout_error_t;

extern char * globus_i_gsi_authz_callout_error_strings[];

#define GLOBUS_GSI_AUTHZ_CALLOUT_ERROR(__RESULT, __TYPE, __ERRSTR) \
{                                                                        \
    char *                          _tmp_str_ =                          \
        globus_common_create_string __ERRSTR;                            \
    (__RESULT) = globus_error_put(                                       \
        globus_error_construct_error(                                    \
            GLOBUS_GSI_AUTHZ_CALLOUT_ERROR_MODULE,                 \
            (__RESULT) ? globus_error_get(__RESULT) : NULL,              \
            __TYPE,                                                      \
            "%s:%d: %s: %s%s%s",                                         \
            __FILE__, __LINE__, "GRAM Authorization Callout",            \
            globus_i_gsi_authz_callout_error_strings[__TYPE],      \
            _tmp_str_ ? ": " : "",                                       \
            _tmp_str_ ? _tmp_str_ : ""));                                \
    if(_tmp_str_) free(_tmp_str_);                                       \
}


EXTERN_C_END

#endif
