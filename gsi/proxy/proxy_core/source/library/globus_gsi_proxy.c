#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file globus_gsi_proxy.c
 * @author Sam Lang, Sam Meder
 *
 * $RCSfile$
 * $Revision$
 * $Date$
 */
#endif

#define PROXY_NAME                      "proxy"
#define LIMITED_PROXY_NAME              "limited proxy"

#include "globus_i_gsi_proxy.h"
#include "globus_gsi_proxy_constants.h"
#include "version.h"
#include "globus_error_openssl.h"
#include "globus_openssl.h"
#include "proxycertinfo.h"

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL

#define GLOBUS_GSI_PROXY_MALLOC_ERROR(_LENGTH_) \
    globus_error_put(globus_error_wrap_errno_error( \
        GLOBUS_GSI_PROXY_MODULE, \
        errno, \
        GLOBUS_GSI_PROXY_ERROR_ERRNO, \
        __FILE__, \
        _function_name_, \
        __LINE__, \
        "Could not allocate enough memory: %d bytes", \
        _LENGTH_))

static int globus_l_gsi_proxy_activate(void);
static int globus_l_gsi_proxy_deactivate(void);

/**
 * Module descriptor static initializer.
 */
globus_module_descriptor_t		globus_i_gsi_proxy_module =
{
    "globus_gsi_proxy",
    globus_l_gsi_proxy_activate,
    globus_l_gsi_proxy_deactivate,
    GLOBUS_NULL,
    GLOBUS_NULL,
    &local_version
};

int                                     globus_i_gsi_proxy_debug_level;
FILE *                                  globus_i_gsi_proxy_debug_fstream;

/**
 * Module activation
 */
static
int
globus_l_gsi_proxy_activate(void)
{
    char *                              tmpstring = NULL;
    int                                 result = (int) GLOBUS_SUCCESS;
    static char *                       _function_name_ =
        "globus_l_gsi_proxy_activate";

    /* set the debug level */
    tmpstring = globus_module_getenv("GLOBUS_GSI_PROXY_DEBUG_LEVEL");
    
    if(tmpstring != GLOBUS_NULL)
    {
        globus_i_gsi_proxy_debug_level = atoi(tmpstring);

        if(globus_i_gsi_proxy_debug_level < 0)
        {
            globus_i_gsi_proxy_debug_level = 0;
        }
    }

    /* set the location for the debugging for the 
     * debugging output (file or stderr)
     */
    tmpstring = globus_module_getenv("GLOBUS_GSI_PROXY_DEBUG_FILE");
    if(tmpstring != GLOBUS_NULL)
    {
        globus_i_gsi_proxy_debug_fstream = fopen(tmpstring, "a");
        if(globus_i_gsi_proxy_debug_fstream == NULL)
        {
            result = (int) GLOBUS_FAILURE;
            goto exit;
        }
    }
    else
    {
        /* if the env. var isn't set we use stderr */
        globus_i_gsi_proxy_debug_fstream = stderr;
    }

    GLOBUS_I_GSI_PROXY_DEBUG_ENTER;

    if(globus_i_gsi_proxy_debug_level > 7)
    {
        CRYPTO_malloc_debug_init();
        CRYPTO_dbg_set_options(V_CRYPTO_MDEBUG_ALL);
        CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);
    }

    result = globus_module_activate(GLOBUS_OPENSSL_MODULE);
    if(result != GLOBUS_SUCCESS)
    {
        goto exit;
    }
    
    result = globus_module_activate(GLOBUS_GSI_CREDENTIAL_MODULE);
    if(result != GLOBUS_SUCCESS)
    {
        goto exit;
    }

    GLOBUS_I_GSI_PROXY_DEBUG_EXIT;

 exit:
    return result;
}

/**
 * Module deactivation
 *
 */
static
int
globus_l_gsi_proxy_deactivate(void)
{
    int                                 result = GLOBUS_SUCCESS;
    static char *                       _function_name_ = 
        "globus_i_gsi_proxy_deactivate";

    GLOBUS_I_GSI_PROXY_DEBUG_ENTER;

    X509V3_EXT_cleanup();

    globus_module_deactivate(GLOBUS_OPENSSL_MODULE);

    globus_module_deactivate(GLOBUS_GSI_CREDENTIAL_MODULE);

    GLOBUS_I_GSI_PROXY_DEBUG_EXIT;

    if(globus_i_gsi_proxy_debug_fstream != stderr)
    {
        fclose(globus_i_gsi_proxy_debug_fstream);
    }

    return result;
}
/* globus_l_gsi_proxy_deactivate() */

#endif /* GLOBUS_DONT_DOCUMENT_INTERNAL */
    
/**
 * @name Create Request
 * @ingroup globus_gsi_proxy_operations
 */
/*@{*/
/**
 * Create a proxy credential request
 *
 * This function creates a proxy credential request, ie. a unsigned 
 * certificate and the corresponding private key, based on the handle
 * that is passed in.
 * The public part of the request is written to the BIO supplied in
 * the output_bio parameter.  After the request is written, the
 * PROXYCERTINFO extension contained in the handle is written
 * to the BIO. 
 * The proxy handle is updated with the private key.
 *
 * @param handle
 *        A GSI Proxy handle to use for the request operation.
 * @param output_bio
 *        A BIO to write the resulting request structure to.
 * @return
 *        GLOBUS_SUCCESS unless an error occurred, in which case, 
 *        a globus error object ID is returned
 */
