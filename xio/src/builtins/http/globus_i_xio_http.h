#ifndef GLOBUS_I_XIO_HTTP_H
#define GLOBUS_I_XIO_HTTP_H 1

#include "globus_xio.h"
#include "globus_xio_driver.h"
#include "globus_xio_http.h"

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL

enum { GLOBUS_XIO_HTTP_CHUNK_SIZE = 128 };

typedef enum
{
    GLOBUS_XIO_HTTP_TRANSFER_ENCODING_DEFAULT,
    GLOBUS_XIO_HTTP_TRANSFER_ENCODING_IDENTITY,
    GLOBUS_XIO_HTTP_TRANSFER_ENCODING_CHUNKED
}
globus_i_xio_http_transfer_encoding_t;

typedef enum
{
    GLOBUS_XIO_HTTP_REQUEST_LINE,
    GLOBUS_XIO_HTTP_STATUS_LINE,
    GLOBUS_XIO_HTTP_HEADERS,
    GLOBUS_XIO_HTTP_CHUNK_CRLF,
    GLOBUS_XIO_HTTP_CHUNK_LINE,
    GLOBUS_XIO_HTTP_CHUNK_FOOTERS,
    GLOBUS_XIO_HTTP_CHUNK_BODY,
    GLOBUS_XIO_HTTP_IDENTITY_BODY,
    GLOBUS_XIO_HTTP_EOF,
    GLOBUS_XIO_HTTP_CLOSE
}
globus_i_xio_http_parse_state_t;

#define GLOBUS_XIO_HTTP_COPY_BLOB(fifo, blob, len, label) \
    do { \
        result = globus_i_xio_http_copy_blob(fifo, blob, len); \
        if (result != GLOBUS_SUCCESS) \
        { \
            goto label; \
        } \
    } while (0);

typedef struct
{
    /**
     * Copy of iovec array registered by user.
     */
    globus_xio_iovec_t *                iov;
    /**
     * Number of iovecs in the iov.
     */
    globus_size_t                       iovcnt;
    /**
     * Operation associated with user's read or write.
     */
    globus_xio_operation_t              operation;
    /**
     * Number of bytes copied into user buffers already (for residue handling
     * in reads).
     */
    globus_size_t                       nbytes;
    /**
     * Wait for remainder for operation.
     */
    int                                 wait_for;
    /**
     * buffer to hold the chunk size line
     */
    globus_byte_t                       chunk_size_buffer[64];             
}
globus_i_xio_http_operation_info_t;

#define GLOBUS_XIO_ARRAY_LENGTH(x) (sizeof(x)/sizeof(x[0]))

#if BUILD_DEBUG
#define GLOBUS_XIO_HTTP_TODO(msg) \
    do { \
        globus_libc_fprintf(stderr, "TODO: %s\n", msg); \
    } while (0);
#else
#define GLOBUS_XIO_HTTP_TODO(msg) \
    do { \
        globus_libc_fprintf(stderr, "TODO: %s\n", msg); \
        abort(); \
    } while (0);
#endif

/**
 * Target-specific data
 */
typedef struct
{
    /**
     * GLOBUS_TRUE when this is the client side of the connection.
     * GLOBUS_FALSE when this is the server side of the connection.
     */
    globus_bool_t                       is_client;
    /**
     * Host name to connecting to.
     */
    char *                              host;
    /**
     * URI path to access.
     */
    char *                              uri;
}
globus_i_xio_http_target_t;


/**
 * Internal information about a header set for a request or response.
 */
typedef struct
{
    /**
     * HTTP Headers
     *
     * Each entry in the hashtable is keyed by a char * containing the
     * header name, and the value of the hashtable entries are of type
     * #globus_xio_http_string_pair_t
     */
    globus_hashtable_t                  headers;
    /**
     * Body needed flag
     *
     * - GLOBUS_TRUE when the request or response requires that a entity be
     *   sent along with the request, and that has not yet been done.
     * - GLOBUS_FALSE when the request does not require a entity, or
     *   it has already been sent, or some error prevents it from being sent.
     */
    globus_bool_t                       entity_needed;

    /**
     * Content-Length header's value, if present
     */
    globus_size_t                       content_length;

    /**
     * Content-Length header was supplied.
     */
    globus_bool_t                       content_length_set;

    /**
     * Transfer-Encoding header's value, if present.
     */
    globus_i_xio_http_transfer_encoding_t
                                        transfer_encoding;
    /**
     * Connection will be closed after response flag.
     */
    globus_bool_t                       connection_close;
}
globus_i_xio_http_header_info_t;

