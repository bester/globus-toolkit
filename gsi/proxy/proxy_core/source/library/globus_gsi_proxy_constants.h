
#ifndef GLOBUS_GSI_PROXY_CONSTANTS_H
#define GLOBUS_GSI_PROXY_CONSTANTS_H

/**
 * @defgroup globus_gsi_proxy_constants GSI Proxy Constants
 */
/**
 * GSI Proxy Error codes
 * @ingroup globus_gsi_proxy_constants
 */
typedef enum
{
    GLOBUS_GSI_PROXY_ERROR_SUCCESS = 0,
    GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE = 1,
    GLOBUS_GSI_PROXY_ERROR_WITH_HANDLE_ATTRS = 2,
    GLOBUS_GSI_PROXY_ERROR_WITH_PROXYCERTINFO = 3,
    GLOBUS_GSI_PROXY_ERROR_WITH_PROXYRESTRICTION = 4,
    GLOBUS_GSI_PROXY_ERROR_WITH_PROXYGROUP = 5,
    GLOBUS_GSI_PROXY_ERROR_WITH_PATHLENGTH = 6,
    GLOBUS_GSI_PROXY_ERROR_WITH_X509_REQ = 7,
    GLOBUS_GSI_PROXY_ERROR_WITH_X509 = 8,
    GLOBUS_GSI_PROXY_ERROR_WITH_X509_EXTENSIONS = 9,
    GLOBUS_GSI_PROXY_ERROR_WITH_PRIVATE_KEY = 10,
    GLOBUS_GSI_PROXY_ERROR_WITH_BIO = 11,
    GLOBUS_GSI_PROXY_ERROR_WITH_CREDENTIAL = 12,
    GLOBUS_GSI_PROXY_ERROR_WITH_CRED_HANDLE = 13,
    GLOBUS_GSI_PROXY_ERROR_WITH_CRED_HANDLE_ATTRS = 14,
    GLOBUS_GSI_PROXY_ERROR_ERRNO = 15,
    GLOBUS_GSI_PROXY_ERROR_LAST = 16
    
} globus_gsi_proxy_error_t;

#endif
