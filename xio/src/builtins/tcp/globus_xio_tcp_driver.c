#include "globus_xio_driver.h"
#include "globus_xio_tcp_driver.h"
#include "version.h"
#include <netinet/tcp.h>
#include <fcntl.h>

GlobusDebugDefine(GLOBUS_XIO_TCP);

#define GlobusXIOTcpDebugPrintf(level, message)                            \
    GlobusDebugPrintf(GLOBUS_XIO_TCP, level, message)

#define GlobusXIOTcpDebugEnter()                                           \
    GlobusXIOTcpDebugPrintf(                                               \
        GLOBUS_L_XIO_TCP_DEBUG_TRACE,                                      \
        ("[%s] Entering\n", _xio_name))
        
#define GlobusXIOTcpDebugExit()                                            \
    GlobusXIOTcpDebugPrintf(                                               \
        GLOBUS_L_XIO_TCP_DEBUG_TRACE,                                      \
        ("[%s] Exiting\n", _xio_name))

#define GlobusXIOTcpDebugExitWithError()                                   \
    GlobusXIOTcpDebugPrintf(                                               \
        GLOBUS_L_XIO_TCP_DEBUG_TRACE,                                      \
        ("[%s] Exiting with error\n", _xio_name))

enum globus_l_xio_error_levels
{
    GLOBUS_L_XIO_TCP_DEBUG_TRACE       = 1,
    GLOBUS_L_XIO_TCP_DEBUG_INFO        = 2
};

static
int
globus_l_xio_tcp_activate(void);


static
int
globus_l_xio_tcp_deactivate(void);

GlobusXIODefineModule(tcp) =
{
    "globus_xio_tcp",
    globus_l_xio_tcp_activate,
    globus_l_xio_tcp_deactivate,
    GLOBUS_NULL,
    GLOBUS_NULL,
    &local_version
};

#define GlobusXIOTcpErrorNoAddrs()                                          \
    globus_error_put(                                                       \
        globus_error_construct_error(                                       \
            GlobusXIOMyModule(tcp),                                         \
            GLOBUS_NULL,                                                    \
            GLOBUS_XIO_TCP_ERROR_NO_ADDRS,                                  \
            __FILE__,                                                       \
            _xio_name,                                                      \
            __LINE__,                                                       \
            "No addrs for INET family"))


#define GlobusIXIOTcpCloseFd(fd)                                            \
    do                                                                      \
    {                                                                       \
        int                             _rc;                                \
        int                             _fd;                                \
                                                                            \
        _fd = (fd);                                                         \
        do                                                                  \
        {                                                                   \
            _rc = close(_fd);                                               \
        } while(_rc < 0 && errno == EINTR);                                 \
                                                                            \
        (fd) = -1;                                                          \
    } while(0)

/*
 *  attribute structure
 */
typedef struct
{
    /* server attrs */
    char *                              listener_serv;
    int                                 listener_port;
    int                                 listener_backlog;
    int                                 listener_min_port;
    int                                 listener_max_port;
    
    /* handle/server attrs */
    globus_xio_system_handle_t          handle;
    char *                              bind_address;
    globus_bool_t                       restrict_port;
    globus_bool_t                       resuseaddr;
    globus_bool_t                       no_ipv6;
    
    /* handle attrs */
    globus_bool_t                       keepalive;
    globus_bool_t                       linger;
    int                                 linger_time;
    globus_bool_t                       oobinline;
    int                                 sndbuf;
    int                                 rcvbuf;
    globus_bool_t                       nodelay;
    int                                 connector_min_port;
    int                                 connector_max_port;
    
    /* data descriptor */
    int                                 send_flags;
    
    globus_bool_t                       global;
} globus_l_attr_t;

/* default attr (never put any string literals in here, else they may be
 * freed by the affect global attr stuff)
 */
static globus_l_attr_t                  globus_l_xio_tcp_attr_default =
{
    GLOBUS_NULL,                        /* listener_serv */
    0,                                  /* listener_port */
    -1,                                 /* listener_backlog (SOMAXCONN) */
    0,                                  /* listener_min_port */
    0,                                  /* listener_max_port */
    
    GLOBUS_XIO_TCP_INVALID_HANDLE,      /* handle   */ 
    GLOBUS_NULL,                        /* bind_address */
    GLOBUS_TRUE,                        /* restrict_port */
    GLOBUS_FALSE,                       /* reuseaddr */
    GLOBUS_FALSE,                       /* no_ipv6 */
    
    GLOBUS_FALSE,                       /* keepalive */  
    GLOBUS_FALSE,                       /* linger */     
    0,                                  /* linger_time */
    GLOBUS_FALSE,                       /* oobinline */  
    0,                                  /* sndbuf (system default) */     
    0,                                  /* rcvbuf (system default) */     
    GLOBUS_FALSE,                       /* nodelay */    
    0,                                  /* connector_min_port */
    0,                                  /* connector_max_port */
    
    0,                                  /* send_flags */
    GLOBUS_FALSE                        /* global */
};

static int                              globus_l_xio_tcp_port_range_state_file;
static globus_mutex_t                   globus_l_xio_tcp_port_range_state_lock;

/*
 *  server structure
 */
typedef struct
{
    globus_xio_system_handle_t          listener_handle;
    globus_bool_t                       converted;
} globus_l_server_t;

/*
 *  handle structure
 */
typedef struct
{
    globus_xio_system_handle_t          handle;
    globus_bool_t                       converted;
    globus_object_t *                   connection_error;
    globus_xio_operation_t              read_op;
    globus_xio_operation_t              write_op;
} globus_l_handle_t;

static
globus_bool_t
globus_l_xio_tcp_get_env_pair(
    const char *                        env_name,
    int *                               min,
    int *                               max)
{
    char *                              min_max;
    GlobusXIOName(globus_l_xio_tcp_get_env_pair);
    
    min_max = globus_module_getenv(env_name);

    if(min_max && sscanf(min_max, " %d , %d", min, max) == 2)
    {
        return GLOBUS_TRUE;
    }

    return GLOBUS_FALSE;
}

static
void
globus_l_xio_tcp_file_close(void)
{
    int                                 rc;
    
    if(globus_l_xio_tcp_port_range_state_file < 0)
    {
        return;
    }
    
    do
    {
        close(globus_l_xio_tcp_port_range_state_file);
    } while(rc < 0 && errno == EINTR);
    
    globus_l_xio_tcp_port_range_state_file = -1;
}

static
void
globus_l_xio_tcp_file_lock(void)
{
    int                                 rc;
    struct flock                        fl;
    
    globus_mutex_lock(&globus_l_xio_tcp_port_range_state_lock);
    
    if(globus_l_xio_tcp_port_range_state_file >= 0)
    {
        memset(&fl, 0, sizeof (fl));
        fl.l_whence = SEEK_SET;
        fl.l_type = F_WRLCK;
        do
        {
            rc = fcntl(globus_l_xio_tcp_port_range_state_file, F_SETLKW, &fl);
        } while(rc < 0 && errno == EINTR);
        
        if(rc < 0)
        {
            fprintf(stderr, "Unable to lock state file: %s\n",
                strerror(errno));
            globus_l_xio_tcp_file_close();
        }
    }
}

static
void
globus_l_xio_tcp_file_unlock()
{
    int                                 rc;
    struct flock                        fl;
    
    if(globus_l_xio_tcp_port_range_state_file >= 0)
    {
        memset(&fl, 0, sizeof (fl));
        fl.l_whence = SEEK_SET;
        fl.l_type = F_UNLCK;
        do
        {
            rc = fcntl(globus_l_xio_tcp_port_range_state_file, F_SETLK, &fl);
        } while(rc < 0 && errno == EINTR);
        
        if(rc < 0)
        {
            fprintf(stderr, "Unable to unlock state file: %s\n",
                strerror(errno));
            globus_l_xio_tcp_file_close();
        }
    }
    
    globus_mutex_unlock(&globus_l_xio_tcp_port_range_state_lock);
}