/**
 * Client-generated request information.
 *
 * This is used in the HTTP attr and in the driver driver-specific data
 * associated with a driver operation.
 */
typedef struct
{
    /**
     * URI Path
     *
     * This value will overrides the path in the globus_xio_contact_t passed
     * to the target initialization function. If not set, the path in
     * the contact will be used.
     * be
     */
    char *                              uri;
    /**
     * HTTP Method
     *
     * This value will set the HTTP method (GET, PUT, POST, etc) to be
     * used by the client to access the URI. If not set, the default of
     * GET will be used.
     */
    char *                              method;
    /**
     * HTTP Version
     *
     * This value will set the HTTP version number to
     * used by the client to access the URI. If not set, the default of
     * GLOBUS_XIO_HTTP_VERSION_1_1 will be used.
     */
    globus_xio_http_version_t           http_version;
    /**
     * Response Callback Function
     *
     * Function to be called when the response is returned from the HTTP
     * server.
     */
    globus_xio_http_response_ready_callback_t
                                        callback;
    /**
     * Callback function argument.
     */
    void *                              callback_arg;

    /**
     * Information about headers associated with this request
     */
    globus_i_xio_http_header_info_t     headers;
}
globus_i_xio_http_request_t;

/**
 * All server-generated response information.
 *
 * This is used in the HTTP driver-specific data associated with a driver
 * operation.
 */
typedef struct
{
    /**
     * HTTP status code
     *
     * Defaults to 200. See RFC 2616 for details.
     */
    int                                 status_code;
    /**
     * HTTP reason phrase
     *
     * Defaults to string corresponding to status code,
     * as described in RFC 2616.
     */
    char *                              reason_phrase;
    /**
     * HTTP Version
     *
     * This value will set the HTTP version number to
     * used by the server in its response. If not set, the default of
     * GLOBUS_XIO_HTTP_VERSION_1_1 will be used.
     */
    globus_xio_http_version_t           http_version;
    /**
     * Request Callback Function
     *
     * Function to be called when the request is parsed by the HTTP driver.
     */
    globus_xio_http_request_ready_callback_t
                                        callback;
    /**
     * Callback function argument.
     */
    void *                              callback_arg;

    /**
     * Read operation to process response.
     */
    globus_xio_operation_t              read_operation;

    /**
     * Information about headers associated with this request
     */
    globus_i_xio_http_header_info_t     headers;
}
globus_i_xio_http_response_t;

typedef struct
{
    /**
     * Information about the target this handle is associated with.
     */
    globus_i_xio_http_target_t          target_info;
    /**
     * Information about the request this handle is associated with
     */
    globus_i_xio_http_request_t         request_info;
    /**
     * Information about the response to the request this handle
     * is associated with
     */
    globus_i_xio_http_response_t        response_info;
    /**
     * Driver handle associated with this HTTP handle
     */
    globus_xio_driver_handle_t          handle;

    /**
     * Dynamically allocated array of iovecs for sending headers.
     */
    globus_xio_iovec_t *                header_iovec;
    /**
     * Length of header iovec.
     */
    int                                 header_iovcnt;

    /**
     * Iovec for reading request/response header information.
     */
    globus_xio_iovec_t                  read_buffer;

    /**
     * Iovec containing part of the read_buffer which aren't busy with
     * unparsed data.
     */
    globus_xio_iovec_t                  read_iovec;

    /**
     * Beginning of unparsed data in the @a read_buffer.
     */
    globus_size_t                       read_buffer_offset;

    /**
     * Number of bytes in the @a read_buffer after the offset which
     * contain unparsed data
     */
    globus_size_t                       read_buffer_valid;
    
    /**
     * Operation used for closing, when an error occurs at open time.
     */
    globus_xio_operation_t              close_operation;
    
    /**
     * Remaining-to-be-read chunk.
     */
    globus_size_t                       read_chunk_left;
    /**
     * Current state of the HTTP parser for reading data.
     */
    globus_i_xio_http_parse_state_t     parse_state;
    /**
     * Current state of the HTTP parser for writing data.
     */
    globus_i_xio_http_parse_state_t     send_state;
    
    globus_i_xio_http_operation_info_t  read_operation;
    globus_i_xio_http_operation_info_t  write_operation;

    /**
     * Flag indicating whether close was called on this handle.
     */
    globus_bool_t                       user_close;

    /**
     * Lock for thread-safety
     */
    globus_mutex_t                      mutex;
}
globus_i_xio_http_handle_t;