globus_result_t
globus_gsi_proxy_create_req(
    globus_gsi_proxy_handle_t           handle,
    BIO *                               output_bio)
{
    X509_NAME *                         req_name = NULL;
    X509_NAME_ENTRY *                   req_name_entry = NULL;
    RSA *                               rsa_key = NULL;
    globus_result_t                     result;

    static char *                       _function_name_ =
        "globus_gsi_proxy_create_req";

    GLOBUS_I_GSI_PROXY_DEBUG_ENTER;
        
    if(handle == NULL)
    {
        GLOBUS_GSI_PROXY_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE,
            ("NULL handle passed to function: %s", _function_name_));
        goto exit;
    }

    if(output_bio == NULL)
    {
        GLOBUS_GSI_PROXY_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_BIO,
            ("NULL bio passed to function: %s", _function_name_));
        goto exit;
    }

    if(handle->proxy_key)
    {
        GLOBUS_GSI_PROXY_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_PRIVATE_KEY,
            ("The handle's private key has already been initialized"));
        goto exit;
    }

    /* initialize the private key */
    if((handle->proxy_key = EVP_PKEY_new()) == NULL)
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_PRIVATE_KEY,
            ("Couldn't create new private key structure for handle"));
        goto exit;
    }

    /* First, generate and setup private/public key pair */
    rsa_key = RSA_generate_key(handle->attrs->key_bits, 
                               handle->attrs->init_prime, 
                               handle->attrs->key_gen_callback, 
                               NULL);

    if(rsa_key == NULL)
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_PRIVATE_KEY, 
            ("Couldn't generate RSA key pair for proxy handle"));
        goto exit;
    }

    if(!EVP_PKEY_assign_RSA(handle->proxy_key, rsa_key))
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_PRIVATE_KEY,
            ("Could not set private key in proxy handle"));
        goto error_exit;
    }

    if(!X509_REQ_set_version(handle->req, 0L))
    {
        GLOBUS_GSI_PROXY_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_X509_REQ,
            ("Could not set version of X509 request in proxy handle"));
        goto error_exit;
    }

    if(!X509_REQ_set_pubkey(handle->req, handle->proxy_key))
    {
        GLOBUS_GSI_PROXY_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_X509_REQ,
            ("Couldn't set public key of X509 request in proxy handle"));
        goto error_exit;
    }

    req_name = X509_NAME_new();
    if(!req_name)
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_X509_REQ,
            ("Couldn't create a new X509_NAME for "
             "the proxy certificate request"));
        goto error_exit;
    }

    req_name_entry = X509_NAME_ENTRY_create_by_NID(
        NULL, 
        NID_commonName,
        V_ASN1_APP_CHOOSE,
        (unsigned char *) "NULL SUBJECT NAME ENTRY",
        -1);
    if(!req_name_entry)
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_X509_REQ,
            ("Couldn't create a new X509_NAME_ENTRY for "
             "the proxy certificate request"));
        goto error_exit;
    }

    if(!X509_NAME_add_entry(req_name,
                            req_name_entry,
                            X509_NAME_entry_count(req_name),
                            0))
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_X509_REQ,
            ("Couldn't add the X509_NAME_ENTRY to the "
             "proxy certificate request's subject name"));
        goto error_exit;
    }

    if(req_name_entry)
    {
        X509_NAME_ENTRY_free(req_name_entry);
        req_name_entry = NULL;
    }

    X509_REQ_set_subject_name(handle->req, req_name);
    X509_NAME_free(req_name);
    req_name = NULL;

    if(GLOBUS_GSI_CERT_UTILS_IS_GSI_3_PROXY(handle->type))
    {
        ASN1_OCTET_STRING *             ext_data;
        int                             length;
        unsigned char *                 data;
        unsigned char *                 der_data;
        X509_EXTENSION *                pci_ext;
        STACK_OF(X509_EXTENSION) *      extensions;
        
        length = i2d_PROXYCERTINFO(handle->proxy_cert_info, 
                                   NULL);
        if(length < 0)
        {
            GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                result,
                GLOBUS_GSI_PROXY_ERROR_WITH_PROXYCERTINFO,
                ("Couldn't convert PROXYCERTINFO struct from internal"
                 " to DER encoded form"));
            goto error_exit;
        }
        
        data = malloc(length);

        if(!data)
        {
            GLOBUS_GSI_PROXY_MALLOC_ERROR(length);
            goto error_exit;
        }

        der_data = data;
        
        length = i2d_PROXYCERTINFO(handle->proxy_cert_info, &der_data);
        
        if(length < 0)
        {
            GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                result,
                GLOBUS_GSI_PROXY_ERROR_WITH_PROXYCERTINFO,
                ("Couldn't convert PROXYCERTINFO struct from internal"
                 " to DER encoded form"));
            free(data);
            goto error_exit;
        }

        ext_data = ASN1_OCTET_STRING_new();
        
        if(!ASN1_OCTET_STRING_set(ext_data, data, length))
        {
            GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                result,
                GLOBUS_GSI_PROXY_ERROR_WITH_PROXYCERTINFO,
                ("Couldn't convert PROXYCERTINFO struct from internal"
                 " to DER encoded form"));
            ASN1_OCTET_STRING_free(ext_data);
            free(data);
            goto error_exit;            
        }

        free(data);
        
        pci_ext = X509_EXTENSION_create_by_NID(NULL,
                                               OBJ_sn2nid(PROXYCERTINFO_SN),
                                               1,
                                               ext_data);
        if(pci_ext == NULL)
        {
            GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                result,
                GLOBUS_GSI_PROXY_ERROR_WITH_PROXYCERTINFO,
                ("Couldn't create PROXYCERTINFO extension"));
            ASN1_OCTET_STRING_free(ext_data);
            goto error_exit;
        }
        
        ASN1_OCTET_STRING_free(ext_data);

        extensions = sk_X509_EXTENSION_new_null();

        sk_X509_EXTENSION_push(extensions, pci_ext);

        X509_REQ_add_extensions(handle->req, extensions);

        sk_X509_EXTENSION_pop_free(extensions, X509_EXTENSION_free);

        GLOBUS_I_GSI_PROXY_DEBUG_PRINT(3, "****** START PROXYCERTINFO ******\n");
        GLOBUS_I_GSI_PROXY_DEBUG_PRINT_OBJECT(3, 
                                              PROXYCERTINFO, 
                                              handle->proxy_cert_info);
        GLOBUS_I_GSI_PROXY_DEBUG_PRINT(3, "******  END PROXYCERTINFO  ******\n");
    }
    
    if (!X509_REQ_sign(handle->req, handle->proxy_key, EVP_md5()))
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_X509_REQ,
            ("Couldn't sign the X509_REQ structure for later verification"));
        goto error_exit;
    }

    /* write the request to the BIO */
    if(i2d_X509_REQ_bio(output_bio, handle->req) == 0)
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_X509_REQ,
            ("Couldn't convert X509 request from internal to"
             " DER encoded form"));
        goto error_exit;
    }

    GLOBUS_I_GSI_PROXY_DEBUG_PRINT(3, "****** START X509_REQ ******\n");
    GLOBUS_I_GSI_PROXY_DEBUG_PRINT_OBJECT(3, X509_REQ, handle->req);
    GLOBUS_I_GSI_PROXY_DEBUG_PRINT(3, "******  END X509_REQ  ******\n");

    result = GLOBUS_SUCCESS;
    goto exit;

 error_exit:
    if(rsa_key)
    {
        RSA_free(rsa_key);
    }

 exit:

    if(req_name)
    {
        X509_NAME_free(req_name);
    }

    if(req_name_entry)
    {
        X509_NAME_ENTRY_free(req_name_entry);
    }

    GLOBUS_I_GSI_PROXY_DEBUG_EXIT;
    return result;
}
/* globus_gsi_proxy_create_req */
/*@}*/