static
void
globus_l_xio_tcp_file_open(
    const char *                        pathname)
{
    do
    {
        globus_l_xio_tcp_port_range_state_file = 
            open(pathname, O_CREAT | O_RDWR,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    } while(globus_l_xio_tcp_port_range_state_file < 0 && errno == EINTR);
    
    if(globus_l_xio_tcp_port_range_state_file < 0)
    {
        fprintf(stderr, "Could not open lock file %s: %s\n", 
            pathname, strerror(errno));
    }
    else
    {
        /* test locking -- if they fail, they will close the fd */
        globus_l_xio_tcp_file_lock();
        globus_l_xio_tcp_file_unlock();
    }
}

static
int
globus_l_xio_tcp_file_read_port(void)
{
    char                                buf[6];
    int                                 rc;
    int                                 nbytes = 0;
    int                                 port = -1;
    
    if(globus_l_xio_tcp_port_range_state_file < 0)
    {
        return -1;
    }
    
    if(lseek(globus_l_xio_tcp_port_range_state_file, 0, SEEK_SET) == 0)
    {
        do
        {
            if((rc = read(
                globus_l_xio_tcp_port_range_state_file,
                buf + nbytes, 5 - nbytes)) > 0)
            {
                nbytes += rc;
            }
        } while((rc > 0 && nbytes < 5) || (rc < 0 && errno == EINTR));
        
        if(nbytes == 5)
        {
            buf[5] = 0;
            port = atoi(buf);
        }
    }
    
    return port;
}

static
void
globus_l_xio_tcp_file_write_port(
    int                                 port)
{
    char                                buf[6];
    int                                 rc = -1;
    int                                 nbytes = 0;
    
    if(globus_l_xio_tcp_port_range_state_file < 0)
    {
        return;
    }
    
    snprintf(buf, 6, "%.5d", port);
    if(lseek(globus_l_xio_tcp_port_range_state_file, 0, SEEK_SET) == 0)
    {
        do
        {
            if((rc = write(
                globus_l_xio_tcp_port_range_state_file,
                buf + nbytes, 6 - nbytes)) > 0)
            {
                nbytes += rc;
            }
        } while((rc >= 0 && nbytes < 6) || (rc < 0 && errno == EINTR));
    }
    
    if(rc < 0)
    {
        fprintf(stderr, "Unable to update state file: %s\n", strerror(errno));
        globus_l_xio_tcp_file_close();
    }
}

/*
 *  initialize a driver attribute
 */
static
globus_result_t
globus_l_xio_tcp_attr_init(
    void **                             out_attr)
{
    globus_l_attr_t *                   attr;
    globus_result_t                     result;
    GlobusXIOName(globus_l_xio_tcp_attr_init);
    
    GlobusXIOTcpDebugEnter();
    /*
     *  create a tcp attr structure and intialize its values
     */
    attr = (globus_l_attr_t *) globus_malloc(sizeof(globus_l_attr_t));
    if(!attr)
    {
        result = GlobusXIOErrorMemory("attr");
        goto error_attr;
    }
    
    memcpy(attr, &globus_l_xio_tcp_attr_default, sizeof(globus_l_attr_t));
    *out_attr = attr;
    
    GlobusXIOTcpDebugExit();
    return GLOBUS_SUCCESS;

error_attr:
    GlobusXIOTcpDebugExitWithError();
    return result;
}

/*
 *  modify the attribute structure
 */
static
globus_result_t
globus_l_xio_tcp_attr_cntl(
    void *                              driver_attr,
    int                                 cmd,
    va_list                             ap)
{
    globus_l_attr_t *                   attr;
    globus_xio_system_handle_t *        out_handle;
    char **                             out_string;
    int *                               out_int;
    globus_bool_t *                     out_bool;
    globus_result_t                     result;
    GlobusXIOName(globus_l_xio_tcp_attr_cntl);

    GlobusXIOTcpDebugEnter();
    attr = (globus_l_attr_t *) driver_attr;
    if(attr->global && cmd != GLOBUS_XIO_TCP_AFFECT_ATTR_DEFAULTS)
    {
        attr = &globus_l_xio_tcp_attr_default;
    }
    
    switch(cmd)
    {
      case GLOBUS_XIO_TCP_AFFECT_ATTR_DEFAULTS:
        attr->global = va_arg(ap, globus_bool_t);
        break;
        
      /**
       *  server attrs
       */
      /* char *                         service_name */
      case GLOBUS_XIO_TCP_SET_SERVICE:
        if(attr->listener_serv)
        {
            globus_free(attr->listener_serv);
        }
        
        attr->listener_serv = va_arg(ap, char *);
        if(attr->listener_serv)
        {
            attr->listener_serv = globus_libc_strdup(attr->listener_serv);
            if(!attr->listener_serv)
            {
                result = GlobusXIOErrorMemory("listener_serv");
                goto error_memory;
            }
        }
        break;
      
      /* char **                        service_name_out */
      case GLOBUS_XIO_TCP_GET_SERVICE:
        out_string = va_arg(ap, char **);
        if(attr->listener_serv)
        {
            *out_string = globus_libc_strdup(attr->listener_serv);
            if(!*out_string)
            {
                result = GlobusXIOErrorMemory("listener_serv_out");
                goto error_memory;
            }
        }
        else
        {
            *out_string = GLOBUS_NULL;
        }
        break;
      
      /* int                            listener_port */
      case GLOBUS_XIO_TCP_SET_PORT:
        attr->listener_port = va_arg(ap, int);
        break;
      
      /* int *                          listener_port_out */
      case GLOBUS_XIO_TCP_GET_PORT:
        out_int = va_arg(ap, int *);
        *out_int = attr->listener_port;
        break;
      
      /* int                            listener_backlog */
      case GLOBUS_XIO_TCP_SET_BACKLOG:
        attr->listener_backlog = va_arg(ap, int);
        break;
      
      /* int *                          listener_backlog_out */
      case GLOBUS_XIO_TCP_GET_BACKLOG:
        out_int = va_arg(ap, int *);
        *out_int = attr->listener_backlog;
        break;
      
      /* int                            listener_min_port */
      /* int                            listener_max_port */
      case GLOBUS_XIO_TCP_SET_LISTEN_RANGE:
        attr->listener_min_port = va_arg(ap, int);
        attr->listener_max_port = va_arg(ap, int);
        break;
      
      /* int *                          listener_min_port_out */
      /* int *                          listener_max_port_out */
      case GLOBUS_XIO_TCP_GET_LISTEN_RANGE:
        out_int = va_arg(ap, int *);
        *out_int = attr->listener_min_port;
        out_int = va_arg(ap, int *);
        *out_int = attr->listener_max_port;
        break;
        
      /**
       *  handle/server attrs
       */
      /* globus_xio_system_handle_t     handle */
      case GLOBUS_XIO_TCP_SET_HANDLE:
        attr->handle = va_arg(ap, globus_xio_system_handle_t);
        break;
      
      /* globus_xio_system_handle_t *   handle_out */
      case GLOBUS_XIO_TCP_GET_HANDLE:
        out_handle = va_arg(ap, globus_xio_system_handle_t *);
        *out_handle = attr->handle;
        break;
       
      /* char *                         interface */
      case GLOBUS_XIO_TCP_SET_INTERFACE:
        if(attr->bind_address)
        {
            globus_free(attr->bind_address);
        }
        
        attr->bind_address = va_arg(ap, char *);
        if(attr->bind_address)
        {
            attr->bind_address = globus_libc_strdup(attr->bind_address);
            if(!attr->bind_address)
            {
                result = GlobusXIOErrorMemory("bind_address");
                goto error_memory;
            }
        }
        break;
      
      /* char **                        interface_out */
      case GLOBUS_XIO_TCP_GET_INTERFACE:
        out_string = va_arg(ap, char **);
        if(attr->bind_address)
        {
            *out_string = globus_libc_strdup(attr->bind_address);
            if(!*out_string)
            {
                result = GlobusXIOErrorMemory("bind_address_out");
                goto error_memory;
            }
        }
        else
        {
            *out_string = GLOBUS_NULL;
        }
        break;
      
      /* globus_bool_t                  restrict_port */
      case GLOBUS_XIO_TCP_SET_RESTRICT_PORT:
        attr->restrict_port = va_arg(ap, globus_bool_t);
        break;
      
      /* globus_bool_t *                restrict_port_out */
      case GLOBUS_XIO_TCP_GET_RESTRICT_PORT:
        out_bool = va_arg(ap, globus_bool_t *);
        *out_bool = attr->restrict_port;
        break;
      
      /* globus_bool_t                  resuseaddr */
      case GLOBUS_XIO_TCP_SET_REUSEADDR:
        attr->resuseaddr = va_arg(ap, globus_bool_t);
        break;
        
      /* globus_bool_t *                resuseaddr_out */
      case GLOBUS_XIO_TCP_GET_REUSEADDR:
        out_bool = va_arg(ap, globus_bool_t *);
        *out_bool = attr->resuseaddr;
        break;
      
      /* globus_bool_t                  no_ipv6 */
      case GLOBUS_XIO_TCP_SET_NO_IPV6:
        attr->no_ipv6 = va_arg(ap, globus_bool_t);
        break;
        
      /* globus_bool_t *                no_ipv6_out */
      case GLOBUS_XIO_TCP_GET_NO_IPV6:
        out_bool = va_arg(ap, globus_bool_t *);
        *out_bool = attr->no_ipv6;
        break;
        
      /**
       *  handle attrs
       */
      /* globus_bool_t                  keepalive */
      case GLOBUS_XIO_TCP_SET_KEEPALIVE:
        attr->keepalive = va_arg(ap, globus_bool_t);
        break;
        
      /* globus_bool_t *                keepalive_out */
      case GLOBUS_XIO_TCP_GET_KEEPALIVE:
        out_bool = va_arg(ap, globus_bool_t *);
        *out_bool = attr->keepalive;
        break;
        
      /* globus_bool_t                  linger */
      /* int                            linger_time */
      case GLOBUS_XIO_TCP_SET_LINGER:
        attr->linger = va_arg(ap, globus_bool_t);
        attr->linger_time = va_arg(ap, int);
        break;
        
      /* globus_bool_t *                linger_out */
      /* int *                          linger_time_out */
      case GLOBUS_XIO_TCP_GET_LINGER:
        out_bool = va_arg(ap, globus_bool_t *);
        *out_bool = attr->linger;
        out_int = va_arg(ap, int *);
        *out_int = attr->linger_time;
        break;
        
      /* globus_bool_t                  oobinline */
      case GLOBUS_XIO_TCP_SET_OOBINLINE:
        attr->oobinline = va_arg(ap, globus_bool_t);
        break;
        
      /* globus_bool_t *                oobinline_out */
      case GLOBUS_XIO_TCP_GET_OOBINLINE:
        out_bool = va_arg(ap, globus_bool_t *);
        *out_bool = attr->oobinline;
        break;
        
      /* int                            sndbuf */
      case GLOBUS_XIO_TCP_SET_SNDBUF:
        attr->sndbuf = va_arg(ap, int);
        break;
        
      /* int *                          sndbuf_out */
      case GLOBUS_XIO_TCP_GET_SNDBUF:
        out_int = va_arg(ap, int *);
        *out_int = attr->sndbuf;
        break;
        
      /* int                            rcvbuf */
      case GLOBUS_XIO_TCP_SET_RCVBUF:
        attr->rcvbuf = va_arg(ap, int);
        break;
        
      /* int *                          rcvbuf_out */
      case GLOBUS_XIO_TCP_GET_RCVBUF:
        out_int = va_arg(ap, int *);
        *out_int = attr->rcvbuf;
        break;
        
      /* globus_bool_t                  nodelay */
      case GLOBUS_XIO_TCP_SET_NODELAY:
        attr->nodelay = va_arg(ap, globus_bool_t);
        break;
        
      /* globus_bool_t *                nodelay_out */
      case GLOBUS_XIO_TCP_GET_NODELAY:
        out_bool = va_arg(ap, globus_bool_t *);
        *out_bool = attr->nodelay;
        break;
        
      /* int                            connector_min_port */
      /* int                            connector_max_port */
      case GLOBUS_XIO_TCP_SET_CONNECT_RANGE:
        attr->connector_min_port = va_arg(ap, int);
        attr->connector_max_port = va_arg(ap, int);
        break;
      
      /* int *                          connector_min_port_out */
      /* int *                          connector_max_port_out */
      case GLOBUS_XIO_TCP_GET_CONNECT_RANGE:
        out_int = va_arg(ap, int *);
        *out_int = attr->connector_min_port;
        out_int = va_arg(ap, int *);
        *out_int = attr->connector_max_port;
        break;
      
      /**
       * data descriptors
       */
      /* int                            send_flags */
      case GLOBUS_XIO_TCP_SET_SEND_FLAGS:
        attr->send_flags = va_arg(ap, int);
        break;
        
      /* int *                          send_flags_out */
      case GLOBUS_XIO_TCP_GET_SEND_FLAGS:
        out_int = va_arg(ap, int *);
        *out_int = attr->send_flags;
        break;
        
      default:
        result = GlobusXIOErrorInvalidCommand(cmd);
        goto error_invalid;
        break;
    }
    
    GlobusXIOTcpDebugExit();
    return GLOBUS_SUCCESS;

error_memory:
error_invalid:
    GlobusXIOTcpDebugExitWithError();
    return result;
}

/*
 *  copy an attribute structure
 */
static
globus_result_t
globus_l_xio_tcp_attr_copy(
    void **                             dst,
    void *                              src)
{
    globus_l_attr_t *                   attr;
    globus_result_t                     result;
    GlobusXIOName(globus_l_xio_tcp_attr_copy);
    
    GlobusXIOTcpDebugEnter();
    attr = (globus_l_attr_t *) globus_malloc(sizeof(globus_l_attr_t));
    if(!attr)
    {
        result = GlobusXIOErrorMemory("attr");
        goto error_attr;
    }
    
    memcpy(attr, src, sizeof(globus_l_attr_t));
    if(attr->bind_address)
    {
        attr->bind_address = globus_libc_strdup(attr->bind_address);
        if(!attr->bind_address)
        {
            result = GlobusXIOErrorMemory("bind_address");
            goto error_bind_address;
        }
    }
    if(attr->listener_serv)
    {
        attr->listener_serv = globus_libc_strdup(attr->listener_serv);
        if(!attr->listener_serv)
        {
            result = GlobusXIOErrorMemory("listener_serv");
            goto error_listener_serv;
        }
    }
    
    /* copies do not inherit the affect_global */
    attr->global = GLOBUS_FALSE;
    
    *dst = attr;
    
    GlobusXIOTcpDebugExit();
    return GLOBUS_SUCCESS;

error_listener_serv:
    if(attr->bind_address)
    {
        globus_free(attr->bind_address);
    }
    
error_bind_address:
    globus_free(attr);
    
error_attr:
    GlobusXIOTcpDebugExitWithError();
    return result;
}

/*
 *  destroy an attr structure
 */
static
globus_result_t
globus_l_xio_tcp_attr_destroy(
    void *                              driver_attr)
{
    globus_l_attr_t *                   attr;
    GlobusXIOName(globus_l_xio_tcp_attr_destroy);
    
    GlobusXIOTcpDebugEnter();
    attr = (globus_l_attr_t *) driver_attr;
    if(attr->bind_address)
    {
        globus_free(attr->bind_address);
    }
    if(attr->listener_serv)
    {
        globus_free(attr->listener_serv);
    }
    
    globus_free(driver_attr);
    
    GlobusXIOTcpDebugExit();
    return GLOBUS_SUCCESS;
}

static
globus_result_t
globus_l_xio_tcp_apply_handle_attrs(
    const globus_l_attr_t *             attr,
    int                                 fd,
    globus_bool_t                       do_bind_attrs,
    globus_bool_t                       converted)
{
    globus_result_t                     result;
    int                                 int_one = 1;
    GlobusXIOName(globus_l_xio_tcp_apply_handle_attrs);
    
    GlobusXIOTcpDebugEnter();
    
    if(!converted)
    {
        /* all handles created by me are closed on exec */
        fcntl(fd, F_SETFD, FD_CLOEXEC);
    }
        
    if(do_bind_attrs)
    {
        if(attr->resuseaddr &&
            setsockopt(
                fd, SOL_SOCKET, SO_REUSEADDR, &int_one, sizeof(int_one)) < 0)
        {
            result = GlobusXIOErrorSystemError("setsockopt", errno);
            goto error_sockopt;
        }
    }
    
    if(attr->keepalive &&
       setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &int_one, sizeof(int_one)) < 0)
    {
        result = GlobusXIOErrorSystemError("setsockopt", errno);
        goto error_sockopt;
    }
    
    if(attr->linger)
    {
        struct linger                   linger;
        
        linger.l_onoff = 1;
        linger.l_linger = attr->linger_time;
        
        if(setsockopt(fd, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger)) < 0)
        {
            result = GlobusXIOErrorSystemError("setsockopt", errno);
            goto error_sockopt;
        }
    }
    
    if(attr->oobinline &&
       setsockopt(fd, SOL_SOCKET, SO_OOBINLINE, &int_one, sizeof(int_one)) < 0)
    {
        result = GlobusXIOErrorSystemError("setsockopt", errno);
        goto error_sockopt;
    }

