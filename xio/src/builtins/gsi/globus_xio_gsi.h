#ifndef GLOBUS_XIO_GSI_DRIVER_INCLUDE
#define GLOBUS_XIO_GSI_DRIVER_INCLUDE

typedef enum
{
    GLOBUS_XIO_GSI_SET_CREDENTIAL,
    GLOBUS_XIO_GSI_GET_CREDENTIAL,
    GLOBUS_XIO_GSI_SET_GSSAPI_REQ_FLAGS,
    GLOBUS_XIO_GSI_GET_GSSAPI_REQ_FLAGS,
    GLOBUS_XIO_GSI_SET_ALLOW_LIMITED_PROXY,
    GLOBUS_XIO_GSI_SET_ALLOW_LIMITED_PROXY_MANY,
    GLOBUS_XIO_GSI_SET_DELEGATE_LIMITED_PROXY,
    GLOBUS_XIO_GSI_SET_DELEGATE_FULL_PROXY,
    GLOBUS_XIO_GSI_SET_SSL_COMPATIBLE,
    GLOBUS_XIO_GSI_SET_ANON,
    GLOBUS_XIO_GSI_SET_WRAP_MODE,
    GLOBUS_XIO_GSI_GET_WRAP_MODE,
    GLOBUS_XIO_GSI_SET_BUFFER_SIZE,
    GLOBUS_XIO_GSI_GET_BUFFER_SIZE,
    GLOBUS_XIO_GSI_SET_PROTECTION_LEVEL,
    GLOBUS_XIO_GSI_GET_PROTECTION_LEVEL,
    GLOBUS_XIO_GSI_GET_TARGET_NAME,
    GLOBUS_XIO_GSI_SET_TARGET_NAME
} globus_xio_gsi_cmd_t;

typedef enum
{
    GLOBUS_XIO_GSI_PROTECTION_LEVEL_NONE,
    GLOBUS_XIO_GSI_PROTECTION_LEVEL_INTEGRITY,
    GLOBUS_XIO_GSI_PROTECTION_LEVEL_PRIVACY
} globus_xio_gsi_protection_level_t;

#endif
