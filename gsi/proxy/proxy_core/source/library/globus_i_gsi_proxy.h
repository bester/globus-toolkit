#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file globus_i_gsi_proxy.h
 * Globus GSI Proxy Library
 * @author Sam Meder, Sam Lang
 *
 * $RCSfile$
 * $Revision$
 * $Date $
 */

#include "globus_gsi_proxy.h"
#include "proxycertinfo.h"
#include "globus_common.h"

#ifndef GLOBUS_I_INCLUDE_GSI_PROXY_H
#define GLOBUS_I_INCLUDE_GSI_PROXY_H

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

/* DEBUG MACROS */

#ifdef BUILD_DEBUG

extern int                              globus_i_gsi_proxy_debug_level;
extern FILE *                           globus_i_gsi_proxy_debug_fstream;

#define GLOBUS_I_GSI_PROXY_DEBUG(_LEVEL_) \
    (globus_i_gsi_proxy_debug_level >= (_LEVEL_))

#define GLOBUS_I_GSI_PROXY_DEBUG_FPRINTF(_LEVEL_, _MESSAGE_) \
    { \
        if (GLOBUS_I_GSI_PROXY_DEBUG(_LEVEL_)) \
        { \
           globus_libc_fprintf _MESSAGE_; \
        } \
    }

#define GLOBUS_I_GSI_PROXY_DEBUG_FNPRINTF(_LEVEL_, _MESSAGE_) \
    { \
        if (GLOBUS_I_GSI_PROXY_DEBUG(_LEVEL_)) \
        { \
           char *                          _tmp_str_ = \
               globus_gsi_cert_utils_create_nstring _MESSAGE_; \
           globus_libc_fprintf(globus_i_gsi_cert_utils_debug_fstream, \
                               _tmp_str_); \
           globus_libc_free(_tmp_str_); \
        } \
    }

#define GLOBUS_I_GSI_PROXY_DEBUG_PRINT(_LEVEL_, _MESSAGE_) \
    { \
        if (GLOBUS_I_GSI_PROXY_DEBUG(_LEVEL_)) \
        { \
           globus_libc_fprintf(globus_i_gsi_proxy_debug_fstream, _MESSAGE_); \
        } \
    }

#define GLOBUS_I_GSI_PROXY_DEBUG_PRINT_OBJECT(_LEVEL_, _OBJ_NAME_, _OBJ_) \
    { \
        if (GLOBUS_I_GSI_PROXY_DEBUG(_LEVEL_)) \
        { \
           _OBJ_NAME_##_print_fp(globus_i_gsi_proxy_debug_fstream, _OBJ_); \
        } \
    }

#else

#define GLOBUS_I_GSI_PROXY_DEBUG_FPRINTF(_LEVEL_, _MESSAGE_) {}
#define GLOBUS_I_GSI_PROXY_DEBUG_FNPRINTF(_LEVEL_, _MESSAGE_) {}
#define GLOBUS_I_GSI_PROXY_DEBUG_PRINT(_LEVEL_, _MESSAGE_) {}
#define GLOBUS_I_GSI_PROXY_DEBUG_PRINT_OBJECT(_LEVEL_, _OBJ_NAME_, _OBJ_) {}

#endif

#define GLOBUS_I_GSI_PROXY_DEBUG_ENTER \
            GLOBUS_I_GSI_PROXY_DEBUG_FPRINTF( \
                1, (globus_i_gsi_proxy_debug_fstream, \
                    "%s entering\n", _function_name_))

#define GLOBUS_I_GSI_PROXY_DEBUG_EXIT \
            GLOBUS_I_GSI_PROXY_DEBUG_FPRINTF( \
                1, (globus_i_gsi_proxy_debug_fstream, \
                    "%s exiting\n", _function_name_))

/* ERROR MACROS */

#define GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(_RESULT_, \
                                              _ERRORTYPE_, _ERRORSTR_) \
    char *                              _tmp_string_ = \
        globus_gsi_cert_utils_create_string _ERRORSTR_; \
    _RESULT_ = globus_i_gsi_proxy_openssl_error_result( \
        _ERRORTYPE_, \
        __FILE__, \
        _function_name_, \
        __LINE__, \
        _tmp_string_); \
    globus_libc_free(_tmp_string_);