#ifdef TCP_RFC1323
    if(attr->sndbuf || attr->rcvbuf)
    {
        /* On AIX, RFC 1323 extensions can be set system-wide,
         * using the 'no' network options command. But we can also set them
         * per-socket, so let's try just in case. 
         */
        if(setsockopt(
            fd, IPPROTO_TCP, TCP_RFC1323, &int_one, sizeof(int_one)) < 0)
        {
            result = GlobusXIOErrorSystemError("setsockopt", errno);
            goto error_sockopt;
        }
    }
#endif

    if(attr->sndbuf &&
        setsockopt(
           fd, SOL_SOCKET, SO_SNDBUF, &attr->sndbuf, sizeof(attr->sndbuf)) < 0)
    {
        result = GlobusXIOErrorSystemError("setsockopt", errno);
        goto error_sockopt;
    }
    
    if(attr->rcvbuf &&
        setsockopt(
           fd, SOL_SOCKET, SO_RCVBUF, &attr->rcvbuf, sizeof(attr->rcvbuf)) < 0)
    {
        result = GlobusXIOErrorSystemError("setsockopt", errno);
        goto error_sockopt;
    }
    
    if(attr->nodelay &&
       setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &int_one, sizeof(int_one)) < 0)
    {
        result = GlobusXIOErrorSystemError("setsockopt", errno);
        goto error_sockopt;
    }
    
    GlobusXIOTcpDebugExit();
    return GLOBUS_SUCCESS;

error_sockopt:
    GlobusXIOTcpDebugExitWithError();
    return result;
}

static
globus_result_t
globus_l_xio_tcp_contact_string(
    globus_xio_system_handle_t          handle,
    int                                 cmd,
    char **                             contact_string)
{
    globus_result_t                     result;
    globus_sockaddr_t                   sock_name;
    globus_socklen_t                    sock_len;
    int                                 flags;
    GlobusXIOName(globus_l_xio_tcp_contact_string);
    
    GlobusXIOTcpDebugEnter();
    sock_len = sizeof(sock_name);
    flags = 0;
    
    switch(cmd)
    {
      case GLOBUS_XIO_TCP_GET_LOCAL_NUMERIC_CONTACT:
      case GLOBUS_XIO_GET_LOCAL_NUMERIC_CONTACT:
        flags |= GLOBUS_LIBC_ADDR_NUMERIC;
        /* fall through */
        
      case GLOBUS_XIO_TCP_GET_LOCAL_CONTACT:
      case GLOBUS_XIO_GET_LOCAL_CONTACT:
        if(getsockname(handle, (struct sockaddr *) &sock_name, &sock_len) < 0)
        {
            result = GlobusXIOErrorSystemError("getsockname", errno);
            goto error_sockopt;
        }
        break;
        
      case GLOBUS_XIO_TCP_GET_REMOTE_NUMERIC_CONTACT:
      case GLOBUS_XIO_GET_REMOTE_NUMERIC_CONTACT:
        flags |= GLOBUS_LIBC_ADDR_NUMERIC;
        /* fall through */
        
      case GLOBUS_XIO_TCP_GET_REMOTE_CONTACT:
      case GLOBUS_XIO_GET_REMOTE_CONTACT:
        if(getpeername(handle, (struct sockaddr *) &sock_name, &sock_len) < 0)
        {
            result = GlobusXIOErrorSystemError("getpeername", errno);
            goto error_sockopt;
        }
        break;
      
      default:
        globus_assert(0 && "Unexpected command");
        result = GlobusXIOErrorInvalidCommand(cmd);
        goto error_sockopt;
    }

    result = globus_libc_addr_to_contact_string(
        &sock_name,
        flags,
        contact_string);
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusXIOErrorWrapFailed(
            "globus_libc_addr_to_contact_string", result);
        goto error_contact;
    }
    
    GlobusXIOTcpDebugExit();
    return GLOBUS_SUCCESS;

error_contact:
error_sockopt:
    GlobusXIOTcpDebugExitWithError();
    return result;
}

static
globus_result_t
globus_l_xio_tcp_link_cntl(
    void *                              driver_link,
    int                                 cmd,
    va_list                             ap)
{
    globus_xio_system_handle_t *        accepted_handle;
    globus_result_t                     result;
    char **                             out_string;
    globus_xio_system_handle_t *        out_handle;
    GlobusXIOName(globus_l_xio_tcp_link_cntl);

    GlobusXIOTcpDebugEnter();
    accepted_handle = (globus_xio_system_handle_t *) driver_link;
    switch(cmd)
    {
      /* globus_xio_system_handle_t *   handle_out */
      case GLOBUS_XIO_TCP_GET_HANDLE:
        out_handle = va_arg(ap, globus_xio_system_handle_t *);
        *out_handle = *accepted_handle;
        break;
        
      /* char **                        contact_string_out */
      case GLOBUS_XIO_TCP_GET_LOCAL_NUMERIC_CONTACT:
      case GLOBUS_XIO_TCP_GET_LOCAL_CONTACT:
      case GLOBUS_XIO_TCP_GET_REMOTE_NUMERIC_CONTACT:
      case GLOBUS_XIO_TCP_GET_REMOTE_CONTACT:
      case GLOBUS_XIO_GET_LOCAL_NUMERIC_CONTACT:
      case GLOBUS_XIO_GET_LOCAL_CONTACT:
      case GLOBUS_XIO_GET_REMOTE_NUMERIC_CONTACT:
      case GLOBUS_XIO_GET_REMOTE_CONTACT:
        out_string = va_arg(ap, char **);
        result = globus_l_xio_tcp_contact_string(
            *accepted_handle, cmd, out_string);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusXIOErrorWrapFailed(
                "globus_l_xio_tcp_contact_string", result);
            goto error_contact;
        }
        break;
        
      default:
        result = GlobusXIOErrorInvalidCommand(cmd);
        goto error_invalid;
        break;
    }
    
    GlobusXIOTcpDebugExit();
    return GLOBUS_SUCCESS;