/**
 * XIO Attributes for HTTP
 */
typedef struct
{
    /* attrs for client side */
    globus_i_xio_http_request_t         request;

    /* only one attr for server side for now*/
    globus_xio_http_request_ready_callback_t
                                        request_callback;
    void *                              request_callback_arg;
}
globus_i_xio_http_attr_t;

/* globus_xio_http.c */
extern
globus_result_t
globus_i_xio_http_copy_blob(
    globus_fifo_t *                     fifo,
    const unsigned char *               blob,
    size_t                              len);

extern
char *
globus_i_xio_http_find_eol(
    const char *                        blob,
    globus_size_t                       blob_length);

extern
globus_bool_t
globus_i_xio_http_method_requires_entity(
    const char *                        method);

extern
globus_xio_http_version_t
globus_i_xio_http_guess_version(
    int                                 major_version,
    int                                 minor_version);

extern
globus_result_t
globus_i_xio_http_clean_read_buffer(
    globus_i_xio_http_handle_t *        http_handle);

/* globus_xio_http_attr.c */
extern
globus_result_t
globus_i_xio_http_attr_init(
    void **                             out_attr);

extern
globus_result_t
globus_i_xio_http_attr_cntl(
    void *                              driver_attr,
    int                                 cmd,
    va_list                             ap);

extern
globus_result_t
globus_i_xio_http_attr_copy(
    void **                             dst,
    void *                              src);

extern
globus_result_t
globus_i_xio_http_attr_destroy(
    void *                              driver_attr);

/* globus_xio_http_client.c */
extern
void
globus_i_xio_http_client_open_callback(
    globus_xio_operation_t              op,
    globus_result_t                     result,
    void *                              user_arg);

/* globus_xio_http_handle.c */
extern
globus_result_t
globus_i_xio_http_handle_init(
    globus_i_xio_http_handle_t *        http_handle,
    globus_i_xio_http_attr_t *          attr,
    globus_i_xio_http_target_t *        target);

extern
void
globus_i_xio_http_handle_destroy(
    globus_i_xio_http_handle_t *        http_handle);

extern
globus_result_t
globus_i_xio_http_handle_cntl(
    void *                              handle,
    int                                 cmd,
    va_list                             ap);

extern
globus_result_t
globus_i_xio_http_set_end_of_entity(
    globus_i_xio_http_handle_t *        http_handle);

/* globus_xio_http_header.c */
extern
void
globus_i_xio_http_header_copy(
    void **                             dest_key,
    void **                             dest_datum,
    void *                              src_key,
    void *                              src_datum);

extern
void
globus_i_xio_http_header_destroy(
    void *                              header);

extern
globus_result_t
globus_i_xio_http_header_parse(
    globus_i_xio_http_handle_t *        handle,
    globus_bool_t *                     done);

/* globus_xio_http_header_info.c */
extern
globus_result_t
globus_i_xio_http_header_info_init(
    globus_i_xio_http_header_info_t *   headers);

extern
void
globus_i_xio_http_header_info_destroy(
    globus_i_xio_http_header_info_t *   headers);

extern
globus_result_t
globus_i_xio_http_header_info_copy(
    globus_i_xio_http_header_info_t *   dest,
    const globus_i_xio_http_header_info_t *
                                        src);

extern
globus_result_t
globus_i_xio_http_header_info_set_header(
    globus_i_xio_http_header_info_t *   headers,
    const char *                        header_name,
    const char *                        header_value);

/* globus_xio_http_rfc2616.c */
extern
const char *
globus_i_xio_http_lookup_reason(
    int                                 code);

/* globus_xio_http_target.c */
extern
globus_i_xio_http_target_t *
globus_i_xio_http_target_new(void);

extern
globus_result_t
globus_i_xio_http_target_destroy(
    void *                              driver_target);

extern
globus_result_t
globus_i_xio_http_target_destroy_internal(
    globus_i_xio_http_target_t *        target);

/* globus_xio_http_request_t */
extern
globus_result_t
globus_i_xio_http_request_init(
    globus_i_xio_http_request_t *       request);

extern
globus_result_t
globus_i_xio_http_request_copy(
    globus_i_xio_http_request_t *       dest,
    const globus_i_xio_http_request_t * src);

extern
void
globus_i_xio_http_request_destroy(
    globus_i_xio_http_request_t *       request);

/* globus_xio_http_response.c */
extern
globus_result_t
globus_i_xio_http_response_init(
    globus_i_xio_http_response_t *      response);

extern
void
globus_i_xio_http_response_destroy(
    globus_i_xio_http_response_t *      response);


