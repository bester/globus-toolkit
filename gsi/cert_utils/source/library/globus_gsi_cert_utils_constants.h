#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file globus_gsi_cert_utils_constants.h
 * Globus GSI Cert Utils
 * @author Sam Meder, Sam Lang
 *
 * $RCSfile$
 * $Revision$
 * $Date$
 */
#endif

#ifndef GLOBUS_GSI_CERT_UTILS_CONSTANTS_H
#define GLOBUS_GSI_CERT_UTILS_CONSTANTS_H

/**
 * @defgroup globus_gsi_cert_utils_constants Cert Utils Constants
 */
/**
 * Cert Utils Error Codes
 * @ingroup globus_gsi_cert_utils_constants
 */
typedef enum
{
    /** Success - never used */
    GLOBUS_GSI_CERT_UTILS_ERROR_SUCCESS = 0,
    /** Failed to retreive a subcomponent of the subject */
    GLOBUS_GSI_CERT_UTILS_ERROR_GETTING_NAME_ENTRY_OF_SUBJECT = 1,
    /** A error occured while trying to copy a X.509 subject */
    GLOBUS_GSI_CERT_UTILS_ERROR_COPYING_SUBJECT = 2,
    /** Failed to retreive a CN subcomponent of the subject */
    GLOBUS_GSI_CERT_UTILS_ERROR_GETTING_CN_ENTRY = 3,
    /** Failed to add a CN component to a X.509 subject name */
    GLOBUS_GSI_CERT_UTILS_ERROR_ADDING_CN_TO_SUBJECT = 4,
    /** Out of memory */
    GLOBUS_GSI_CERT_UTILS_ERROR_OUT_OF_MEMORY = 5,
    /** Something unexpected happen while converting a string subject to a
     * X509_NAME structure */ 
    GLOBUS_GSI_CERT_UTILS_ERROR_UNEXPECTED_FORMAT = 6,
    /** Proxy does not comply with the expected format */
    GLOBUS_GSI_CERT_UTILS_ERROR_NON_COMPLIANT_PROXY = 7,
    /** Couldn't dtermine the certificate type */
    GLOBUS_GSI_CERT_UTILS_ERROR_DETERMINING_CERT_TYPE = 8,
    /** Last marker - never used */
    GLOBUS_GSI_CERT_UTILS_ERROR_LAST = 9
} globus_gsi_cert_utils_error_t;


/**
 * Certificate Types.
 * @ingroup globus_gsi_cert_utils_constants
 */
typedef enum globus_gsi_cert_utils_cert_type_e
{
    /** A end entity certificate */
    GLOBUS_GSI_CERT_UTILS_TYPE_EEC,
    /** A CA certificate */
    GLOBUS_GSI_CERT_UTILS_TYPE_CA,
    /** A X.509 Proxy Certificate Profile RFC compliant impersonation proxy */
    GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_IMPERSONATION_PROXY,
    /** A X.509 Proxy Certificate Profile RFC compliant independent proxy */
    GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_INDEPENDENT_PROXY,
    /** A X.509 Proxy Certificate Profile RFC compliant limited proxy */
    GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_LIMITED_PROXY,
    /** A X.509 Proxy Certificate Profile RFC compliant restricted proxy */
    GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_RESTRICTED_PROXY,
    /** A legacy Globus impersonation proxy */
    GLOBUS_GSI_CERT_UTILS_TYPE_GSI_2_PROXY,
    /** A legacy Globus limited impersonation proxy */
    GLOBUS_GSI_CERT_UTILS_TYPE_GSI_2_LIMITED_PROXY
} globus_gsi_cert_utils_cert_type_t;

#endif