error_invalid:
error_contact:
    GlobusXIOTcpDebugExitWithError();
    return result;
}

/*
 *  destroy the link structure
 */
static
globus_result_t
globus_l_xio_tcp_link_destroy(
    void *                              driver_link)
{
    globus_xio_system_handle_t *        accepted_handle;
    GlobusXIOName(globus_l_xio_tcp_link_destroy);
    
    GlobusXIOTcpDebugEnter();
    accepted_handle = (globus_xio_system_handle_t *) driver_link;
    
    if(*accepted_handle != GLOBUS_XIO_TCP_INVALID_HANDLE)
    {
        GlobusIXIOTcpCloseFd(*accepted_handle);
    }
    
    globus_free(accepted_handle);
    
    GlobusXIOTcpDebugExit();
    return GLOBUS_SUCCESS;
}

static
globus_result_t
globus_l_xio_tcp_bind(
    int                                 fd,
    const struct sockaddr *             addr,
    int                                 addr_len,
    int                                 min_port,
    int                                 max_port,
    globus_bool_t                       listener)
{
    int                                 port;
    globus_bool_t                       done;
    globus_sockaddr_t                   myaddr;
    globus_result_t                     result;
    int                                 stop_port;
    globus_bool_t                       unlock = GLOBUS_FALSE;
    GlobusXIOName(globus_l_xio_tcp_bind);
    
    GlobusXIOTcpDebugEnter();
    GlobusLibcSockaddrGetPort(*addr, port);
    
    if(port == 0)
    {
        port = min_port;
        stop_port = max_port;
                    
        if(listener &&
            min_port == globus_l_xio_tcp_attr_default.listener_min_port &&
            max_port == globus_l_xio_tcp_attr_default.listener_max_port &&
            globus_l_xio_tcp_port_range_state_file >= 0)
        {
            int                         tmpport;
            
            unlock = GLOBUS_TRUE;
            globus_l_xio_tcp_file_lock();
            tmpport = globus_l_xio_tcp_file_read_port();
            if(tmpport < max_port && tmpport >= min_port)
            {
                port = tmpport + 1;
                stop_port = tmpport;
            }
        }
    }
    else
    {
        stop_port = port;
    }
    
    done = GLOBUS_FALSE;
    do
    {
        GlobusLibcSockaddrCopy(myaddr, *addr, addr_len);
        GlobusLibcSockaddrSetPort(myaddr, port);
        
        if(bind(
            fd,
            (struct sockaddr *) &myaddr,
            GlobusLibcSockaddrLen(&myaddr)) < 0)
        {
            if(port++ == stop_port)
            {
                result = GlobusXIOErrorSystemError("bind", errno);
                goto error_bind;
            }
            else if(port > max_port)
            {
                port = min_port;
            }
        }
        else
        {
            done = GLOBUS_TRUE;
        }
    } while(!done);
    
    if(unlock)
    {
        globus_l_xio_tcp_file_write_port(port);
        globus_l_xio_tcp_file_unlock();
    }
    
    GlobusXIOTcpDebugExit();
    return GLOBUS_SUCCESS;
    
error_bind:
    if(unlock)
    {
        globus_l_xio_tcp_file_unlock();
    }
    GlobusXIOTcpDebugExitWithError();
    return result;
}

static
globus_result_t
globus_l_xio_tcp_create_listener(
    globus_l_server_t *                 server,
    globus_l_attr_t *                   attr)
{
    globus_result_t                     result;
    globus_addrinfo_t *                 save_addrinfo;
    globus_addrinfo_t *                 addrinfo;
    globus_addrinfo_t                   addrinfo_hints;
    char                                portbuf[10];
    char *                              port;
    int                                 fd;
    int                                 save_errno;
    GlobusXIOName(globus_l_xio_tcp_create_listener);
    
    GlobusXIOTcpDebugEnter();
    save_errno = 0;
    
    if(attr->listener_serv)
    {
        port = attr->listener_serv;
    }
    else
    {
        snprintf(portbuf, sizeof(portbuf), "%d", attr->listener_port);
        port = portbuf;
    }
    
    /* setup hints for types of connectable sockets we want */
    memset(&addrinfo_hints, 0, sizeof(globus_addrinfo_t));
    addrinfo_hints.ai_flags = GLOBUS_AI_PASSIVE;
    addrinfo_hints.ai_family = attr->no_ipv6 ? PF_INET : PF_UNSPEC;
    addrinfo_hints.ai_socktype = SOCK_STREAM;
    addrinfo_hints.ai_protocol = 0;
    
    result = globus_libc_getaddrinfo(
        attr->bind_address, port, &addrinfo_hints, &save_addrinfo);
    if(result != GLOBUS_SUCCESS && 
        attr->listener_serv && attr->listener_port > 0)
    {
        /* it's possible the service name doesnt exist, since they also
         * specified a numeric port, lets try that one
         */
        snprintf(portbuf, sizeof(portbuf), "%d", attr->listener_port);
        result = globus_libc_getaddrinfo(
            attr->bind_address, portbuf, &addrinfo_hints, &save_addrinfo);
    }

    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusXIOErrorWrapFailed("globus_libc_getaddrinfo", result);
        goto error_getaddrinfo;
    }
    
    /* bind to the first one possible --
     * its not possible to bind multiple interfaces (except on some openbsd)
     * so, if the system doesnt map its inet interfaces to inet6, we
     * may have a problem
     */
    for(addrinfo = save_addrinfo; addrinfo; addrinfo = addrinfo->ai_next)
    {
        if(GlobusLibcProtocolFamilyIsIP(addrinfo->ai_family))
        {
            globus_bool_t               found = GLOBUS_FALSE;
            
            do
            {
                fd = socket(
                    addrinfo->ai_family,
                    addrinfo->ai_socktype,
                    addrinfo->ai_protocol);
                if(fd < 0)
                {
                    save_errno = errno;
                    break;
                }
                
                result = globus_l_xio_tcp_apply_handle_attrs(
                    attr, fd, GLOBUS_TRUE, GLOBUS_FALSE);
                if(result != GLOBUS_SUCCESS)
                {
                    result = GlobusXIOErrorWrapFailed(
                        "globus_l_xio_tcp_apply_handle_attrs", result);
                    GlobusIXIOTcpCloseFd(fd);
                    break;
                }
                
                result = globus_l_xio_tcp_bind(
                    fd,
                    addrinfo->ai_addr,
                    addrinfo->ai_addrlen,
                    attr->restrict_port ? attr->listener_min_port : 0,
                    attr->restrict_port ? attr->listener_max_port : 0,
                    GLOBUS_TRUE);
                if(result != GLOBUS_SUCCESS)
                {
                    result = GlobusXIOErrorWrapFailed(
                        "globus_l_xio_tcp_bind", result);
                    GlobusIXIOTcpCloseFd(fd);
                    break;
                }
                
                if(listen(
                    fd, 
                    attr->listener_backlog < 0 
                        ? SOMAXCONN : attr->listener_backlog) < 0)
                {
                    save_errno = errno;
                    GlobusIXIOTcpCloseFd(fd);
                    
                    if(save_errno == EADDRINUSE)
                    {
                        /* there's a race between bind and listen, let's try
                         * this all over again
                         */
                        continue;
                    }
                    else
                    {
                        break;
                    }
                }
                
                found = GLOBUS_TRUE;
            } while(!found);
            
            if(found)
            {
                break;
            }
        }
    }
    
    if(!addrinfo)
    {
        if(result == GLOBUS_SUCCESS)
        {
            if(save_errno == 0)
            {
                result = GlobusXIOTcpErrorNoAddrs();
            }
            else
            {
                result = GlobusXIOErrorSystemError(
                    "socket/listen", save_errno);
            }
        }
        
        goto error_no_addrinfo;
    }
    
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    
    server->listener_handle = fd;
    globus_libc_freeaddrinfo(save_addrinfo);
    
    GlobusXIOTcpDebugExit();
    return GLOBUS_SUCCESS;

error_no_addrinfo:
    globus_libc_freeaddrinfo(save_addrinfo);

error_getaddrinfo:
    GlobusXIOTcpDebugExitWithError();
    return result;
}

/*
 * server interface funcs
 */
static
globus_result_t
globus_l_xio_tcp_server_init(
    void *                              driver_attr,
    const globus_xio_contact_t *        contact_info,
    globus_xio_operation_t              op)
{
    globus_l_server_t *                 server;
    globus_l_attr_t *                   attr;
    globus_result_t                     result;
    globus_xio_contact_t                my_contact_info;
    char *                              cs;
    GlobusXIOName(globus_l_xio_tcp_server_init);
    
    GlobusXIOTcpDebugEnter();
    attr = (globus_l_attr_t *) 
        (driver_attr ? driver_attr : &globus_l_xio_tcp_attr_default);
    
    server = (globus_l_server_t *) globus_malloc(sizeof(globus_l_server_t));
    if(!server)
    {
        result = GlobusXIOErrorMemory("server");
        goto error_server;
    }
    server->converted = GLOBUS_FALSE;
    
    if(attr->handle == GLOBUS_XIO_TCP_INVALID_HANDLE)
    {
        result = globus_l_xio_tcp_create_listener(server, attr);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusXIOErrorWrapFailed(
                "globus_l_xio_tcp_create_listener", result);
            goto error_listener;
        }
    }
    else
    {
        /* user specified handle */
        server->listener_handle = attr->handle;
        server->converted = GLOBUS_TRUE;
        result = globus_l_xio_tcp_apply_handle_attrs(
            attr, server->listener_handle, GLOBUS_FALSE, GLOBUS_FALSE);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusXIOErrorWrapFailed(
                "globus_l_xio_tcp_apply_handle_attrs", result);
            goto error_listener;
        }
    }
    
    result = globus_l_xio_tcp_contact_string(
        server->listener_handle,
        GLOBUS_XIO_TCP_GET_LOCAL_CONTACT,
        &cs);
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusXIOErrorWrapFailed(
            "globus_l_xio_tcp_contact_string", result);
        goto error_pass;
    }
    
    result = globus_xio_contact_parse(&my_contact_info, cs);
    globus_free(cs);
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusXIOErrorWrapFailed(
            "globus_xio_contact_parse", result);
        goto error_pass;
    }
    
    result = globus_xio_driver_pass_server_init(op, &my_contact_info, server);
    globus_xio_contact_destroy(&my_contact_info);
    if(result != GLOBUS_SUCCESS)
    {
        goto error_pass;
    }
    
    GlobusXIOTcpDebugExit();
    return GLOBUS_SUCCESS;