#define GLOBUS_GSI_PROXY_ERROR_RESULT(_RESULT_, \
                                      _ERRORTYPE_, _ERRORSTR_) \
    char *                              _tmp_string_ = \
        globus_gsi_cert_utils_create_string _ERRORSTR_; \
    _RESULT_ = globus_i_gsi_proxy_error_result( \
        _ERRORTYPE_, \
        __FILE__, \
        _function_name_, \
        __LINE__, \
        _tmp_string_); \
    globus_libc_free(_tmp_string_);

#define GLOBUS_GSI_PROXY_ERROR_CHAIN_RESULT(_RESULT_, \
                                            _ERRORTYPE_) \
    _RESULT_ = globus_i_gsi_proxy_error_chain_result( \
        _RESULT_, \
        _ERRORTYPE_, \
        __FILE__, \
        _function_name_, \
        __LINE__, \
        NULL);

#include "globus_gsi_proxy_constants.h"

/**
 * Handle attributes.
 * @ingroup globus_gsi_credential_handle_attrs
 */

/**
 * GSI Proxy handle attributes implementation
 * @ingroup globus_gsi_proxy_handle
 * @internal
 *
 * This structure contains the attributes
 * of a proxy handle.
 */
typedef struct globus_l_gsi_proxy_handle_attrs_s
{
    /** 
     * The size of the keys to generate for
     * the certificate request
     */
    int                                 key_bits;
    /**
     * The initial prime to use for creating
     * the key pair
     */
    int                                 init_prime;
    /**
     * The signing algorithm to use for 
     * generating the proxy certificate
     */
    EVP_MD *                            signing_algorithm;
    /**
     * The number of minutes the proxy certificate
     * is valid for
     */
    int                                 time_valid;
    /**
     * The clock skew (in seconds) allowed 
     * for the proxy certificate.  The skew
     * adjusts the validity time of the proxy cert.
     */
    int                                 clock_skew;
    /**
     * The callback for the creation of the public/private key
     * pair.
     */
    void (*key_gen_callback)(int, int, void *);

} globus_i_gsi_proxy_handle_attrs_t;

/**
 * GSI Proxy handle implementation
 * @ingroup globus_gsi_proxy_handle
 * @internal
 *
 * This structure contains all of the state associated with a proxy
 * handle.
 *
 * @see globus_proxy_handle_init(), globus_proxy_handle_destroy()
 */

typedef struct globus_l_gsi_proxy_handle_s
{
    /** The proxy request */
    X509_REQ *                          req;
    /** The proxy private key */
    EVP_PKEY *                          proxy_key;
    /** Proxy handle attributes */
    globus_gsi_proxy_handle_attrs_t     attrs;
    /** Flag for whether the proxy is limited or not */
    globus_bool_t                       is_limited;
    /** The proxy cert info extension used in the operations */
    PROXYCERTINFO *                     proxy_cert_info;    

} globus_i_gsi_proxy_handle_t;


/* used for printing the status of a private key generating algorithm */
void 
globus_i_gsi_proxy_create_private_key_cb(
    int                                 num1,
    int                                 num2,
    BIO *                               output);

globus_result_t
globus_i_gsi_proxy_set_pc_times(
    X509 *                              new_pc, 
    X509 *                              issuer_cert,
    int                                 clock_skew,
    int                                 time_valid);

globus_result_t
globus_i_gsi_proxy_set_subject(
    X509 *                              new_pc, 
    X509 *                              issuer_cert,
    char *                              common_name);

globus_result_t
globus_i_gsi_proxy_openssl_error_result(
    int                                 error_type,
    const char *                        filename,
    const char *                        function_name,
    int                                 line_number,
    const char *                        long_desc);

globus_result_t
globus_i_gsi_proxy_error_result(
    int                                 error_type,
    const char *                        filename,
    const char *                        function_name,
    int                                 line_number,
    const char *                        long_desc);

globus_result_t
globus_i_gsi_proxy_error_chain_result(
    globus_result_t                     chain_result,
    int                                 error_type,
    const char *                        filename,
    const char *                        function_name,
    int                                 line_number,
    const char *                        long_desc);

EXTERN_C_END

#endif /* GLOBUS_I_INCLUDE_GSI_PROXY_H */

#endif /* GLOBUS_DONT_DOCUMENT_INTERNAL */
