
#ifndef GLOBUS_INCLUDE_GLOBUS_GSI_CREDENTIAL_H
#define GLOBUS_INCLUDE_GLOBUS_GSI_CREDENTIAL_H


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

#include <openssl/x509.h>
#include <openssl/bio.h>

#include "globus_common.h"
#include "globus_error_openssl.h"
#include "globus_gsi_cred_constants.h"

/** 
 * @defgroup globus_gsi_credential_activation Activation
 *
 * Globus GSI Credential uses standard Globus module activation and
 * deactivation.  Before any Globus GSI Credential functions are called, the
 * following function must be called:
 *
 * @code
 *      globus_module_activate(GLOBUS_GSI_CREDENTIAL_MODULE)
 * @endcode
 *
 *
 * This function returns GLOBUS_SUCCESS if Globus GSI Credential was
 * successfully initialized, and you are therefore allowed to
 * subsequently call Globus GSI Credential functions.  Otherwise, an error
 * code is returned, and Globus GSI Credential functions should not be
 * subsequently called. This function may be called multiple times.
 *
 * To deactivate Globus GSI Credential, the following function must be called:
 *
 * @code
 *    globus_module_deactivate(GLOBUS_GSI_CREDENTIAL_MODULE)
 * @endcode
 *
 * This function should be called once for each time Globus GSI Credential
 * was activated. 
 *
 */

/** Module descriptor
 * @ingroup globus_gsi_credential_activation
 * @hideinitializer
 */
#define GLOBUS_GSI_CREDENTIAL_MODULE    (&globus_i_gsi_proxy_module)

extern 
globus_module_descriptor_t              globus_i_gsi_proxy_module;


#include "globus_gsi_cred_constants.h"
extern
char *
globus_l_gsi_cred_error_strings[GLOBUS_GSI_CRED_ERROR_LAST];

/**
 * GSI Credential Type
 * @ingroup globus_gsi_credential_handle
 *
 * An enum representing a GSI Credential Type which holds info about 
 * the type of a particular credential.  The three types of credential
 * can be: GLOBUS_PROXY, GLOBUS_USER, or GLOBUS_HOST.
 * 
 * @see globus_gsi_credential_handle
 */
typedef enum 
{
    GLOBUS_PROXY,
    GLOBUS_USER,
    GLOBUS_HOST
} globus_gsi_cred_type_t;

/**
 * GSI Credential Handle.
 * @ingroup globus_gsi_cred_handle
 *
 * A GSI Credential handle keeps track of state relating to a credential.
 * Handles can have immutable @ref globus_gsi_cred_handle_attrs_t "attributes"
 * associated with them.  All credential @link globus_gsi_cred_operations
 * operations @endlink take a credential handle pointer as a parameter.
 * 
 * @see globus_gsi_cred_handle_init(),
 * globus_gsi_cred_handle_destroy(), globus_gsi_cred_handle_attrs_t
 */

typedef struct globus_l_gsi_cred_handle_s * 
                                        globus_gsi_cred_handle_t;

/**
 * Credential Handle Attributes.
 * @ingroup globus_gsi_cred_handle_attrs
 *
 * Credential handle attributes provide a set of immutable parameters
 * for a credential handle
 *
 * @see globus_gsi_cred_handle_init
 */

typedef struct globus_l_gsi_cred_handle_attrs_s *
                                        globus_gsi_cred_handle_attrs_t;

/**
 * @defgroup globus_gsi_cred_handle Credential Handle Management
 *
 * Create/Destroy/Modify a GSI Credential Handle.
 *
 * Within the Globus GSI Credential Library, all credential operations 
 * require a handle parameter.  Currenlty only one operation may be
 * in progress at once per proxy handle.
 *
 * This section defines operations to create, modify and destroy GSI
 * Credential handles.
 */

#ifndef DOXYGEN

#include <globus_error_generic.h>
#include "globus_error_openssl.h"

globus_result_t globus_gsi_cred_handle_init(
    globus_gsi_cred_handle_t *          handle,
    globus_gsi_cred_handle_attrs_t      handle_attrs);

globus_result_t globus_gsi_cred_handle_destroy(
    globus_gsi_cred_handle_t            handle);

globus_result_t globus_gsi_cred_handle_attrs_init(
    globus_gsi_cred_handle_attrs_t *    handle_attrs);

globus_result_t globus_gsi_cred_handle_attrs_destroy(
    globus_gsi_cred_handle_attrs_t      handle_attrs);

globus_result_t globus_gsi_cred_handle_attrs_copy(
    globus_gsi_cred_handle_attrs_t      a,
    globus_gsi_cred_handle_attrs_t *    b);