error_pass:
    if(!server->converted)
    {
        int                             rc;
        do
        {
            rc = close(server->listener_handle);
        } while(rc < 0 && errno == EINTR);
    }
    
error_listener:
    globus_free(server);
    
error_server:
    GlobusXIOTcpDebugExitWithError();
    return result;
}

typedef struct
{
    globus_xio_operation_t              op;
    globus_xio_system_handle_t *        accepted_handle;
} globus_l_accept_info_t;

static
void
globus_l_xio_tcp_system_accept_cb(
    globus_result_t                     result,
    void *                              user_arg)
{
    globus_l_accept_info_t *            accept_info;
    GlobusXIOName(globus_l_xio_tcp_system_accept_cb);
    
    GlobusXIOTcpDebugEnter();
    accept_info = (globus_l_accept_info_t *) user_arg;
    
    if(result == GLOBUS_SUCCESS)
    {
        /* all handles created by me are closed on exec */
        fcntl(*accept_info->accepted_handle, F_SETFD, FD_CLOEXEC);
    }
    else
    {
        globus_free(accept_info->accepted_handle);
        accept_info->accepted_handle = GLOBUS_NULL;
    }
    
    globus_xio_driver_finished_accept(
        accept_info->op, accept_info->accepted_handle, result);
    
    globus_free(accept_info);
    
    GlobusXIOTcpDebugExit();
}

static
globus_result_t
globus_l_xio_tcp_server_accept(
    void *                              driver_server,
    globus_xio_operation_t              op)
{
    globus_l_server_t *                 server;
    globus_xio_system_handle_t *        accepted_handle;
    globus_result_t                     result;
    globus_l_accept_info_t *            accept_info;
    GlobusXIOName(globus_l_xio_tcp_server_accept);
    
    GlobusXIOTcpDebugEnter();
    server = (globus_l_server_t *) driver_server;
    
     /* create the link structure */
    accepted_handle = (globus_xio_system_handle_t *)
        globus_malloc(sizeof(globus_xio_system_handle_t));
    if(!accepted_handle)
    {
        result = GlobusXIOErrorMemory("accepted_handle");
        goto error_handle;
    }
    
    *accepted_handle = GLOBUS_XIO_TCP_INVALID_HANDLE;
    
    accept_info = (globus_l_accept_info_t *)
        globus_malloc(sizeof(globus_l_accept_info_t));
    if(!accept_info)
    {
        result = GlobusXIOErrorMemory("accept_info");
        goto error_info;
    }
    
    accept_info->op = op;
    accept_info->accepted_handle = accepted_handle;
    
    result = globus_xio_system_register_accept(
        op,
        server->listener_handle,
        accepted_handle,
        globus_l_xio_tcp_system_accept_cb,
        accept_info);
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusXIOErrorWrapFailed(
            "globus_xio_system_register_accept", result);
        goto error_register;
    }
    
    GlobusXIOTcpDebugExit();
    return GLOBUS_SUCCESS;

error_register:
    globus_free(accept_info);
    
error_info:
    globus_free(accepted_handle);
    
error_handle:
    GlobusXIOTcpDebugExitWithError();
    return result;
}

static
globus_result_t
globus_l_xio_tcp_server_cntl(
    void *                              driver_server,
    int                                 cmd,
    va_list                             ap)
{
    globus_l_server_t *                 server;
    globus_result_t                     result;
    char **                             out_string;
    globus_xio_system_handle_t *        out_handle;
    GlobusXIOName(globus_l_xio_tcp_server_cntl);
    
    GlobusXIOTcpDebugEnter();
    server = (globus_l_server_t *) driver_server;
    
    switch(cmd)
    {
      /* globus_xio_system_handle_t *   handle_out */
      case GLOBUS_XIO_TCP_GET_HANDLE:
        out_handle = va_arg(ap, globus_xio_system_handle_t *);
        *out_handle = server->listener_handle;
        break;
      
      /* char **                        contact_string_out */
      case GLOBUS_XIO_TCP_GET_LOCAL_NUMERIC_CONTACT:
      case GLOBUS_XIO_TCP_GET_LOCAL_CONTACT:
      case GLOBUS_XIO_GET_LOCAL_NUMERIC_CONTACT:
      case GLOBUS_XIO_GET_LOCAL_CONTACT:
        out_string = va_arg(ap, char **);
        result = globus_l_xio_tcp_contact_string(
            server->listener_handle, cmd, out_string);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusXIOErrorWrapFailed(
                "globus_l_xio_tcp_contact_string", result);
            goto error_contact;
        }
        break;
    
      default:
        result = GlobusXIOErrorInvalidCommand(cmd);
        goto error_invalid;
        break;
    }
    
    GlobusXIOTcpDebugExit();
    return GLOBUS_SUCCESS;

error_invalid:
error_contact:
    GlobusXIOTcpDebugExitWithError();
    return result;
}

static
globus_result_t
globus_l_xio_tcp_server_destroy(
    void *                              driver_server)
{
    globus_result_t                     result;
    globus_l_server_t *                 server;
    int                                 rc;
    GlobusXIOName(globus_l_xio_tcp_server_destroy);
    
    GlobusXIOTcpDebugEnter();
    server = (globus_l_server_t *) driver_server;
    
    if(!server->converted)
    {
        do                                                                  
        {                                                                   
            rc = close(server->listener_handle);                             
        } while(rc < 0 && errno == EINTR);       
        
        if(rc < 0)
        {
            result = GlobusXIOErrorSystemError("close", errno);
            goto error_close;
        }              
    }            
    
    globus_free(server);
    
    GlobusXIOTcpDebugExit();
    return GLOBUS_SUCCESS;
    
error_close:
    globus_free(server);
    GlobusXIOTcpDebugExitWithError();
    return result;
}

static
globus_result_t
globus_l_xio_tcp_handle_init(
    globus_l_handle_t **                handle)
{
    globus_result_t                     result;
    GlobusXIOName(globus_l_xio_tcp_handle_init);
    
    GlobusXIOTcpDebugEnter();
    *handle = (globus_l_handle_t *)
        globus_calloc(1, sizeof(globus_l_handle_t));
    if(!*handle)
    {
        result = GlobusXIOErrorMemory("handle");
        goto error_handle;
    }
    
    GlobusXIOTcpDebugExit();
    return GLOBUS_SUCCESS;

error_handle:
    GlobusXIOTcpDebugExitWithError();
    return result;    
}

static
void
globus_l_xio_tcp_handle_destroy(
    globus_l_handle_t *                 handle)
{
    GlobusXIOName(globus_l_xio_tcp_handle_destroy);
    
    GlobusXIOTcpDebugEnter();
    if(handle->connection_error)
    {
        globus_object_free(handle->connection_error);
    }
    globus_free(handle);
    GlobusXIOTcpDebugExit();
}

static
globus_result_t
globus_l_xio_tcp_bind_local(
    int                                 fd,
    globus_l_attr_t *                   attr)
{
    globus_result_t                     result;
    globus_addrinfo_t *                 save_addrinfo;
    globus_addrinfo_t *                 addrinfo;
    globus_addrinfo_t                   addrinfo_hints;
    char *                              port = "0";
    GlobusXIOName(globus_l_xio_tcp_bind_local);
    
    GlobusXIOTcpDebugEnter();
    memset(&addrinfo_hints, 0, sizeof(globus_addrinfo_t));
    addrinfo_hints.ai_flags = GLOBUS_AI_PASSIVE;
    addrinfo_hints.ai_family = attr->no_ipv6 ? PF_INET : PF_UNSPEC;
    addrinfo_hints.ai_socktype = SOCK_STREAM;
    addrinfo_hints.ai_protocol = 0;
    
    result = globus_libc_getaddrinfo(
        attr->bind_address, port, &addrinfo_hints, &save_addrinfo);
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusXIOErrorWrapFailed("globus_libc_getaddrinfo", result);
        goto error_getaddrinfo;
    }
    
    /* bind to the first one possible --
     * its not possible to bind multiple interfaces (except on some openbsd)
     * so, if the system doesnt map its inet interfaces to inet6, we
     * may have a problem
     */
    for(addrinfo = save_addrinfo; addrinfo; addrinfo = addrinfo->ai_next)
    {
        if(GlobusLibcProtocolFamilyIsIP(addrinfo->ai_family))
        {
            result = globus_l_xio_tcp_bind(
                fd,
                addrinfo->ai_addr,
                addrinfo->ai_addrlen,
                attr->restrict_port ? attr->connector_min_port : 0,
                attr->restrict_port ? attr->connector_max_port : 0,
                GLOBUS_FALSE);
            if(result != GLOBUS_SUCCESS)
            {
                result = GlobusXIOErrorWrapFailed(
                    "globus_l_xio_tcp_bind", result);
                continue;
            }
            
            break;
        }
    }
    
    if(!addrinfo)
    {
        if(result == GLOBUS_SUCCESS)
        {
            result = GlobusXIOTcpErrorNoAddrs();
        }
        
        goto error_no_addrinfo;
    }
    
    globus_libc_freeaddrinfo(save_addrinfo);
    
    GlobusXIOTcpDebugExit();
    return GLOBUS_SUCCESS;

error_no_addrinfo:
    globus_libc_freeaddrinfo(save_addrinfo);
    
error_getaddrinfo:
    GlobusXIOTcpDebugExitWithError();
    return result;
}

typedef struct
{
    globus_xio_operation_t              op;
    globus_l_handle_t *                 handle;
    globus_l_attr_t *                   attr;
    globus_addrinfo_t *                 save_addrinfo;
    globus_addrinfo_t *                 next_addrinfo;
    char *                              contact_string;
} globus_l_connect_info_t;

static
globus_result_t
globus_l_xio_tcp_connect_next(
    globus_l_connect_info_t *           connect_info);
    