/**
 * @name Inquire Request
 * @ingroup globus_gsi_proxy_operations
 */
/*@{*/
/**
 * Inquire a proxy credential request
 *
 * This function reads the public part of a proxy credential request
 * from input_bio and if the request contains a ProxyCertInfo
 * extension, updates the handle with the information contained in the
 * extension.
 *
 * @param handle
 *        A GSI Proxy handle to use for the inquire operation.
 * @param input_bio
 *        A BIO to read a request structure from.
 * @return
 *        GLOBUS_SUCCESS unless an error occurred, in which case, 
 *        a globus error object ID is returned
 */
globus_result_t
globus_gsi_proxy_inquire_req(
    globus_gsi_proxy_handle_t           handle,
    BIO *                               input_bio)
{
    globus_result_t                     result;
    PROXYPOLICY *                       policy = NULL;
    ASN1_OBJECT *                       policy_lang = NULL;
    ASN1_OBJECT *                       extension_oid = NULL;
    ASN1_OCTET_STRING *                 ext_data = NULL;
    int                                 policy_nid;
    int                                 pci_NID;
    int                                 i;
    STACK_OF(X509_EXTENSION) *          req_extensions = NULL;
    X509_EXTENSION *                    extension;
    unsigned char *                     tmp_data;
    
    static char *                       _function_name_ =
        "globus_gsi_proxy_inquire_req";

    GLOBUS_I_GSI_PROXY_DEBUG_ENTER;

    if(handle == NULL)
    {
        GLOBUS_GSI_PROXY_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE,
            ("NULL handle passed to function: %s", _function_name_));
        goto done;
    }

    if(input_bio == NULL)
    {
        GLOBUS_GSI_PROXY_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_BIO,
            ("NULL bio passed to function: %s", _function_name_));
        goto done;
    }

    if(handle->req)
    {
        X509_REQ_free(handle->req);
        handle->req = NULL;
    }

    if(d2i_X509_REQ_bio(input_bio, & handle->req) == NULL)
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_X509_REQ,
            ("Couldn't convert X509_REQ struct from DER encoded "
             "to internal form"));
        goto done;
    }

    req_extensions = X509_REQ_get_extensions(handle->req);

    pci_NID = OBJ_sn2nid(PROXYCERTINFO_SN);
    
    for(i=0;i<sk_X509_EXTENSION_num(req_extensions);i++)
    {
        extension = sk_X509_EXTENSION_value(req_extensions,i);
        extension_oid = X509_EXTENSION_get_object(extension);

        if(OBJ_obj2nid(extension_oid) == pci_NID)
        {
            if((ext_data = X509_EXTENSION_get_data(extension)) == NULL)
            {
                GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                    result,
                    GLOBUS_GSI_PROXY_ERROR_WITH_PROXYCERTINFO,
                    ("Can't get DER encoded extension "
                     "data from X509 extension object"));
                goto done;
            }

            if(handle->proxy_cert_info)
            {
                PROXYCERTINFO_free(handle->proxy_cert_info);
                handle->proxy_cert_info = NULL;
            }    

            tmp_data = ext_data->data;
            
            if((d2i_PROXYCERTINFO(
                    &handle->proxy_cert_info,
                    &tmp_data,
                    ext_data->length)) == NULL)
            {
                GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                    result,
                    GLOBUS_GSI_PROXY_ERROR_WITH_PROXYCERTINFO,
                    ("Can't convert DER encoded PROXYCERTINFO "
                     "extension to internal form"));
                goto done;
            }
            break;
        }
    }
    
    if(handle->proxy_cert_info != NULL)
    {
        if((policy = PROXYCERTINFO_get_policy(handle->proxy_cert_info))
           == NULL)
        {
            GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                result,
                GLOBUS_GSI_PROXY_ERROR_WITH_PROXYCERTINFO,
                ("Can't get policy from PROXYCERTINFO extension"));
            goto done;
        }
        
        if((policy_lang = PROXYPOLICY_get_policy_language(policy))
           == NULL)
        {
            GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                result,
                GLOBUS_GSI_PROXY_ERROR_WITH_PROXYCERTINFO,
                ("Can't get policy language from"
                 " PROXYCERTINFO extension"));
            goto done;
        }
        
        policy_nid = OBJ_obj2nid(policy_lang);
        
        if(policy_nid == OBJ_sn2nid(IMPERSONATION_PROXY_SN))
        {
            handle->type= GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_IMPERSONATION_PROXY;
        }
        else if(policy_nid == OBJ_sn2nid(INDEPENDENT_PROXY_SN))
        {
            handle->type = GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_INDEPENDENT_PROXY;
        }
        else if(policy_nid == OBJ_sn2nid(LIMITED_PROXY_SN))
        {
            handle->type = GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_LIMITED_PROXY;
        }
        else
        {
            handle->type = GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_RESTRICTED_PROXY;
        }
    }
    else
    {
        handle->type = GLOBUS_GSI_CERT_UTILS_TYPE_GSI_2_PROXY;
    }

    GLOBUS_I_GSI_PROXY_DEBUG_PRINT(3, "****** START X509_REQ ******\n");
    GLOBUS_I_GSI_PROXY_DEBUG_PRINT_OBJECT(3, X509_REQ, handle->req);
    GLOBUS_I_GSI_PROXY_DEBUG_PRINT(3, "******  END X509_REQ  ******\n");
    GLOBUS_I_GSI_PROXY_DEBUG_PRINT(3, "****** START PCI ******\n");
    GLOBUS_I_GSI_PROXY_DEBUG_PRINT_OBJECT(3, PROXYCERTINFO, 
                                          handle->proxy_cert_info);
    GLOBUS_I_GSI_PROXY_DEBUG_PRINT(3, "******  END PCI  ******\n");

    result = GLOBUS_SUCCESS;

 done:

    if(req_extensions != NULL)
    {
        sk_X509_EXTENSION_pop_free(req_extensions, X509_EXTENSION_free);
    }
    
    GLOBUS_I_GSI_PROXY_DEBUG_EXIT;
    return result;
}
/* globus_gsi_proxy_inquire_req */
/*@}*/