/* acquire a credential from a filesystem location. The search order
 * is there to allow people to specify what kind of credential should
 * looked for first. I'm not quite sure whether I like this yet.
 */

globus_result_t globus_gsi_cred_read(
    globus_gsi_cred_handle_t            handle,
    char *                              desired_subject);

globus_result_t globus_gsi_cred_read_proxy(
    globus_gsi_cred_handle_t            handle);

globus_result_t globus_gsi_cred_read_cert_and_key(
    globus_gsi_cred_handle_t            handle);


/* Read a credential from a BIO. IE: read cert, read key, read cert
 * chain.
 */

globus_result_t globus_gsi_cred_read_bio(
    globus_gsi_cred_handle_t            handle,
    BIO *                               bio);

/* Write a credential to a BIO. IE: write cert_chain, write key, write
 * cert. 
 */

globus_result_t globus_gsi_cred_write(
    globus_gsi_cred_handle_t            handle,
    BIO *                               bio);

/* Utility function that will write the credential to the standard
 * proxy file.
 */

globus_result_t globus_gsi_cred_write_proxy(
    globus_gsi_cred_handle_t            handle);

/* Determine whether the credential structure contains a proxy */

globus_result_t globus_gsi_cred_is_proxy(
    globus_gsi_cred_handle_t            handle,
    int *                               is_proxy);

/* If we can, walk the cert chain and make sure that the credential is
  * ok. Might also check that all proxy certs are wellformed.
  */

globus_result_t globus_gsi_cred_verify(
    globus_gsi_cred_handle_t            handle);

globus_result_t globus_gsi_cred_get_subject_name(
    globus_gsi_cred_handle_t            handle,
    char **                             subject_name);

globus_result_t globus_gsi_cred_get_issuer_name(
    globus_gsi_cred_handle_t            handle,
    char **                             issuer_name);

globus_result_t globus_gsi_cred_set_cert(
    globus_gsi_cred_handle_t            handle,
    X509 *                              cert);

globus_result_t globus_gsi_cred_set_key(
    globus_gsi_cred_handle_t            handle,
    EVP_PKEY *                          key);

globus_result_t globus_gsi_cred_set_cert_chain(
    globus_gsi_cred_handle_t            handle,
    STACK_OF(X509) *                    cert_chain);

globus_result_t globus_gsi_cred_get_cert(
    globus_gsi_cred_handle_t            handle,
    X509 **                             cert);

globus_result_t globus_gsi_cred_get_key(
    globus_gsi_cred_handle_t            handle,
    EVP_PKEY **                         key);

globus_result_t globus_gsi_cred_get_cert_chain(
    globus_gsi_cred_handle_t            handle,
    STACK_OF(X509) **                   cert_chain);

globus_result_t globus_gsi_cred_handle_attrs_set_ca_cert_dir(
    globus_gsi_cred_handle_attrs_t      handle_attrs,
    char *                              ca_cert_dir);


globus_result_t globus_gsi_cred_handle_attrs_get_ca_cert_dir(
    globus_gsi_cred_handle_attrs_t      handle_attrs,
    char **                             ca_cert_dir);


globus_result_t globus_gsi_cred_handle_attrs_set_proxy_file(
    globus_gsi_cred_handle_attrs_t      handle_attrs,
    char *                              proxy_file);


globus_result_t globus_gsi_cred_handle_attrs_get_proxy_file(
    globus_gsi_cred_handle_attrs_t      handle_attrs,
    char **                             proxy_file);

globus_result_t globus_gsi_cred_handle_attrs_set_cert_file(
    globus_gsi_cred_handle_attrs_t      handle_attrs,
    char *                              cert_file);


globus_result_t globus_gsi_cred_handle_attrs_get_cert_file(
    globus_gsi_cred_handle_attrs_t      handle_attrs,
    char **                             cert_file);

globus_result_t globus_gsi_cred_handle_attrs_set_key_file(
    globus_gsi_cred_handle_attrs_t      handle_attrs,
    char *                              key_file);


globus_result_t globus_gsi_cred_handle_attrs_get_key_file(
    globus_gsi_cred_handle_attrs_t      handle_attrs,
    char **                             key_file);

globus_result_t globus_gsi_cred_handle_attrs_set_search_order(
    globus_gsi_cred_handle_attrs_t      handle_attrs,
    globus_gsi_cred_type_t              search_order[]); /*{PROXY,USER,HOST}*/


globus_result_t globus_gsi_cred_handle_attrs_get_search_order(
    globus_gsi_cred_handle_attrs_t      handle_attrs,
    globus_gsi_cred_type_t *            search_order[]);/*{PROXY,USER,HOST}*/

#endif

EXTERN_C_END

#endif /* GLOBUS_INCLUDE_GLOBUS_GSI_CREDENTIAL_H */