static
void
globus_l_xio_tcp_system_connect_cb(
    globus_result_t                     result,
    void *                              user_arg)
{
    globus_l_connect_info_t *           connect_info;
    GlobusXIOName(globus_l_xio_tcp_system_connect_cb);
    
    GlobusXIOTcpDebugEnter();
    connect_info = (globus_l_connect_info_t *) user_arg;
    
    if(result != GLOBUS_SUCCESS)
    {
        GlobusIXIOTcpCloseFd(connect_info->handle->handle);
        if(!globus_xio_operation_is_canceled(connect_info->op))
        {
            globus_result_t                 res;
            
            res = globus_l_xio_tcp_connect_next(connect_info);
            if(res == GLOBUS_SUCCESS)
            {
                goto error_tryagain;
            }
        }
    }
    
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusXIOErrorWrapFailedWithMessage(result,
            "Unable to connect to %s", connect_info->contact_string);
        globus_l_xio_tcp_handle_destroy(connect_info->handle);
        connect_info->handle = GLOBUS_NULL;
    }
    
    globus_xio_driver_finished_open(
        connect_info->handle,
        connect_info->op,
        result);
    
    globus_libc_freeaddrinfo(connect_info->save_addrinfo);
    globus_l_xio_tcp_attr_destroy(connect_info->attr);
    globus_free(connect_info->contact_string);
    globus_free(connect_info);
    
    GlobusXIOTcpDebugExit();
    return;
    
error_tryagain:
    GlobusXIOTcpDebugExitWithError();
    return;
}

static
globus_result_t
globus_l_xio_tcp_connect_next(
    globus_l_connect_info_t *           connect_info)
{
    globus_addrinfo_t *                 addrinfo;
    int                                 fd;
    globus_result_t                     result;
    int                                 save_errno;
    globus_sockaddr_t                   myaddr;
    globus_l_attr_t *                   attr;
    GlobusXIOName(globus_l_xio_tcp_connect_next);
    
    GlobusXIOTcpDebugEnter();
    attr = connect_info->attr;
    result = GLOBUS_SUCCESS;
    save_errno = 0;
    for(addrinfo = connect_info->next_addrinfo;
        addrinfo;
        addrinfo = addrinfo->ai_next)
    {
        if(GlobusLibcProtocolFamilyIsIP(addrinfo->ai_family))
        {
            fd = socket(
                addrinfo->ai_family,
                addrinfo->ai_socktype,
                addrinfo->ai_protocol);
            if(fd < 0)
            {
                save_errno = errno;
                continue;
            }
            
            result = globus_l_xio_tcp_apply_handle_attrs(
                attr, fd, GLOBUS_TRUE, GLOBUS_FALSE);
            if(result != GLOBUS_SUCCESS)
            {
                result = GlobusXIOErrorWrapFailed(
                    "globus_l_xio_tcp_apply_handle_attrs", result);
                GlobusIXIOTcpCloseFd(fd);
                continue;
            }
            
            /* if specifying interface or outgoing port ranges, need to bind */
            if(attr->bind_address || 
                (attr->restrict_port && attr->connector_max_port > 0))
            {
                result = globus_l_xio_tcp_bind_local(fd, attr);
                if(result != GLOBUS_SUCCESS)
                {
                    result = GlobusXIOErrorWrapFailed(
                        "globus_l_xio_tcp_bind_local", result);
                    GlobusIXIOTcpCloseFd(fd);
                    continue;
                }
            }
            
            connect_info->handle->handle = fd;
            connect_info->next_addrinfo = addrinfo->ai_next;
            GlobusLibcSockaddrCopy(
                myaddr, *addrinfo->ai_addr, addrinfo->ai_addrlen);
                
            result = globus_xio_system_register_connect(
                connect_info->op,
                fd, 
                &myaddr,
                globus_l_xio_tcp_system_connect_cb,
                connect_info);
            if(result != GLOBUS_SUCCESS)
            {
                result = GlobusXIOErrorWrapFailed(
                    "globus_xio_system_register_connect", result);
                GlobusIXIOTcpCloseFd(fd);
                continue;
            }

            break;
        }
    }
    
    /* addrinfo maybe a dangling pointer here, but I am only checking for
     * null, so we're safe
     */
    if(!addrinfo)
    {
        if(result == GLOBUS_SUCCESS)
        {
            if(save_errno == 0)
            {
                result = GlobusXIOTcpErrorNoAddrs();
            }
            else
            {
                result = GlobusXIOErrorSystemError("socket", save_errno);
            }
        }
        
        goto error_no_addrinfo;
    }
    
    GlobusXIOTcpDebugExit();
    return GLOBUS_SUCCESS;
    
error_no_addrinfo:
    GlobusXIOTcpDebugExitWithError();
    return result;
}

static
globus_result_t
globus_l_xio_tcp_connect(
    globus_xio_operation_t              op,
    globus_l_handle_t *                 handle,
    const globus_l_attr_t *             attr,
    const char *                        host,
    const char *                        port)
{
    globus_result_t                     result;
    globus_addrinfo_t *                 addrinfo;
    globus_addrinfo_t                   addrinfo_hints;
    globus_l_connect_info_t *           connect_info;
    GlobusXIOName(globus_l_xio_tcp_connect);
    
    GlobusXIOTcpDebugEnter();
    /* setup hints for types of connectable sockets we want */
    memset(&addrinfo_hints, 0, sizeof(globus_addrinfo_t));
    addrinfo_hints.ai_flags = 0;
    addrinfo_hints.ai_family = attr->no_ipv6 ? PF_INET : PF_UNSPEC;
    addrinfo_hints.ai_socktype = SOCK_STREAM;
    addrinfo_hints.ai_protocol = 0;
    
    result = globus_libc_getaddrinfo(host, port, &addrinfo_hints, &addrinfo);
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusXIOErrorWrapFailed(
            "globus_libc_getaddrinfo", result);
        goto error_getaddrinfo;
    }
    
    connect_info = (globus_l_connect_info_t *)
        globus_malloc(sizeof(globus_l_connect_info_t));
    if(!connect_info)
    {
        result = GlobusXIOErrorMemory("connect_info");
        goto error_info;
    }
    
    connect_info->contact_string =
        globus_common_create_string("%s:%s", host, port);
    if(!connect_info->contact_string)
    {
        result = GlobusXIOErrorMemory("connect_info");
        goto error_contact;
    }
    
    result = globus_l_xio_tcp_attr_copy(
        (void **) &connect_info->attr, (void *) attr);
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusXIOErrorWrapFailed(
            "globus_l_xio_tcp_attr_copy", result);
        goto error_attr;
    }
    
    connect_info->op = op;
    connect_info->handle = handle;
    connect_info->save_addrinfo = addrinfo;
    connect_info->next_addrinfo = addrinfo;
    
    result = globus_l_xio_tcp_connect_next(connect_info);
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusXIOErrorWrapFailed(
            "globus_l_xio_tcp_connect_next", result);
        goto error_connect_next;
    }
    
    GlobusXIOTcpDebugExit();
    return GLOBUS_SUCCESS;

error_connect_next:
    globus_l_xio_tcp_attr_destroy(connect_info->attr);
    
error_attr:
    globus_free(connect_info->contact_string);
    
error_contact:
    globus_free(connect_info);

error_info:
    globus_libc_freeaddrinfo(addrinfo);

error_getaddrinfo:
    GlobusXIOTcpDebugExitWithError();
    return result;
}

/*
 *  open a tcp
 */
static
globus_result_t
globus_l_xio_tcp_open(
    const globus_xio_contact_t *        contact_info,
    void *                              driver_link,
    void *                              driver_attr,
    globus_xio_operation_t              op)
{
    globus_l_handle_t *                 handle;
    const globus_l_attr_t *             attr;
    globus_result_t                     result;
    GlobusXIOName(globus_l_xio_tcp_open);
    
    GlobusXIOTcpDebugEnter();
    attr = (globus_l_attr_t *) 
        (driver_attr ? driver_attr : &globus_l_xio_tcp_attr_default);
    
    result = globus_l_xio_tcp_handle_init(&handle);
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusXIOErrorWrapFailed(
            "globus_l_xio_tcp_handle_init", result);
        goto error_handle;
    }
    
    if(!driver_link && attr->handle == GLOBUS_XIO_TCP_INVALID_HANDLE)
    {
        if(!(contact_info->host && contact_info->port))
        {
            result = GlobusXIOErrorContactString("missing host or port");
            goto error_contact_string;
        }
        
        result = globus_l_xio_tcp_connect(
            op, handle, attr, contact_info->host, contact_info->port);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusXIOErrorWrapFailedWithMessage2(result,
                "Unable to connect to %s:%s",
                contact_info->host, contact_info->port);
            goto error_connect;
        }
    }
    else
    {
        if(driver_link)
        {
            globus_xio_system_handle_t * accepted_handle;
            
            accepted_handle = (globus_xio_system_handle_t *) driver_link;
            handle->handle = *accepted_handle;
            
            /* prevent link destroy from closing this */
            *accepted_handle = GLOBUS_XIO_TCP_INVALID_HANDLE;
            handle->converted = GLOBUS_FALSE;
        }
        else
        {
            handle->handle = attr->handle;
            handle->converted = GLOBUS_TRUE;
        }
        
        result = globus_l_xio_tcp_apply_handle_attrs(
            attr, handle->handle, GLOBUS_FALSE, GLOBUS_TRUE);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusXIOErrorWrapFailed(
                "globus_l_xio_tcp_apply_handle_attrs", result);
            goto error_attrs;
        }
        
        globus_xio_driver_finished_open(handle, op, GLOBUS_SUCCESS);
    }
    
    GlobusXIOTcpDebugExit();
    return GLOBUS_SUCCESS;

error_attrs:   
error_connect:
error_contact_string:
    globus_l_xio_tcp_handle_destroy(handle);  

error_handle:
    GlobusXIOTcpDebugExitWithError();
    return result;
}

static
void
globus_l_xio_tcp_system_close_cb(
    globus_result_t                     result,
    void *                              user_arg)
{
    globus_xio_operation_t              op;
    globus_l_handle_t *                 handle;
    GlobusXIOName(globus_l_xio_tcp_system_close_cb);
    
    GlobusXIOTcpDebugEnter();
    op = (globus_xio_operation_t) user_arg;
    
    handle = (globus_l_handle_t *)
        globus_xio_operation_get_driver_specific(op);
    
    globus_xio_driver_finished_close(op, result);
    globus_l_xio_tcp_handle_destroy(handle);
    
    GlobusXIOTcpDebugExit();
}

/*
 *  close a tcp
 */