/**
 * @name Sign Request
 * @ingroup globus_gsi_proxy_operations
 */
/*@{*/
/**
 * Sign a proxy certificate request
 *
 * This function signs the public part of a proxy credential request,
 * i.e. the unsigned certificate, previously read by inquire req using
 * the supplied issuer_credential. This operation will add a
 * ProxyCertInfo extension to the proxy certificate if values
 * contained in the extension are specified in the handle.
 * The resulting signed certificate is written to the output_bio, along
 * with the new cert chain for that certificate.  The order of data written
 * to the bio is: N, N-1, ..., 0.  N being the newly signed certificate.
 *
 * @param handle
 *        A GSI Proxy handle to use for the signing operation.
 * @param issuer_credential
 *        The credential structure to be used for signing the proxy
 *        certificate. 
 * @param output_bio
 *        A BIO to write the resulting certificate and cert chain to.
 * @return
 *        GLOBUS_SUCCESS unless an error occurred, in which case, 
 *        a globus error object ID is returned
 */
globus_result_t
globus_gsi_proxy_sign_req(
    globus_gsi_proxy_handle_t           handle,
    globus_gsi_cred_handle_t            issuer_credential,
    BIO *                               output_bio)
{
    char *                              common_name;
    int                                 pci_NID = NID_undef;
    int                                 pci_DER_length;
    unsigned char *                     pci_DER = NULL;
    unsigned char *                     mod_pci_DER = NULL;
    ASN1_OCTET_STRING *                 pci_DER_string = NULL;
    X509 *                              new_pc = NULL;
    X509 *                              issuer_cert = NULL;
    X509_EXTENSION *                    pci_ext = NULL;
    X509_EXTENSION *                    extension;
    int                                 position;
    EVP_PKEY *                          issuer_pkey = NULL;
    EVP_PKEY *                          req_pubkey = NULL;
    globus_result_t                     result = GLOBUS_SUCCESS;
    int                                 res;
    ASN1_INTEGER *                      serial_number = NULL;
    
    static char *                       _function_name_ =
        "globus_gsi_proxy_sign_req";

    GLOBUS_I_GSI_PROXY_DEBUG_ENTER;
    
    if(handle == NULL || issuer_credential == NULL)
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE,
            ("NULL handle passed to function: %s", _function_name_));
        goto done;
    }
    
    if(output_bio == NULL)
    {
        GLOBUS_GSI_PROXY_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_BIO,
            ("NULL bio passed to function: %s", _function_name_));
        goto done;
    }

    req_pubkey = X509_REQ_get_pubkey(handle->req);
    if(!req_pubkey)
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_X509_REQ,
            ("Error getting public key from request structure"));
        goto done;
    }

    res = X509_REQ_verify(handle->req, req_pubkey);
    if(!res)
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_X509_REQ,
            ("Error verifying X509_REQ struct"));
        goto done;
    }

    result = globus_gsi_cred_get_cert(issuer_credential, &issuer_cert);
    if(result != GLOBUS_SUCCESS)
    {
        GLOBUS_GSI_PROXY_ERROR_CHAIN_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_CREDENTIAL);
        goto done;
    }

    if((new_pc = X509_new()) == NULL)
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_X509,
            ("Couldn't initialize new X509"));
        goto done;
    }
    
    if(GLOBUS_GSI_CERT_UTILS_IS_GSI_3_PROXY(handle->type))
    {
        EVP_MD *                        sha1 = EVP_sha1();
        unsigned char                   md[SHA_DIGEST_LENGTH];
        long                            sub_hash;
        unsigned int                    len;
        
        ASN1_digest(i2d_PUBKEY,sha1,(char *) req_pubkey,md,&len);

        sub_hash = md[0] + (md[1] + (md[2] + (md[3] >> 1) * 256) * 256) * 256; 
        
        if(handle->common_name)
        {
            common_name = strdup(handle->common_name);
        }
        else
        { 
            common_name = malloc(sizeof(long)*4 + 1);

            if(!common_name)
            {
                result =
                    GLOBUS_GSI_PROXY_MALLOC_ERROR(sizeof(long)*4 + 1);
                goto done;
            }

            sprintf(common_name, "%ld", sub_hash);        
        }

        serial_number = ASN1_INTEGER_new();

        ASN1_INTEGER_set(serial_number, sub_hash);

        pci_NID = OBJ_sn2nid(PROXYCERTINFO_SN);

        /* create the X509 extension from the PROXYCERTINFO */

        if(pci_NID == NID_undef)
        {
            GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                result,
                GLOBUS_GSI_PROXY_ERROR_WITH_PROXYCERTINFO,
                ("No valid PROXYCERTINFO numeric identifier found"));
            goto done;
        }
        
        pci_DER_length = i2d_PROXYCERTINFO(handle->proxy_cert_info, 
                                           NULL);
        if(pci_DER_length < 0)
        {
            GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                result,
                GLOBUS_GSI_PROXY_ERROR_WITH_PROXYCERTINFO,
                ("Couldn't convert PROXYCERTINFO struct from internal"
                 " to DER encoded form"));
            goto done;
        }
        
        pci_DER = malloc(pci_DER_length);

        if(!pci_DER)
        {
            GLOBUS_GSI_PROXY_MALLOC_ERROR(pci_DER_length);
            goto done;
        }
        
        mod_pci_DER = pci_DER;
        pci_DER_length = i2d_PROXYCERTINFO(handle->proxy_cert_info,
                                           (unsigned char **) &mod_pci_DER);
        if(pci_DER_length < 0)
        {
            GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                result,
                GLOBUS_GSI_PROXY_ERROR_WITH_PROXYCERTINFO,
                ("Couldn't convert PROXYCERTINFO struct from internal"
                 " to DER encoded form"));
            goto done;
        }
        
        pci_DER_string = ASN1_OCTET_STRING_new();
        if(pci_DER_string == NULL)
        {
            GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                result,
                GLOBUS_GSI_PROXY_ERROR_WITH_PROXYCERTINFO,
                ("Couldn't creat new ASN.1 octet string for the DER encoding"
                 " of a PROXYCERTINFO struct"));
            goto done;
        }
        
        pci_DER_string->data = pci_DER;
        pci_DER_string->length = pci_DER_length;
        
        pci_ext = X509_EXTENSION_create_by_NID(
            &pci_ext, 
            pci_NID, 
            1,
            pci_DER_string);

        if(pci_ext == NULL)
        {
            GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                result,
                GLOBUS_GSI_PROXY_ERROR_WITH_X509_EXTENSIONS,
                ("Couldn't create X509 extension list "
                 "to hold PROXYCERTINFO extension"));
            goto done;
        }

        if(!X509_add_ext(new_pc, pci_ext, 0))
        {
            GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                result,
                GLOBUS_GSI_PROXY_ERROR_WITH_X509_EXTENSIONS,
                ("Couldn't add X509 extension to new proxy cert"));
            goto done;
        }
    }
    else if(handle->type == GLOBUS_GSI_CERT_UTILS_TYPE_GSI_2_LIMITED_PROXY)
    {
        common_name = LIMITED_PROXY_NAME;
        serial_number = X509_get_serialNumber(issuer_cert);
    }
    else
    {
        common_name = PROXY_NAME;
        serial_number = X509_get_serialNumber(issuer_cert);
    }

    /* add any keyUsage and extendedKeyUsage extensions present in the issuer
     * cert
     */

    if((position = X509_get_ext_by_NID(issuer_cert, NID_key_usage, -1)) > -1)
    {
        ASN1_BIT_STRING *               usage;
        ASN1_OCTET_STRING *             ku_DER_string;
        unsigned char *                 ku_DER;
        unsigned char *                 mod_ku_DER;
        int                             ku_DER_length;

        if(!(extension = X509_get_ext(issuer_cert, position)))
        {
            GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                result,
                GLOBUS_GSI_PROXY_ERROR_WITH_X509_EXTENSIONS,
                ("Couldn't get keyUsage extension form issuer cert"));
            goto done;            
        }
        
        if(!(usage = X509_get_ext_d2i(issuer_cert, NID_key_usage, NULL, NULL)))
        {
            GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                result,
                GLOBUS_GSI_PROXY_ERROR_WITH_X509_EXTENSIONS,
                ("Couldn't convert keyUsage struct from DER encoded form"
                 " to internal form"));
            goto done;
        }

        /* clear bits specified in draft */
        
        ASN1_BIT_STRING_set_bit(usage, 1, 0); /* Non Repudiation */
        ASN1_BIT_STRING_set_bit(usage, 5, 0); /* Certificate Sign */
        
        ku_DER_length = i2d_ASN1_BIT_STRING(usage,
                                            NULL);
        if(ku_DER_length < 0)
        {
            GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                result,
                GLOBUS_GSI_PROXY_ERROR_WITH_X509_EXTENSIONS,
                ("Couldn't convert keyUsage struct from internal"
                 " to DER encoded form"));
            ASN1_BIT_STRING_free(usage);
            goto done;
        }
        
        ku_DER = malloc(ku_DER_length);

        if(!ku_DER)
        {
            GLOBUS_GSI_PROXY_MALLOC_ERROR(ku_DER_length);
            ASN1_BIT_STRING_free(usage);
            goto done;
        }
        
        mod_ku_DER = ku_DER;

        ku_DER_length = i2d_ASN1_BIT_STRING(usage,
                                            &mod_ku_DER);

        if(ku_DER_length < 0)
        {
            GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                result,
                GLOBUS_GSI_PROXY_ERROR_WITH_X509_EXTENSIONS,
                ("Couldn't convert keyUsage from internal"
                 " to DER encoded form"));
            ASN1_BIT_STRING_free(usage);
            goto done;
        }

        ASN1_BIT_STRING_free(usage);        
        
        ku_DER_string = ASN1_OCTET_STRING_new();
        if(ku_DER_string == NULL)
        {
            GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                result,
                GLOBUS_GSI_PROXY_ERROR_WITH_X509_EXTENSIONS,
                ("Couldn't creat new ASN.1 octet string for the DER encoding"
                 " of the keyUsage"));
            free(ku_DER);
            goto done;
        }
        
        ku_DER_string->data = ku_DER;
        ku_DER_string->length = ku_DER_length;

        extension = X509_EXTENSION_create_by_NID(
            NULL,
            NID_key_usage,
            1,
            ku_DER_string);

        ASN1_OCTET_STRING_free(ku_DER_string);
        
        if(extension == NULL)
        {
            GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                result,
                GLOBUS_GSI_PROXY_ERROR_WITH_X509_EXTENSIONS,
                ("Couldn't create new keyUsage extension"));
            goto done;
        }
        
        if(!X509_add_ext(new_pc, extension, 0))
        {
            GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                result,
                GLOBUS_GSI_PROXY_ERROR_WITH_X509_EXTENSIONS,
                ("Couldn't add X509 keyUsage extension to new proxy cert"));
            X509_EXTENSION_free(extension);
            goto done;
        }

        X509_EXTENSION_free(extension);
    }

    if((position =
        X509_get_ext_by_NID(issuer_cert, NID_ext_key_usage, -1)) > -1)
    {
        if(!(extension = X509_get_ext(issuer_cert, position)))
        {
            GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                result,
                GLOBUS_GSI_PROXY_ERROR_WITH_X509_EXTENSIONS,
                ("Couldn't get extendedKeyUsage extension form issuer cert"));
            goto done;            
        }

        extension = X509_EXTENSION_dup(extension);

        if(extension == NULL)
        {
            GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                result,
                GLOBUS_GSI_PROXY_ERROR_WITH_X509_EXTENSIONS,
                ("Couldn't copy extendedKeyUsage extension"));
            goto done;
        }

        if(!X509_add_ext(new_pc, extension, 0))
        {
            GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                result,
                GLOBUS_GSI_PROXY_ERROR_WITH_X509_EXTENSIONS,
                ("Couldn't add X509 extendedKeyUsage extension to new proxy cert"));
            goto done;
        }
    }
    
    /* create proxy subject name */
    result = globus_i_gsi_proxy_set_subject(new_pc, issuer_cert, common_name);
    if(result != GLOBUS_SUCCESS)
    {
        GLOBUS_GSI_PROXY_ERROR_CHAIN_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_X509);
        goto done;
    }

    if(!X509_set_issuer_name(new_pc, X509_get_subject_name(issuer_cert)))
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_X509,
            ("Error setting issuer's subject of X509"));
        goto done;
    }

    if(!X509_set_version(new_pc, 2))
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_X509,
            ("Error setting version number of X509"));
        goto done;
    }

    if(!X509_set_serialNumber(new_pc, serial_number))
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_X509,
            ("Error setting serial number of X509"));
        goto done;
    }

    result = globus_i_gsi_proxy_set_pc_times(new_pc, issuer_cert, 
                                             handle->attrs->clock_skew, 
                                             handle->time_valid);
    if(result != GLOBUS_SUCCESS)
    {
        GLOBUS_GSI_PROXY_ERROR_CHAIN_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_X509);
        goto done;
    }
    
    if(!X509_set_pubkey(new_pc, req_pubkey))
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_X509,
            ("Couldn't set pubkey of X509 cert"));
        goto done;
    }

    /* sign the new certificate */
    if((result = globus_gsi_cred_get_key(issuer_credential, &issuer_pkey))
       != GLOBUS_SUCCESS)
    {
        GLOBUS_GSI_PROXY_ERROR_CHAIN_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_CREDENTIAL);
        goto done;
    }
    
    /* right now if MD5 isn't requested as the signing algorithm,
     * we throw an error
     */
    if(EVP_MD_type(handle->attrs->signing_algorithm) != NID_md5)
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE,
            ("The signing algorithm: %s is not currently allowed."
             "\nUse MD5 to sign certificate requests",
             OBJ_nid2sn(EVP_MD_type(handle->attrs->signing_algorithm))));
        goto done;
    }
    
    if(!X509_sign(new_pc, issuer_pkey, handle->attrs->signing_algorithm))
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_X509,
            ("Error signing proxy cert"));
        goto done;
    }

    /* write out the X509 certificate in DER encoded format to the BIO */
    if(!i2d_X509_bio(output_bio, new_pc))
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_X509,
            ("Error converting X509 proxy cert from internal "
             "to DER encoded form"));
        goto done;
    }
        
    GLOBUS_I_GSI_PROXY_DEBUG_PRINT(3, "****** START SIGNED CERT ******\n");
    GLOBUS_I_GSI_PROXY_DEBUG_PRINT_OBJECT(3, X509, new_pc);
    GLOBUS_I_GSI_PROXY_DEBUG_PRINT(3, "******  END SIGNED CERT  ******\n");

    result = GLOBUS_SUCCESS;

 done:

    if(issuer_pkey)
    {
        EVP_PKEY_free(issuer_pkey);
    }

    if(issuer_cert)
    {
        X509_free(issuer_cert);
    }

    if(new_pc)
    {
        X509_free(new_pc); 
    }
    
    if(req_pubkey)
    {
        EVP_PKEY_free(req_pubkey);
    }

    if(GLOBUS_GSI_CERT_UTILS_IS_GSI_3_PROXY(handle->type))
    {
        if(pci_ext)
        {
            X509_EXTENSION_free(pci_ext);
        }
        
        #ifdef WIN32
        /* In Win32 can't mix library and OpenSSL versions of free */
        /*     so pci_DER can't be freed in ASN1_OCTET_STRING_free */
        if(pci_DER_string)
        {
            if(pci_DER)
            {
                free(pci_DER);
    			pci_DER = NULL;
            }
            pci_DER_string->data = NULL;
            pci_DER_string->length = 0;
            ASN1_OCTET_STRING_free(pci_DER_string);
			pci_DER_string = NULL;
        }
        #else
        
        if(pci_DER_string)
        {
            ASN1_OCTET_STRING_free(pci_DER_string);
        }
        else if(pci_DER)
        {
            free(pci_DER);
        }
        #endif
                
        if(serial_number)
        {
            ASN1_INTEGER_free(serial_number);
        }

        if(!handle->common_name && common_name)
        {
            free(common_name);
        }
    }

    GLOBUS_I_GSI_PROXY_DEBUG_EXIT;
    return result;
}
/* globus_gsi_proxy_sign_req */
/*@}*/

