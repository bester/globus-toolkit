#ifndef GLOBUS_XIO_TCP_DRIVER_INCLUDE
#define GLOBUS_XIO_TCP_DRIVER_INCLUDE

#include "globus_xio_system.h"

/**
 *  possible commands for attr cntl
 */

#define GLOBUS_XIO_TCP_INVALID_HANDLE GLOBUS_XIO_SYSTEM_INVALID_HANDLE

typedef enum
{
    GLOBUS_XIO_TCP_ERROR_NO_ADDRS
} globus_xio_tcp_error_type_t;

typedef enum
{
    /**
     *  target/server attrs
     */
    /* globus_xio_system_handle_t       handle */
    GLOBUS_XIO_TCP_SET_HANDLE,
    /* globus_xio_system_handle_t *     handle_out */
    GLOBUS_XIO_TCP_GET_HANDLE,
    
    /**
     *  server attrs
     */
    /* const char *                     service_name */
    GLOBUS_XIO_TCP_SET_SERVICE,
    /* char **                          service_name_out */
    GLOBUS_XIO_TCP_GET_SERVICE,
    /* int                              listener_port */
    GLOBUS_XIO_TCP_SET_PORT,
    /* int *                            listener_port_out */
    GLOBUS_XIO_TCP_GET_PORT,
    /* int                              listener_backlog */
    GLOBUS_XIO_TCP_SET_BACKLOG,
    /* int *                            listener_backlog_out */
    GLOBUS_XIO_TCP_GET_BACKLOG,
    /* int                              listener_min_port */
    /* int                              listener_max_port */
    GLOBUS_XIO_TCP_SET_LISTEN_RANGE,
    /* int *                            listener_min_port_out */
    /* int *                            listener_max_port_out */
    GLOBUS_XIO_TCP_GET_LISTEN_RANGE,
    
    /**
     *  handle/server attrs
     */
    /* const char *                     interface */
    GLOBUS_XIO_TCP_SET_INTERFACE,
    /* char **                          interface_out */
    GLOBUS_XIO_TCP_GET_INTERFACE,
    /* globus_bool_t                    restrict_port */
    GLOBUS_XIO_TCP_SET_RESTRICT_PORT,
    /* globus_bool_t *                  restrict_port_out */
    GLOBUS_XIO_TCP_GET_RESTRICT_PORT,
    /* globus_bool_t                    resuseaddr */
    GLOBUS_XIO_TCP_SET_REUSEADDR,
    /* globus_bool_t *                  resuseaddr_out */
    GLOBUS_XIO_TCP_GET_REUSEADDR,
    
    /**
     *  handle attrs
     */
    /* int                              connector_min_port */
    /* int                              connector_max_port */
    GLOBUS_XIO_TCP_SET_CONNECT_RANGE,
    /* int *                            connector_min_port_out */
    /* int *                            connector_max_port_out */
    GLOBUS_XIO_TCP_GET_CONNECT_RANGE,
    
    /**
     *  handle attrs/cntls
     */
    /* globus_bool_t                    keepalive */
    GLOBUS_XIO_TCP_SET_KEEPALIVE,
    /* globus_bool_t *                  keepalive_out */
    GLOBUS_XIO_TCP_GET_KEEPALIVE,
    /* globus_bool_t                    linger */
    /* int                              linger_time */
    GLOBUS_XIO_TCP_SET_LINGER,
    /* globus_bool_t *                  linger_out */
    /* int *                            linger_time_out */
    GLOBUS_XIO_TCP_GET_LINGER,
    /* globus_bool_t                    oobinline */
    GLOBUS_XIO_TCP_SET_OOBINLINE,
    /* globus_bool_t *                  oobinline_out */
    GLOBUS_XIO_TCP_GET_OOBINLINE,
    /* int                              sndbuf */
    GLOBUS_XIO_TCP_SET_SNDBUF,
    /* int *                            sndbuf_out */
    GLOBUS_XIO_TCP_GET_SNDBUF,
    /* int                              rcvbuf */
    GLOBUS_XIO_TCP_SET_RCVBUF,
    /* int *                            rcvbuf_out */
    GLOBUS_XIO_TCP_GET_RCVBUF,
    /* globus_bool_t                    nodelay */
    GLOBUS_XIO_TCP_SET_NODELAY,
    /* globus_bool_t *                  nodelay_out */
    GLOBUS_XIO_TCP_GET_NODELAY,
    
    /**
     * data descriptors
     */
    /* int                              send_flags */
    GLOBUS_XIO_TCP_SET_SEND_FLAGS,
    /* int *                            send_flags_out */
    GLOBUS_XIO_TCP_GET_SEND_FLAGS,
    
    /**
     * handle/server/target cntls
     */
    /* char **                          contact_string_out */
    GLOBUS_XIO_TCP_GET_LOCAL_CONTACT,
    /* char **                          contact_string_out */
    GLOBUS_XIO_TCP_GET_LOCAL_NUMERIC_CONTACT,
    /* char **                          contact_string_out */
    GLOBUS_XIO_TCP_GET_REMOTE_CONTACT,
    /* char **                          contact_string_out */
    GLOBUS_XIO_TCP_GET_REMOTE_NUMERIC_CONTACT
    
} globus_xio_tcp_cmd_t;

typedef enum
{
    GLOBUS_XIO_TCP_SEND_OOB = MSG_OOB
} globus_xio_tcp_send_flags_t;

#endif