static
globus_result_t
globus_l_xio_tcp_close(
    void *                              driver_specific_handle,
    void *                              attr,
    globus_xio_operation_t              op)
{
    globus_l_handle_t *                 handle;
    globus_result_t                     result;
    GlobusXIOName(globus_l_xio_tcp_close);
    
    GlobusXIOTcpDebugEnter();
    handle = (globus_l_handle_t *) driver_specific_handle;
        
    if(handle->converted)
    {
        globus_xio_driver_finished_close(op, GLOBUS_SUCCESS);
        globus_l_xio_tcp_handle_destroy(handle);
    }
    else
    {
        result = globus_xio_system_register_close(
            op,
            handle->handle,
            globus_l_xio_tcp_system_close_cb,
            op);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusXIOErrorWrapFailed(
                "globus_xio_system_register_close", result);
            goto error_register;
        }
    }
    
    GlobusXIOTcpDebugExit();
    return GLOBUS_SUCCESS;
    
error_register:
    globus_l_xio_tcp_handle_destroy(handle);
    
    GlobusXIOTcpDebugExitWithError();
    return result;
}

static
void
globus_l_xio_tcp_finish_read(
    globus_l_handle_t *                 handle,
    globus_result_t                     result,
    globus_size_t                       nbytes)
{
    globus_xio_operation_t              op;
    
    /**
     * all of this should have a mutex to protect it, but that would only
     * protect against users misusing this driver
     */
    op = handle->read_op;
    handle->read_op = NULL;
    
    if(result != GLOBUS_SUCCESS &&
        !globus_xio_error_is_canceled(result) &&
        !globus_xio_error_is_eof(result) &&
        !handle->connection_error)
    {
        handle->connection_error =
            globus_object_copy(globus_error_peek(result));
    }

    globus_xio_driver_finished_read(op, result, nbytes);
}

static
void
globus_l_xio_tcp_system_read_cb(
    globus_result_t                     result,
    globus_size_t                       nbytes,
    void *                              user_arg)
{
    globus_l_handle_t *                 handle;
    GlobusXIOName(globus_l_xio_tcp_system_read_cb);
    
    GlobusXIOTcpDebugEnter();
    handle = (globus_l_handle_t *) user_arg;
    globus_l_xio_tcp_finish_read(handle, result, nbytes);
    GlobusXIOTcpDebugExit();
}

/*
 *  read from a tcp
 */
static
globus_result_t
globus_l_xio_tcp_read(
    void *                              driver_specific_handle,
    const globus_xio_iovec_t *          iovec,
    int                                 iovec_count,
    globus_xio_operation_t              op)
{
    globus_l_handle_t *                 handle;
    globus_result_t                     result;
    GlobusXIOName(globus_l_xio_tcp_read);
    
    GlobusXIOTcpDebugEnter();
    handle = (globus_l_handle_t *) driver_specific_handle;
    if(handle->read_op)
    {
        result = GlobusXIOErrorAlreadyRegistered();
        goto error_already;
    }
    if(handle->connection_error)
    {
        result =
            globus_error_put(globus_object_copy(handle->connection_error));
        goto error_already;
    }
    
    handle->read_op = op;
    
    /* if buflen and waitfor are both 0, we behave like register select */
    if(globus_xio_operation_get_wait_for(op) == 0 &&
        (iovec_count > 1 || iovec[0].iov_len > 0))
    {
        globus_size_t                   nbytes;
        
        result = globus_xio_system_try_read(
            handle->handle, iovec, iovec_count, &nbytes);
        globus_l_xio_tcp_finish_read(handle, result, nbytes);
        /* dont want to return error here mainly because error could be eof, 
         * which is against our convention to return an eof error on async
         * calls.  Other than that, the choice is arbitrary
         */
    }
    else
    {
        result = globus_xio_system_register_read(
            op,
            handle->handle,
            iovec,
            iovec_count,
            globus_xio_operation_get_wait_for(op),
            globus_l_xio_tcp_system_read_cb,
            handle);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusXIOErrorWrapFailed(
                "globus_xio_system_register_read", result);
            goto error_register;
        }
    }
    
    GlobusXIOTcpDebugExit();
    return GLOBUS_SUCCESS;

error_register:
    handle->read_op = NULL;
error_already:
    GlobusXIOTcpDebugExitWithError();
    return result;
}

static
void
globus_l_xio_tcp_finish_write(
    globus_l_handle_t *                 handle,
    globus_result_t                     result,
    globus_size_t                       nbytes)
{
    globus_xio_operation_t              op;
    
    /**
     * all of this should have a mutex to protect it, but that would only
     * protect against users misusing this driver
     */
    op = handle->write_op;
    handle->write_op = NULL;
    
    if(result != GLOBUS_SUCCESS &&
        !globus_xio_error_is_canceled(result) &&
        !handle->connection_error)
    {
        handle->connection_error =
            globus_object_copy(globus_error_peek(result));
    }

    globus_xio_driver_finished_write(op, result, nbytes);
}

static
void
globus_l_xio_tcp_system_write_cb(
    globus_result_t                     result,
    globus_size_t                       nbytes,
    void *                              user_arg)
{
    globus_l_handle_t *                 handle;
    GlobusXIOName(globus_l_xio_tcp_system_write_cb);
    
    GlobusXIOTcpDebugEnter();
    handle = (globus_l_handle_t *) user_arg;
    globus_l_xio_tcp_finish_write(handle, result, nbytes);
    GlobusXIOTcpDebugExit();
}

/*
 *  write to a tcp
 */
static
globus_result_t
globus_l_xio_tcp_write(
    void *                              driver_specific_handle,
    const globus_xio_iovec_t *          iovec,
    int                                 iovec_count,
    globus_xio_operation_t              op)
{
    globus_l_handle_t *                 handle;
    globus_l_attr_t *                   attr;
    globus_result_t                     result;
    GlobusXIOName(globus_l_xio_tcp_write);
    
    GlobusXIOTcpDebugEnter();
    
    GlobusXIOTcpDebugPrintf(
        GLOBUS_L_XIO_TCP_DEBUG_INFO,
        ("[%s] count=%d, 1st buflen=%d\n",
            _xio_name, iovec_count, (int) iovec[0].iov_len));
    
    /**
     * all of this should have a mutex to protect it, but that would only
     * protect against users misusing this driver
     */
    handle = (globus_l_handle_t *) driver_specific_handle;
    if(handle->write_op)
    {
        result = GlobusXIOErrorAlreadyRegistered();
        goto error_already;
    }
    if(handle->connection_error)
    {
        result =
            globus_error_put(globus_object_copy(handle->connection_error));
        goto error_already;
    }
    
    handle->write_op = op;
    
    attr = (globus_l_attr_t *)
        globus_xio_operation_get_data_descriptor(op, GLOBUS_FALSE);
    
    /* if buflen and waitfor are both 0, we behave like register select */
    if(globus_xio_operation_get_wait_for(op) == 0 &&
        (iovec_count > 1 || iovec[0].iov_len > 0))
    {
        globus_size_t                   nbytes;
        
        if(attr && attr->send_flags)
        {
            result = globus_xio_system_try_write_ex(
                handle->handle,
                iovec,
                iovec_count,
                attr->send_flags,
                GLOBUS_NULL,
                &nbytes);
        }
        else
        {
            result = globus_xio_system_try_write(
                handle->handle, iovec, iovec_count, &nbytes);
        }
        globus_l_xio_tcp_finish_write(handle, result, nbytes);
    }
    else
    {
        if(attr && attr->send_flags)
        {
            result = globus_xio_system_register_write_ex(
                op,
                handle->handle,
                iovec,
                iovec_count,
                globus_xio_operation_get_wait_for(op),
                attr->send_flags,
                GLOBUS_NULL,
                globus_l_xio_tcp_system_write_cb,
                handle);
        }
        else
        {
            result = globus_xio_system_register_write(
                op,
                handle->handle,
                iovec,
                iovec_count,
                globus_xio_operation_get_wait_for(op),
                globus_l_xio_tcp_system_write_cb,
                handle);
        }
        
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusXIOErrorWrapFailed(
                "globus_xio_system_register_write", result);
            goto error_register;
        }
    }
    
    GlobusXIOTcpDebugExit();
    return GLOBUS_SUCCESS;

error_register:
    handle->write_op = NULL;
error_already:
    GlobusXIOTcpDebugExitWithError();
    return result;
}