/**
 * @name Create Signed
 * @ingroup globus_gsi_proxy_operations
 */
/* @{ */
/**
 * Create Signed Proxy Certificate
 *
 * @param handle
 *        The proxy handle used to create and sign the proxy certificate
 * @param issuer
 *        The issuing credential, used for signing the proxy certificate
 * @param proxy_credential
 *        The new proxy credential, containing the signed cert, 
 *        private key, etc.
 * 
 * @return
 *        GLOBUS_SUCCESS if no error occurred, an error object ID otherwise
 */
globus_result_t
globus_gsi_proxy_create_signed(
    globus_gsi_proxy_handle_t           handle,
    globus_gsi_cred_handle_t            issuer,
    globus_gsi_cred_handle_t *          proxy_credential)
{
    X509 *                              issuer_cert = NULL;
    STACK_OF(X509) *                    issuer_cert_chain = NULL;
    int                                 chain_index = 0;
    globus_gsi_proxy_handle_t           inquire_handle = NULL;
    globus_result_t                     result = GLOBUS_SUCCESS;
    BIO *                               rw_mem_bio = NULL;
    static char *                       _function_name_ =
        "globus_gsi_proxy_create_signed";
    GLOBUS_I_GSI_PROXY_DEBUG_ENTER;

    rw_mem_bio = BIO_new(BIO_s_mem());
    if(!rw_mem_bio)
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_BIO,
            ("Can't create memory BIO for reading and writing"));
        goto exit;
    }
    
    result = globus_gsi_proxy_create_req(handle, rw_mem_bio);
    if(result != GLOBUS_SUCCESS)
    {
        GLOBUS_GSI_PROXY_ERROR_CHAIN_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE);
        goto exit;
    }

    result = globus_gsi_proxy_handle_init(&inquire_handle, handle->attrs);
    if(result != GLOBUS_SUCCESS)
    {
        GLOBUS_GSI_PROXY_ERROR_CHAIN_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE);
        goto exit;
    }

    result = globus_gsi_proxy_inquire_req(inquire_handle, rw_mem_bio);
    if(result != GLOBUS_SUCCESS)
    {
        GLOBUS_GSI_PROXY_ERROR_CHAIN_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE);
        goto exit;
    }

    result = globus_gsi_proxy_handle_set_type(
        inquire_handle,
        handle->type);

    if(result != GLOBUS_SUCCESS)
    {
        GLOBUS_GSI_PROXY_ERROR_CHAIN_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE);
        goto exit;
    }

    result = globus_gsi_proxy_handle_set_common_name(
        inquire_handle,
        handle->common_name);

    if(result != GLOBUS_SUCCESS)
    {
        GLOBUS_GSI_PROXY_ERROR_CHAIN_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE);
        goto exit;
    }

    result = globus_gsi_proxy_handle_set_time_valid(
        inquire_handle,
        handle->time_valid);

    if(result != GLOBUS_SUCCESS)
    {
        GLOBUS_GSI_PROXY_ERROR_CHAIN_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE);
        goto exit;
    }
    
    result = globus_gsi_proxy_sign_req(inquire_handle, issuer, rw_mem_bio);
    if(result != GLOBUS_SUCCESS)
    {
        GLOBUS_GSI_PROXY_ERROR_CHAIN_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE);
        goto exit;
    }

    result = globus_gsi_cred_get_cert(issuer, &issuer_cert);
    if(result != GLOBUS_SUCCESS)
    {
        GLOBUS_GSI_PROXY_ERROR_CHAIN_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE);
        goto exit;
    }

    if(!i2d_X509_bio(rw_mem_bio, issuer_cert))
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_BIO,
            ("Couldn't write issuer cert to mem bio"));
        goto exit;
    }

    X509_free(issuer_cert);
    issuer_cert = NULL;

    result = globus_gsi_cred_get_cert_chain(issuer, &issuer_cert_chain);
    if(result != GLOBUS_SUCCESS)
    {
        GLOBUS_GSI_PROXY_ERROR_CHAIN_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE);
        goto exit;
    }

    for(chain_index = 0;
        chain_index < sk_X509_num(issuer_cert_chain);
        ++chain_index)
    {
        X509 *                          chain_cert =
            sk_X509_value(issuer_cert_chain, 
                          chain_index);
        if(!i2d_X509_bio(rw_mem_bio, chain_cert))
        {
            GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                result,
                GLOBUS_GSI_PROXY_ERROR_WITH_BIO,
                ("Couldn't write cert from cert chain to mem bio"));
            goto exit;
        }
    }

    sk_X509_pop_free(issuer_cert_chain, X509_free);
    issuer_cert_chain = NULL;

    result = globus_gsi_proxy_handle_destroy(inquire_handle);
    inquire_handle = NULL;

    result = globus_gsi_proxy_assemble_cred(handle, 
                                            proxy_credential, 
                                            rw_mem_bio);
    if(result != GLOBUS_SUCCESS)
    {
        GLOBUS_GSI_PROXY_ERROR_CHAIN_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE);
        goto exit;
    }

 exit:

    if(inquire_handle)
    {
        globus_gsi_proxy_handle_destroy(inquire_handle);
    }

    if(rw_mem_bio)
    {
        BIO_free(rw_mem_bio);
    }

    GLOBUS_I_GSI_PROXY_DEBUG_EXIT;
    return result;
}
/* @} */            