/* globus_xio_http_server.c */
extern
globus_result_t
globus_i_xio_http_accept(
    void *                              driver_server,
    void *                              driver_attr,
    globus_xio_operation_t              accept_op);

extern
void
globus_i_xio_http_server_open_callback(
    globus_xio_operation_t              op,
    globus_result_t                     result,
    void *                              user_arg);

extern
globus_result_t
globus_i_xio_http_server_write_response(
    globus_i_xio_http_handle_t *        http_handle,
    const globus_xio_iovec_t *          iovec,
    int                                 iovec_count,
    globus_xio_operation_t              op);

/* globus_xio_http_target.c */ 
extern
globus_result_t
globus_i_xio_http_target_init(
    void **                             out_driver_target,
    globus_xio_operation_t              target_op,
    const globus_xio_contact_t *        contact_info,
    void *                              driver_attr);

extern
globus_result_t
globus_i_xio_http_target_copy(
    globus_i_xio_http_target_t *        dest,
    const globus_i_xio_http_target_t *  src);

/* globus_xio_http_transform.c */
extern
globus_result_t
globus_i_xio_http_open(
    void *                              target,
    void *                              attr,
    globus_xio_operation_t              op);

extern
globus_result_t
globus_i_xio_http_read(
    void *                              handle,
    const globus_xio_iovec_t *          iovec,
    int                                 iovec_count,
    globus_xio_operation_t              op);

extern
globus_result_t
globus_i_xio_http_write(
    void *                                  handle,
    const globus_xio_iovec_t *              iovec,
    int                                     iovec_count,
    globus_xio_operation_t                  op);

extern
globus_result_t
globus_i_xio_http_parse_residue(
    globus_i_xio_http_handle_t *        handle);

extern
globus_result_t
globus_i_xio_http_write_chunk(
    globus_i_xio_http_handle_t *        http_handle,
    const globus_xio_iovec_t *          iovec,
    int                                 iovec_count,
    globus_xio_operation_t              op);

extern
void
globus_i_xio_http_write_callback(
    globus_xio_operation_t              op,
    globus_result_t                     result,
    globus_size_t                       nbytes,
    void *                              user_arg);

extern
globus_result_t
globus_i_xio_http_close(
    void *                              handle,
    void *                              attr,
    globus_xio_driver_handle_t          driver_handle,
    globus_xio_operation_t              op);

extern 
globus_result_t
globus_i_xio_http_close_internal(
    globus_i_xio_http_handle_t *        http_handle);

extern
void
globus_i_xio_http_close_callback(
    globus_xio_operation_t              operation,
    globus_result_t                     result,
    void *                              handle);

#define GlobusXIOHttpErrorObjParse(token, context)                          \
        globus_error_construct_error(                                       \
            GLOBUS_XIO_MODULE,                                              \
            GLOBUS_NULL,                                                    \
            GLOBUS_XIO_HTTP_ERROR_PARSE,                                    \
            "[%s:%d] Error parsing %s token at %s",                         \
            _xio_name, __LINE__, token, context)

#define GlobusXIOHttpErrorParse(token, context)                             \
    globus_error_put(                                                       \
        GlobusXIOHttpErrorObjParse(token, context))

#define GlobusXIOHttpErrorObjInvalidHeader(name, value)                     \
        globus_error_construct_error(                                       \
            GLOBUS_XIO_MODULE,                                              \
            GLOBUS_NULL,                                                    \
            GLOBUS_XIO_HTTP_ERROR_INVALID_HEADER,                           \
            "[%s:%d] Invalid %s header value %s",                           \
            _xio_name, __LINE__, name, value)

#define GlobusXIOHttpErrorInvalidHeader(name, value)                        \
    globus_error_put(                                                       \
        GlobusXIOHttpErrorObjInvalidHeader(name, value))

#define GlobusXIOHttpErrorObjNoEntity()                                     \
    globus_error_construct_error(                                           \
            GLOBUS_XIO_MODULE,                                              \
            GLOBUS_NULL,                                                    \
            GLOBUS_XIO_HTTP_ERROR_NO_ENTITY,                                \
            "[%s:%d] No entity to read or write",                           \
            _xio_name, __LINE__)
#define GlobusXIOHttpErrorNoEntity()                                        \
    globus_error_put(                                                       \
        GlobusXIOHttpErrorObjNoEntity())

#endif /* GLOBUS_DONT_DOCUMENT_INTERNAL */

#endif /* GLOBUS_I_XIO_HTTP_H */