static
globus_result_t
globus_l_xio_tcp_cntl(
    void *                              driver_specific_handle,
    int                                 cmd,
    va_list                             ap)
{
    globus_l_handle_t *                 handle;
    globus_result_t                     result;
    int *                               out_int;
    globus_bool_t                       in_bool;
    globus_bool_t *                     out_bool;
    int                                 in_int;
    int                                 fd;
    globus_socklen_t                    len;
    char **                             out_string;
    globus_xio_system_handle_t *        out_handle;
    GlobusXIOName(globus_l_xio_tcp_cntl);
    
    GlobusXIOTcpDebugEnter();
    handle = (globus_l_handle_t *) driver_specific_handle;
    fd = handle->handle;
    switch(cmd)
    {
      /* globus_xio_system_handle_t *   handle_out */
      case GLOBUS_XIO_TCP_GET_HANDLE:
        out_handle = va_arg(ap, globus_xio_system_handle_t *);
        *out_handle = fd;
        break;
        
      /* globus_bool_t                  keepalive */
      case GLOBUS_XIO_TCP_SET_KEEPALIVE:
        in_bool = va_arg(ap, globus_bool_t);
        if(setsockopt(
            fd, SOL_SOCKET, SO_KEEPALIVE, &in_bool, sizeof(in_bool)) < 0)
        {
            result = GlobusXIOErrorSystemError("setsockopt", errno);
            goto error_sockopt;
        }
        break;
        
      /* globus_bool_t *                keepalive_out */
      case GLOBUS_XIO_TCP_GET_KEEPALIVE:
        out_bool = va_arg(ap, globus_bool_t *);
        len = sizeof(globus_bool_t);
        if(getsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, out_bool, &len) < 0)
        {
            result = GlobusXIOErrorSystemError("getsockopt", errno);
            goto error_sockopt;
        }
        break;
        
      /* globus_bool_t                  linger */
      /* int                            linger_time */
      case GLOBUS_XIO_TCP_SET_LINGER:
        {
            struct linger           linger;
        
            linger.l_onoff = va_arg(ap, globus_bool_t);;
            linger.l_linger = va_arg(ap, int);
            
            if(setsockopt(
                fd, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger)) < 0)
            {
                result = GlobusXIOErrorSystemError("setsockopt", errno);
                goto error_sockopt;
            }
        }
        break;
        
      /* globus_bool_t *                linger_out */
      /* int *                          linger_time_out */
      case GLOBUS_XIO_TCP_GET_LINGER:
        {
            struct linger           linger;
            
            len = sizeof(linger);
            if(getsockopt(fd, SOL_SOCKET, SO_LINGER, &linger, &len) < 0)
            {
                result = GlobusXIOErrorSystemError("getsockopt", errno);
                goto error_sockopt;
            }
            
            out_bool = va_arg(ap, globus_bool_t *);
            out_int = va_arg(ap, int *);
            *out_bool = linger.l_onoff;
            *out_int = linger.l_linger;
        }
        break;
        
      /* globus_bool_t                  oobinline */
      case GLOBUS_XIO_TCP_SET_OOBINLINE:
        in_bool = va_arg(ap, globus_bool_t);
        if(setsockopt(
            fd, SOL_SOCKET, SO_OOBINLINE, &in_bool, sizeof(in_bool)) < 0)
        {
            result = GlobusXIOErrorSystemError("setsockopt", errno);
            goto error_sockopt;
        }
        break;
        
      /* globus_bool_t *                oobinline_out */
      case GLOBUS_XIO_TCP_GET_OOBINLINE:
        out_bool = va_arg(ap, globus_bool_t *);
        len = sizeof(globus_bool_t);
        if(getsockopt(
            fd, SOL_SOCKET, SO_OOBINLINE, out_bool, &len) < 0)
        {
            result = GlobusXIOErrorSystemError("getsockopt", errno);
            goto error_sockopt;
        }
        break;
        
      /* int                            sndbuf */
      case GLOBUS_XIO_TCP_SET_SNDBUF:
        in_int = va_arg(ap, int);
        if(setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &in_int, sizeof(in_int)) < 0)
        {
            result = GlobusXIOErrorSystemError("setsockopt", errno);
            goto error_sockopt;
        }
        break;
        
      /* int *                          sndbuf_out */
      case GLOBUS_XIO_TCP_GET_SNDBUF:
        out_int = va_arg(ap, int *);
        len = sizeof(int);
        if(getsockopt(fd, SOL_SOCKET, SO_SNDBUF, out_int, &len) < 0)
        {
            result = GlobusXIOErrorSystemError("getsockopt", errno);
            goto error_sockopt;
        }
        break;
        
      /* int                            rcvbuf */
      case GLOBUS_XIO_TCP_SET_RCVBUF:
        in_int = va_arg(ap, int);
        if(setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &in_int, sizeof(in_int)) < 0)
        {
            result = GlobusXIOErrorSystemError("setsockopt", errno);
            goto error_sockopt;
        }
        break;
        
      /* int *                          rcvbuf_out */
      case GLOBUS_XIO_TCP_GET_RCVBUF:
        out_int = va_arg(ap, int *);
        len = sizeof(int);
        if(getsockopt(fd, SOL_SOCKET, SO_RCVBUF, out_int, &len) < 0)
        {
            result = GlobusXIOErrorSystemError("getsockopt", errno);
            goto error_sockopt;
        }
        break;
        
      /* globus_bool_t                  nodelay */
      case GLOBUS_XIO_TCP_SET_NODELAY:
        in_bool = va_arg(ap, globus_bool_t);
        if(setsockopt(
            fd, IPPROTO_TCP, TCP_NODELAY, &in_bool, sizeof(in_bool)) < 0)
        {
            result = GlobusXIOErrorSystemError("setsockopt", errno);
            goto error_sockopt;
        }
        break;
        
      /* globus_bool_t *                nodelay_out */
      case GLOBUS_XIO_TCP_GET_NODELAY:
        out_bool = va_arg(ap, globus_bool_t *);
        len = sizeof(globus_bool_t);
        if(getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, out_bool, &len) < 0)
        {
            result = GlobusXIOErrorSystemError("getsockopt", errno);
            goto error_sockopt;
        }
        break;
      
      /* char **                        contact_string_out */
      case GLOBUS_XIO_TCP_GET_LOCAL_NUMERIC_CONTACT:
      case GLOBUS_XIO_TCP_GET_LOCAL_CONTACT:
      case GLOBUS_XIO_TCP_GET_REMOTE_NUMERIC_CONTACT:
      case GLOBUS_XIO_TCP_GET_REMOTE_CONTACT:
      case GLOBUS_XIO_GET_LOCAL_NUMERIC_CONTACT:
      case GLOBUS_XIO_GET_LOCAL_CONTACT:
      case GLOBUS_XIO_GET_REMOTE_NUMERIC_CONTACT:
      case GLOBUS_XIO_GET_REMOTE_CONTACT:
        out_string = va_arg(ap, char **);
        result = globus_l_xio_tcp_contact_string(fd, cmd, out_string);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusXIOErrorWrapFailed(
                "globus_l_xio_tcp_contact_string", result);
            goto error_contact;
        }
        break;
        
      default:
        result = GlobusXIOErrorInvalidCommand(cmd);
        goto error_invalid;
        break;
    }
    
    GlobusXIOTcpDebugExit();
    return GLOBUS_SUCCESS;

error_invalid:
error_contact:
error_sockopt:
    GlobusXIOTcpDebugExitWithError();
    return result;
}

static
globus_result_t
globus_l_xio_tcp_init(
    globus_xio_driver_t *               out_driver)
{
    globus_xio_driver_t                 driver;
    globus_result_t                     result;
    GlobusXIOName(globus_l_xio_tcp_init);
    
    GlobusXIOTcpDebugEnter();
    /* I dont support any driver options, so I'll ignore the ap */
    
    result = globus_xio_driver_init(&driver, "tcp", GLOBUS_NULL);
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusXIOErrorWrapFailed(
            "globus_l_xio_tcp_handle_init", result);
        goto error_init;
    }

    globus_xio_driver_set_transport(
        driver,
        globus_l_xio_tcp_open,
        globus_l_xio_tcp_close,
        globus_l_xio_tcp_read,
        globus_l_xio_tcp_write,
        globus_l_xio_tcp_cntl);

    globus_xio_driver_set_server(
        driver,
        globus_l_xio_tcp_server_init,
        globus_l_xio_tcp_server_accept,
        globus_l_xio_tcp_server_destroy,
        globus_l_xio_tcp_server_cntl,
        globus_l_xio_tcp_link_cntl,
        globus_l_xio_tcp_link_destroy);
        
    globus_xio_driver_set_attr(
        driver,
        globus_l_xio_tcp_attr_init,
        globus_l_xio_tcp_attr_copy,
        globus_l_xio_tcp_attr_cntl,
        globus_l_xio_tcp_attr_destroy);
    
    *out_driver = driver;
    
    GlobusXIOTcpDebugExit();
    return GLOBUS_SUCCESS;

error_init:
    GlobusXIOTcpDebugExitWithError();
    return result;
}

static
void
globus_l_xio_tcp_destroy(
    globus_xio_driver_t                 driver)
{
    GlobusXIOName(globus_l_xio_tcp_destroy);
    
    GlobusXIOTcpDebugEnter();
    globus_xio_driver_destroy(driver);
    GlobusXIOTcpDebugExit();
}

GlobusXIODefineDriver(
    tcp,
    globus_l_xio_tcp_init,
    globus_l_xio_tcp_destroy);

static
int
globus_l_xio_tcp_activate(void)
{
    int                                 min;
    int                                 max;
    int                                 rc;
    GlobusXIOName(globus_l_xio_tcp_activate);
    
    GlobusDebugInit(GLOBUS_XIO_TCP, TRACE);
    
    GlobusXIOTcpDebugEnter();
    
    globus_l_xio_tcp_port_range_state_file = -1;
    globus_mutex_init(&globus_l_xio_tcp_port_range_state_lock, NULL);
    
    if(globus_l_xio_tcp_get_env_pair(
        "GLOBUS_TCP_PORT_RANGE", &min, &max) && min <= max)
    {
        char *                          tmp;
        
        globus_l_xio_tcp_attr_default.listener_min_port = min;
        globus_l_xio_tcp_attr_default.listener_max_port = max;
        
        if((tmp = globus_module_getenv("GLOBUS_TCP_PORT_RANGE_STATE_FILE")) &&
            *tmp)
        {
            globus_l_xio_tcp_file_open(tmp);
        }
    }
    
    if(globus_l_xio_tcp_get_env_pair(
        "GLOBUS_TCP_SOURCE_RANGE", &min, &max) && min <= max)
    {
        globus_l_xio_tcp_attr_default.connector_min_port = min;
        globus_l_xio_tcp_attr_default.connector_max_port = max;
    }
    
    rc = globus_module_activate(GLOBUS_XIO_SYSTEM_MODULE);
    if(rc != GLOBUS_SUCCESS)
    {
        goto error_activate;
    }
    
    GlobusXIORegisterDriver(tcp);
    
    GlobusXIOTcpDebugExit();
    return GLOBUS_SUCCESS;
    
error_activate:
    globus_l_xio_tcp_file_close();
    globus_mutex_destroy(&globus_l_xio_tcp_port_range_state_lock);
    GlobusXIOTcpDebugExitWithError();
    GlobusDebugDestroy(GLOBUS_XIO_TCP);
    return rc;
}

static
int
globus_l_xio_tcp_deactivate(void)
{
    GlobusXIOName(globus_l_xio_tcp_deactivate);
    
    GlobusXIOTcpDebugEnter();
    
    globus_l_xio_tcp_attr_default.listener_min_port = 0;
    globus_l_xio_tcp_attr_default.listener_max_port = 0;
    globus_l_xio_tcp_attr_default.connector_min_port = 0;
    globus_l_xio_tcp_attr_default.connector_max_port = 0;
    
    globus_l_xio_tcp_file_close();
    globus_mutex_destroy(&globus_l_xio_tcp_port_range_state_lock);
    
    GlobusXIOUnRegisterDriver(tcp);
    globus_module_deactivate(GLOBUS_XIO_SYSTEM_MODULE);
    
    GlobusXIOTcpDebugExit();
    GlobusDebugDestroy(GLOBUS_XIO_TCP);
    
    return GLOBUS_SUCCESS;
}