/**
 * @name Assemble credential
 * @ingroup globus_gsi_proxy_operations
 */
/*@{*/
/**
 * Assemble a proxy credential
 *
 * This function assembles a proxy credential. It reads a signed proxy
 * certificate and a associated certificate chain from the input_bio
 * and combines them with a private key previously generated by a call
 * to globus_gsi_proxy_create_req. The resulting credential is then
 * returned through the proxy_credential parameter.
 *
 * @param handle
 *        A GSI Proxy handle to use for the assemble operation.
 * @param proxy_credential
 *        This parameter will contain the assembled credential upon
 *        successful return.
 * @param input_bio
 *        A BIO to read a signed certificate and corresponding
 *        certificate chain from.
 * @return
 *        GLOBUS_SUCCESS if no error occurred, an error object ID otherwise
 */
globus_result_t
globus_gsi_proxy_assemble_cred(
    globus_gsi_proxy_handle_t           handle,
    globus_gsi_cred_handle_t *          proxy_credential,
    BIO *                               input_bio)
{
    X509 *                              signed_cert = NULL;
    STACK_OF(X509) *                    cert_chain = NULL;
    globus_gsi_cred_handle_attrs_t      cred_handle_attrs = NULL;
    globus_result_t                     result;

    static char *                       _function_name_ =
        "globus_gsi_proxy_assemble_cred";

    GLOBUS_I_GSI_PROXY_DEBUG_ENTER;

    /* check to make sure params are ok */
    if(handle == NULL)
    {
        GLOBUS_GSI_PROXY_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE,
            ("NULL handle parameter passed to function: %s", _function_name_));
        goto done;
    }

    if(proxy_credential == NULL)
    {
        GLOBUS_GSI_PROXY_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_CREDENTIAL,
            ("NULL proxy credential passed to function: %s", _function_name_));
        goto done;
    }

    if(input_bio == NULL)
    {
        GLOBUS_GSI_PROXY_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_BIO,
            ("NULL bio passed to function: %s", _function_name_));
        goto done;
    }

    /* get the signed proxy cert from the BIO */
    if(!d2i_X509_bio(input_bio, &signed_cert))
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_X509,
            ("Couldn't convert X509 proxy cert from "
             "DER encoded to internal form"));
        goto done;
    }
    
    result = globus_gsi_cred_handle_attrs_init(&cred_handle_attrs);
    if(result != GLOBUS_SUCCESS)
    {
        GLOBUS_GSI_PROXY_ERROR_CHAIN_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_CRED_HANDLE_ATTRS);        
        goto free_signed_cert;
    }

    result = globus_gsi_cred_handle_init(proxy_credential, 
                                         cred_handle_attrs);
    if(result != GLOBUS_SUCCESS)
    {
        GLOBUS_GSI_PROXY_ERROR_CHAIN_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_CRED_HANDLE);
        goto free_cred_handle_attrs;
    }

    result = globus_gsi_cred_set_cert(*proxy_credential, signed_cert);
    if(result != GLOBUS_SUCCESS)
    {
        GLOBUS_GSI_PROXY_ERROR_CHAIN_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_CRED_HANDLE);
        goto free_cred_handle;
    }

    result = globus_gsi_cred_set_key(*proxy_credential, handle->proxy_key);
    if(result != GLOBUS_SUCCESS)
    {
        GLOBUS_GSI_PROXY_ERROR_CHAIN_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_CRED_HANDLE);
        goto free_cred_handle;
    }

    cert_chain = sk_X509_new_null();
    if(!cert_chain)
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_X509,
            ("Couldn't create new stack for cert chains"));
        goto free_cred_handle;
    }

    while(!BIO_eof(input_bio))
    {
        X509 *                          tmp_cert = NULL;

        tmp_cert = d2i_X509_bio(input_bio, &tmp_cert);
        if(tmp_cert == NULL)
        {
            GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                result,
                GLOBUS_GSI_PROXY_ERROR_WITH_X509,
                ("Can't read DER encoded X509 cert from BIO"));
            goto free_cred_handle;
        }
        
        sk_X509_push(cert_chain, tmp_cert);
    }

    result = globus_gsi_cred_set_cert_chain(*proxy_credential,
                                            cert_chain);
    if(result != GLOBUS_SUCCESS)
    {
        GLOBUS_GSI_PROXY_ERROR_CHAIN_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_CRED_HANDLE);
        goto free_cred_handle;
    }

    sk_X509_pop_free(cert_chain, X509_free);

    result = GLOBUS_SUCCESS;
    goto done;

 free_cred_handle:
    globus_gsi_cred_handle_destroy(*proxy_credential);
 done:
 free_cred_handle_attrs:
    if(cred_handle_attrs)
    {
        globus_gsi_cred_handle_attrs_destroy(cred_handle_attrs);
    }
 free_signed_cert:
    if(signed_cert)
    {
        X509_free(signed_cert);
    }
    GLOBUS_I_GSI_PROXY_DEBUG_EXIT;
    return result;
}
/* globus_gsi_proxy_assemble_cred */
/*@}*/

