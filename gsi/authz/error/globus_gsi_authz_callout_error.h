#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file globus_gsi_authz_callout_error.h
 *
 * $RCSfile$
 * $Revision$
 * $Date$
 */

/*
 * Basically copied from globus_gram_jobmanager_callout_error.h
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
 * @defgroup globus_gsi_authz_callout_error_datatypes Datatypes
 */

/**
 * Error codes
 * @ingroup globus_gsi_authz_callout_error_datatypes
 */
typedef enum
{
    GLOBUS_GSI_AUTHZ_CALLOUT_AUTHZ_CALLOUT_ERROR = 0,
    GLOBUS_GSI_AUTHZ_CALLOUT_AUTHZ_DENIED_BY_CALLOUT = 1,
    GLOBUS_GSI_AUTHZ_CALLOUT_CONFIGURATION_ERROR = 2,
    GLOBUS_GSI_AUTHZ_CALLOUT_SYSTEM_ERROR = 3,
    GLOBUS_GSI_AUTHZ_CALLOUT_CREDENTIAL_ERROR = 4,
    GLOBUS_GSI_AUTHZ_CALLOUT_BAD_ARGUMENT_ERROR = 5,
    GLOBUS_GSI_AUTHZ_CALLOUT_ERROR_LAST = 6,
}
globus_gsi_authz_callout_error_t;

extern globus_module_descriptor_t globus_gsi_authz_callout_error_module;

#define GLOBUS_GSI_AUTHZ_CALLOUT_ERROR_MODULE &globus_gsi_authz_callout_error_module

extern char * globus_gsi_authz_callout_error_strings[];

#define GLOBUS_GSI_AUTHZ_CALLOUT_ERROR(__RESULT, __TYPE, __ERRSTR) \
{                                                                        \
    char *                          _tmp_str_ =                          \
        globus_common_create_string (__ERRSTR);                            \
    (__RESULT) = globus_error_put(                                       \
        globus_error_construct_error(                                    \
            GLOBUS_GSI_AUTHZ_CALLOUT_ERROR_MODULE,                 \
            (__RESULT) ? globus_error_get(__RESULT) : NULL,              \
            __TYPE,                                                      \
            "%s:%d: %s: %s%s%s",                                         \
            __FILE__, __LINE__, "Authz Callout",            \
            globus_gsi_authz_callout_error_strings[__TYPE],      \
            _tmp_str_ ? ": " : "",                                       \
            _tmp_str_ ? _tmp_str_ : ""));                                \
    if(_tmp_str_) free(_tmp_str_);                                       \
}

#define GLOBUS_GSI_AUTHZ_CALLOUT_ERRNO_ERROR(__RESULT, __ERRNO) \
{                                                                        \
    (__RESULT) = globus_error_put(                                       \
        globus_error_construct_errno_error(                                    \
            GLOBUS_GSI_AUTHZ_CALLOUT_ERROR_MODULE,                 \
            (__RESULT) ? globus_error_get(__RESULT) : NULL,              \
            __ERRNO));							\
}


EXTERN_C_END

#endif
