#ifndef GLOBUS_I_XIO_GSI_H
#define GLOBUS_I_XIO_GSI_H

#include <assert.h>
#include "globus_xio_driver.h"
#include "globus_xio_load.h"
#include "globus_i_xio.h"
#include "globus_common.h"
#include "globus_xio_gsi.h"
#include "gssapi.h"
#include "globus_error_gssapi.h"

#define GLOBUS_XIO_GSI_DRIVER_MODULE &globus_i_xio_gsi_module

/* create/calculate a token header */

#define GlobusLXIOGSICreateHeader(__iovec, __length)              \
    {                                                             \
        *(((unsigned char *) (__iovec).iov_base)) =               \
            (unsigned char) (((__length) >> 24) & 0xff);          \
        *(((unsigned char *) (__iovec).iov_base)+1) =             \
            (unsigned char) (((__length) >> 16) & 0xff);          \
        *(((unsigned char *) (__iovec).iov_base)+2) =             \
            (unsigned char) (((__length) >>  8) & 0xff);          \
        *(((unsigned char *) (__iovec).iov_base)+3) =             \
            (unsigned char) (((__length)      ) & 0xff);          \
    }

/* get the token length from a wrapped token */

#define GlobusLXIOGSIGetTokenLength(__iovec, __length)            \
    {                                                             \
        globus_byte_t *                 c;                        \
        c = (__iovec).iov_base;                                   \
        (__length)  = ((globus_size_t) (*((c)++))) << 24;         \
        (__length) |= ((globus_size_t) (*((c)++))) << 16;         \
        (__length) |= ((globus_size_t) (*((c)++))) << 8;          \
        (__length) |= ((globus_size_t) (*((c)++)));               \
    }


/* macro for wrapping gssapi errors */

#define GlobusXIOErrorWrapGSSFailed(failed_func, major_status, minor_status) \
    globus_error_put(                                                        \
        globus_error_wrap_gssapi_error(                                      \
            GLOBUS_XIO_GSI_DRIVER_MODULE,                                    \
            (major_status),                                                  \
            (minor_status),                                                  \
            GLOBUS_XIO_ERROR_WRAPPED,                                        \
            __FILE__,                                                        \
            _xio_name,                                                       \
            __LINE__,                                                        \
            "%s failed.",                                                    \
            (failed_func)))

/* XIO debug stuff */

GlobusDebugDeclare(GLOBUS_XIO_GSI);

#define GLOBUS_XIO_GSI_DEBUG_TRACE 4
#define GLOBUS_XIO_GSI_DEBUG_INTERNAL_TRACE 8

#define GlobusXIOGSIDebugPrintf(level, message)                             \
    GlobusDebugPrintf(GLOBUS_XIO_GSI, level, message)

#define GlobusXIOGSIDebugEnter()                                            \
    GlobusXIOGSIDebugPrintf(                                                \
        GLOBUS_XIO_GSI_DEBUG_TRACE,                                         \
        ("[%s] Entering\n", _xio_name))

#define GlobusXIOGSIDebugExit()                                             \
    GlobusXIOGSIDebugPrintf(                                                \
        GLOBUS_XIO_GSI_DEBUG_TRACE,                                         \
        ("[%s] Exiting\n", _xio_name))

#define GlobusXIOGSIDebugExitWithError()                                    \
    GlobusXIOGSIDebugPrintf(                                                \
        GLOBUS_XIO_GSI_DEBUG_TRACE,                                         \
        ("[%s] Exiting with error\n", _xio_name))

#define GlobusXIOGSIDebugInternalEnter()                                    \
    GlobusXIOGSIDebugPrintf(                                                \
        GLOBUS_XIO_GSI_DEBUG_INTERNAL_TRACE,                                \
        ("[%s] I Entering\n", _xio_name))

#define GlobusXIOGSIDebugInternalExit()                                     \
    GlobusXIOGSIDebugPrintf(                                                \
        GLOBUS_XIO_GSI_DEBUG_INTERNAL_TRACE,                                \
        ("[%s] I Exiting\n", _xio_name))

#define GlobusXIOGSIDebugInternalExitWithError()                            \
    GlobusXIOGSIDebugPrintf(                                                \
        GLOBUS_XIO_GSI_DEBUG_INTERNAL_TRACE,                                \
        ("[%s] I Exiting with error\n", _xio_name))

/*
 *  attribute structure
 */
typedef struct
{
    gss_cred_id_t                       credential;
    OM_uint32                           req_flags;
    OM_uint32                           time_req;
    gss_OID                             mech_type;
    gss_channel_bindings_t              channel_bindings;
    globus_bool_t                       wrap_tokens;
    globus_size_t                       buffer_size;
    globus_xio_gsi_protection_level_t   prot_level;
} globus_l_attr_t;

/*
 * target structure
 */

typedef struct
{
    gss_name_t                          target_name;
    globus_l_attr_t *                   attr;
    /* init or accept flag */
    globus_bool_t                       init;
} globus_l_target_t;

/*
 * driver handle structure
 */

typedef struct
{
    globus_l_attr_t *                   attr;
    globus_l_target_t *                 target;
    OM_uint32                           ret_flags;
    OM_uint32                           time_rec;
    OM_uint32                           max_wrap_size;
    gss_ctx_id_t                        context;
    gss_cred_id_t                       delegated_cred;
    gss_OID                             mech_used;
    gss_name_t                          peer_name;
    gss_name_t                          local_name;
    size_t                              write_iovec_count;
    globus_xio_iovec_t *                write_iovec;
    globus_bool_t                       frame_writes;
    size_t                              write_header_count;
    unsigned char *                     write_headers;
    globus_size_t                       bytes_written;
    globus_xio_iovec_t                  read_iovec[2];
    unsigned char                       header[4];
    unsigned char *                     read_buffer;
    globus_size_t                       bytes_read;
    globus_xio_iovec_t *                user_iovec;
    size_t                              user_iovec_count;
    size_t                              user_iovec_index;
    size_t                              user_iovec_offset;
    unsigned char *                     unwrapped_buffer;
    globus_size_t                       unwrapped_buffer_length;
    globus_size_t                       unwrapped_buffer_offset;
    globus_size_t                       bytes_returned;
    globus_bool_t                       done;
    globus_object_t *                   result_obj;
    globus_bool_t                       eof;
    globus_xio_driver_handle_t          xio_driver_handle;
    int                                 connection_id;
} globus_l_handle_t;

/*
 * Structure used for passing information needed for the init/accept delegation
 * operations 
 */

typedef struct
{
    globus_l_handle_t *                 xio_handle;
    void *                              user_arg;
    globus_xio_gsi_delegation_init_callback_t
                                        init_callback;
    globus_xio_gsi_delegation_accept_callback_t
                                        accept_callback;
    gss_cred_id_t                       cred;
    gss_OID_set                         restriction_oids;
    gss_buffer_set_t                    restriction_buffers;
    OM_uint32                           time_req;
    OM_uint32                           time_rec;
    globus_xio_iovec_t                  iovec[2];
    unsigned char                       header[4];
    globus_bool_t                       done;
    globus_object_t *                   result_obj;
    globus_bool_t                       reading_header;
} globus_l_delegation_handle_t;

typedef struct
{
    globus_bool_t                       done;
    globus_result_t                     result;
    globus_mutex_t                      mutex;
    globus_cond_t                       cond;
    OM_uint32 *                         time_rec;
    gss_cred_id_t *                     cred;
} globus_l_xio_gsi_delegation_arg_t;

#endif