/* INTERNAL FUNCTIONS */

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL

/**
 * Prints the status of a private key generating algorithm.
 * this could be modified to return more status information
 * if required.
 */
void 
globus_i_gsi_proxy_create_private_key_cb(
    int                                 num1,
    int                                 num2,
    BIO *                               output)
{
    static char *                       _function_name_ =
        "globus_i_gsi_proxy_create_private_key_cb";
    
    GLOBUS_I_GSI_PROXY_DEBUG_ENTER;

    GLOBUS_I_GSI_PROXY_DEBUG_EXIT;
}


/**
 * Takes the new proxy cert and sets the valid start
 * and end times of the cert
 */
globus_result_t 
globus_i_gsi_proxy_set_pc_times(
    X509 *                              new_pc,
    X509 *                              issuer_cert,
    int                                 skew_allowable,
    int                                 time_valid)
{
    globus_result_t                     result;
    ASN1_UTCTIME *                      pc_notAfter = NULL;
    time_t                              tmp_time;

    static char *                       _function_name_ =
        "globus_i_gsi_proxy_set_pc_times";

    GLOBUS_I_GSI_PROXY_DEBUG_ENTER;

    /* check for overflow */

    if(time_valid > ((time_t)(~0U>>1))/60)
    {
        GLOBUS_GSI_PROXY_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_INVALID_PARAMETER,
            ("Overflow in time value"));
        goto exit;        
    }
    
    /* adjust for the allowable skew */
    if(X509_gmtime_adj(X509_get_notBefore(new_pc), (- skew_allowable)) == NULL)
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_X509,
            ("Error adjusting the allowable time skew for proxy"));
        goto exit;
    }

    tmp_time = time(NULL) + ((long) 60 * time_valid);

    /* check that issuer cert won't expire before new proxy cert */
    if(time_valid == 0 ||
       X509_cmp_time(X509_get_notAfter(issuer_cert), & tmp_time) < 0)
    {
        if((pc_notAfter = 
            M_ASN1_UTCTIME_dup(X509_get_notAfter(issuer_cert))) == NULL)
        {
            GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                result,
                GLOBUS_GSI_PROXY_ERROR_WITH_X509,
                ("Error copying issuer certificate lifetime"));
            goto exit;
        }
    }
    else
    {
        pc_notAfter = M_ASN1_UTCTIME_new();
        if(!pc_notAfter)
        {
            GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                result,
                GLOBUS_GSI_PROXY_ERROR_WITH_X509,
                ("Error creating new ASN1_UTCTIME for expiration date "
                 "of proxy cert"));
        }
        
        if(X509_gmtime_adj(pc_notAfter, ((long) 60 * time_valid)) == NULL)
        {
            GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
                result,
                GLOBUS_GSI_PROXY_ERROR_WITH_X509,
                ("Error adjusting X509 proxy cert's expiration time"));
            goto free_pc_notafter;
        }
    }
    
    if(!X509_set_notAfter(new_pc, pc_notAfter))
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_X509,
            ("Error setting X509 proxy cert's expiration"));
        goto free_pc_notafter;
    }

    result = GLOBUS_SUCCESS;

 free_pc_notafter:
    
    if(pc_notAfter != NULL)
    {
        ASN1_UTCTIME_free(pc_notAfter);
    }

 exit:

    GLOBUS_I_GSI_PROXY_DEBUG_EXIT;
    return result;
}

/**
 * Takes the new proxy cert and sets the subject
 * based on the subject of the issuer cert
 */
globus_result_t 
globus_i_gsi_proxy_set_subject(
    X509 *                              new_pc,
    X509 *                              issuer_cert,
    char *                              common_name)

{
    X509_NAME *                         pc_name = NULL;
    X509_NAME_ENTRY *                   pc_name_entry = NULL;
    globus_result_t                     result;

    static char *                       _function_name_ = 
        "globus_i_gsi_proxy_set_subject";

    GLOBUS_I_GSI_PROXY_DEBUG_ENTER;

    if((pc_name = X509_NAME_dup(X509_get_subject_name(issuer_cert))) == NULL)
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_X509,
            ("Error copying subject name of proxy cert"));
        goto done;
    }
       
    if((pc_name_entry = 
       X509_NAME_ENTRY_create_by_NID(& pc_name_entry, NID_commonName,
                                     V_ASN1_APP_CHOOSE,
                                     (unsigned char *) common_name,
                                     -1)) == NULL)
    {
        
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_X509,
            ("Error creating NAME ENTRY of type common name"));
        goto free_pc_name;
    }

    if(!X509_NAME_add_entry(pc_name, pc_name_entry,
                            X509_NAME_entry_count(pc_name), 0) ||
       !X509_set_subject_name(new_pc, pc_name))
    {
        GLOBUS_GSI_PROXY_OPENSSL_ERROR_RESULT(
            result,
            GLOBUS_GSI_PROXY_ERROR_WITH_X509,
            ("Error setting common name of subject in proxy cert"));
        goto free_pc_name_entry;
    }
    
    result = GLOBUS_SUCCESS;

 free_pc_name_entry:
    if(pc_name_entry)
    {
        X509_NAME_ENTRY_free(pc_name_entry);
    }

 free_pc_name:
    if(pc_name)
    {
        X509_NAME_free(pc_name);
    }

 done:

    GLOBUS_I_GSI_PROXY_DEBUG_EXIT;
    return result;
}

#endif
