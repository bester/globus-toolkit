#include "globus_i_xio.h"
#include "globus_xio_driver.h"
#include "globus_xio_file_driver.h"
#include "version.h"

GlobusDebugDefine(GLOBUS_XIO_FILE);

#define GlobusXIOFileDebugPrintf(level, message)                            \
    GlobusDebugPrintf(GLOBUS_XIO_FILE, level, message)

#define GlobusXIOFileDebugEnter()                                           \
    GlobusXIOFileDebugPrintf(                                               \
        GLOBUS_L_XIO_FILE_DEBUG_TRACE,                                      \
        ("[%s] Entering\n", _xio_name))
        
#define GlobusXIOFileDebugExit()                                            \
    GlobusXIOFileDebugPrintf(                                               \
        GLOBUS_L_XIO_FILE_DEBUG_TRACE,                                      \
        ("[%s] Exiting\n", _xio_name))

#define GlobusXIOFileDebugExitWithError()                                   \
    GlobusXIOFileDebugPrintf(                                               \
        GLOBUS_L_XIO_FILE_DEBUG_TRACE,                                      \
        ("[%s] Exiting with error\n", _xio_name))

enum globus_l_xio_error_levels
{
    GLOBUS_L_XIO_FILE_DEBUG_TRACE       = 1
};

static
int
globus_l_xio_file_activate(void);

static
int
globus_l_xio_file_deactivate(void);

static globus_module_descriptor_t       globus_i_xio_file_module =
{
    "globus_xio_file",
    globus_l_xio_file_activate,
    globus_l_xio_file_deactivate,
    GLOBUS_NULL,
    GLOBUS_NULL,
    &local_version
};

/*
 *  attribute structure
 */
typedef struct
{
    int                                 mode;
    int                                 flags;
    globus_xio_system_handle_t          handle;
} globus_l_attr_t;

/* default attr */
static const globus_l_attr_t            globus_l_xio_file_attr_default =
{
    GLOBUS_XIO_FILE_IRUSR       |       /* mode     */
        GLOBUS_XIO_FILE_IWUSR,  
    GLOBUS_XIO_FILE_CREAT       |       /* flags    */
        GLOBUS_XIO_FILE_RDWR    | 
        GLOBUS_XIO_FILE_BINARY,   
    GLOBUS_XIO_FILE_INVALID_HANDLE      /* handle   */             
};

/*
 *  target structure
 */
typedef struct
{
    char *                              pathname;
    globus_xio_system_handle_t          handle;
} globus_l_target_t;

/*
 *  handle structure
 */
typedef struct
{
    globus_xio_system_handle_t          handle;
} globus_l_handle_t;

static
int
globus_l_xio_file_activate(void)
{
    int                                 rc;
    
    GlobusXIOName(globus_l_xio_file_activate);
    
    GlobusDebugInit(GLOBUS_XIO_FILE, TRACE);
    
    GlobusXIOFileDebugEnter();
    
    rc = globus_module_activate(GLOBUS_XIO_SYSTEM_MODULE);
    if(rc != GLOBUS_SUCCESS)
    {
        goto error_activate;
    }
    
    GlobusXIOFileDebugExit();
    return GLOBUS_SUCCESS;

error_activate:
    GlobusXIOFileDebugExitWithError();
    return rc;
}

static
int
globus_l_xio_file_deactivate(void)
{
    GlobusXIOName(globus_l_xio_file_deactivate);
    
    GlobusXIOFileDebugEnter();
    
    globus_module_deactivate(GLOBUS_XIO_SYSTEM_MODULE);
    
    GlobusXIOFileDebugExit();
    GlobusDebugDestroy(GLOBUS_XIO_FILE);
    
    return GLOBUS_SUCCESS;
}

/*
 *  initialize a driver attribute
 */
static
globus_result_t
globus_l_xio_file_attr_init(
    void **                             out_attr)
{
    globus_l_attr_t *                   attr;
    globus_result_t                     result;
    GlobusXIOName(globus_l_xio_file_attr_init);
    
    GlobusXIOFileDebugEnter();
    /*
     *  create a file attr structure and intialize its values
     */
    attr = (globus_l_attr_t *) globus_malloc(sizeof(globus_l_attr_t));
    if(!attr)
    {
        result = GlobusXIOErrorMemory("attr");
        goto error_attr;
    }
    
    memcpy(attr, &globus_l_xio_file_attr_default, sizeof(globus_l_attr_t));
    *out_attr = attr;
    
    GlobusXIOFileDebugExit();
    return GLOBUS_SUCCESS;

error_attr:
    GlobusXIOFileDebugExitWithError();
    return result;
}

/*
 *  modify the attribute structure
 */
static
globus_result_t
globus_l_xio_file_attr_cntl(
    void *                              driver_attr,
    int                                 cmd,
    va_list                             ap)
{
    globus_l_attr_t *                   attr;
    int *                               out_int;
    globus_xio_system_handle_t *        out_handle;
    GlobusXIOName(globus_l_xio_file_attr_cntl);
    
    GlobusXIOFileDebugEnter();
    
    attr = (globus_l_attr_t *) driver_attr;
    switch(cmd)
    {
      /* int                            mode */
      case GLOBUS_XIO_FILE_SET_MODE:
        attr->mode = va_arg(ap, int);
        break;
        
      /* int *                          mode_out */
      case GLOBUS_XIO_FILE_GET_MODE:
        out_int = va_arg(ap, int *);
        *out_int = attr->mode;
        break;

      /* int                            mode */
      case GLOBUS_XIO_FILE_SET_FLAGS:
        attr->flags = va_arg(ap, int);
        break;

      /* int *                          mode_out */
      case GLOBUS_XIO_FILE_GET_FLAGS:
        out_int = va_arg(ap, int *);
        *out_int = attr->flags;
        break;
    
      /* globus_xio_system_handle_t     handle */
      case GLOBUS_XIO_FILE_SET_HANDLE:
        attr->handle = va_arg(ap, globus_xio_system_handle_t);
        break;
        
      /* globus_xio_system_handle_t *   handle */
      case GLOBUS_XIO_FILE_GET_HANDLE:
        out_handle = va_arg(ap, globus_xio_system_handle_t *);
        *out_handle = attr->handle;
        break;

      default:
        GlobusXIOFileDebugExitWithError();
        return GlobusXIOErrorInvalidCommand(cmd);
        break;
    }
    
    GlobusXIOFileDebugExit();
    return GLOBUS_SUCCESS;
}

/*
 *  copy an attribute structure
 */
static
globus_result_t
globus_l_xio_file_attr_copy(
    void **                             dst,
    void *                              src)
{
    globus_l_attr_t *                   attr;
    globus_result_t                     result;
    GlobusXIOName(globus_l_xio_file_attr_copy);
    
    GlobusXIOFileDebugEnter();
    
    attr = (globus_l_attr_t *) globus_malloc(sizeof(globus_l_attr_t));
    if(!attr)
    {
        result = GlobusXIOErrorMemory("attr");
        goto error_attr;
    }
    
    memcpy(attr, src, sizeof(globus_l_attr_t));
    *dst = attr;
    
    GlobusXIOFileDebugExit();
    return GLOBUS_SUCCESS;

error_attr:
    GlobusXIOFileDebugExitWithError();
    return result;
}

/*
 *  destroy an attr structure
 */
static
globus_result_t
globus_l_xio_file_attr_destroy(
    void *                              driver_attr)
{
    GlobusXIOName(globus_l_xio_file_attr_destroy);
    
    GlobusXIOFileDebugEnter();
    
    globus_free(driver_attr);
    
    GlobusXIOFileDebugExit();
    return GLOBUS_SUCCESS;
}

/*
 *  initialize target structure
 */
static
globus_result_t
globus_l_xio_file_target_init(
    void **                             out_target,
    void *                              driver_attr,
    const char *                        contact_string)
{
    globus_l_target_t *                 target;
    globus_l_attr_t *                   attr;
    globus_result_t                     result;
    GlobusXIOName(globus_l_xio_file_target_init);
    
    GlobusXIOFileDebugEnter();
    
    attr = (globus_l_attr_t *) driver_attr;
    
    /* create the target structure and copy the contact string into it */
    target = (globus_l_target_t *) globus_malloc(sizeof(globus_l_target_t));
    if(!target)
    {
        result = GlobusXIOErrorMemory("target");
        goto error_target;
    }
    
    target->pathname = GLOBUS_NULL;
    target->handle = GLOBUS_XIO_FILE_INVALID_HANDLE;
    
    if(!attr || attr->handle == GLOBUS_XIO_FILE_INVALID_HANDLE)
    {
        target->pathname = globus_libc_strdup(contact_string);
        if(!target->pathname)
        {
            result = GlobusXIOErrorMemory("pathname");
            goto error_pathname;
        }
    }
    else
    {
        target->handle = attr->handle;
    }
    
    *out_target = target;

    GlobusXIOFileDebugExit();
    return GLOBUS_SUCCESS;

error_pathname:
    globus_free(target);
    
error_target:
    GlobusXIOFileDebugExitWithError();
    return result;
}

/*
 *  destroy the target structure
 */
static
globus_result_t
globus_l_xio_file_target_destroy(
    void *                              driver_target)
{
    globus_l_target_t *                 target;
    GlobusXIOName(globus_l_xio_file_target_destroy);
    
    GlobusXIOFileDebugEnter();
    
    target = (globus_l_target_t *) driver_target;
    
    if(target->pathname)
    {
        globus_free(target->pathname);
    }
    globus_free(target);
    
    GlobusXIOFileDebugExit();
    return GLOBUS_SUCCESS;
}

static
globus_result_t
globus_l_xio_file_handle_init(
    globus_l_handle_t **                handle)
{
    globus_result_t                     result;
    GlobusXIOName(globus_l_xio_file_handle_init);
    
    GlobusXIOFileDebugEnter();
    
    *handle = (globus_l_handle_t *) globus_malloc(sizeof(globus_l_handle_t));
    if(!*handle)
    {
        result = GlobusXIOErrorMemory("handle");
        goto error_handle;
    }
    
    GlobusXIOFileDebugExit();
    return GLOBUS_SUCCESS;

error_handle:
    GlobusXIOFileDebugExitWithError();
    return result;    
}

static
void
globus_l_xio_file_handle_destroy(
    globus_l_handle_t *                 handle)
{
    GlobusXIOName(globus_l_xio_file_handle_destroy);
    
    GlobusXIOFileDebugEnter();
    
    globus_free(handle);
    
    GlobusXIOFileDebugExit();
}

typedef struct
{
    globus_xio_operation_t              op;
    globus_l_handle_t *                 handle;
} globus_l_open_info_t;

static
void
globus_l_xio_file_system_open_cb(
    globus_result_t                     result,
    void *                              user_arg)
{
    globus_l_open_info_t *              open_info;
    GlobusXIOName(globus_l_xio_file_system_open_cb);
    
    GlobusXIOFileDebugEnter();
    
    open_info = (globus_l_open_info_t *) user_arg;
    
    if(result == GLOBUS_SUCCESS)
    {
        /* all handles created by me are closed on exec */
        fcntl(open_info->handle->handle, F_SETFD, FD_CLOEXEC);
    }
    else
    {
        globus_l_xio_file_handle_destroy(open_info->handle);
        open_info->handle = GLOBUS_NULL;
    }
    
    GlobusXIODriverFinishedOpen(
        GlobusXIOOperationGetContext(open_info->op),
        open_info->handle,
        open_info->op,
        result);
    
    globus_free(open_info);
    
    GlobusXIOFileDebugExit();
}

/*
 *  open a file
 */
static
globus_result_t
globus_l_xio_file_open(
    void *                              driver_target,
    void *                              driver_attr,
    globus_xio_context_t                context,
    globus_xio_operation_t              op)
{
    globus_l_handle_t *                 handle;
    const globus_l_target_t *           target;
    const globus_l_attr_t *             attr;
    globus_result_t                     result;
    globus_l_open_info_t *              open_info;
    GlobusXIOName(globus_l_xio_file_open);
    
    GlobusXIOFileDebugEnter();
    
    target = (globus_l_target_t *) driver_target;
    attr = (globus_l_attr_t *) 
        driver_attr ? driver_attr : &globus_l_xio_file_attr_default;
    
    result = globus_l_xio_file_handle_init(&handle);
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusXIOErrorWrapFailed(
            "globus_l_xio_file_handle_init", result);
        goto error_handle;
    }
    
    if(target->handle == GLOBUS_XIO_FILE_INVALID_HANDLE)
    {
        open_info = (globus_l_open_info_t *)
            globus_malloc(sizeof(globus_l_open_info_t));
        if(!open_info)
        {
            result = GlobusXIOErrorMemory("open_info");
            goto error_info;
        }
        
        open_info->op = op;
        open_info->handle = handle;
        
        result = globus_xio_system_register_open(
            op,
            target->pathname,
            attr->flags,
            attr->mode,
            &handle->handle,
            globus_l_xio_file_system_open_cb,
            open_info);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusXIOErrorWrapFailed(
                "globus_xio_system_register_open", result);
            goto error_register;
        }
    }
    else
    {
        handle->handle = target->handle;
        GlobusXIODriverFinishedOpen(context, handle, op, GLOBUS_SUCCESS);
    }
    
    GlobusXIOFileDebugExit();
    return GLOBUS_SUCCESS;
    
error_register:
    globus_free(open_info);
    
error_info:
    globus_l_xio_file_handle_destroy(handle);

error_handle:
    GlobusXIOFileDebugExitWithError();
    return result;
}

static
void
globus_l_xio_file_system_close_cb(
    globus_result_t                     result,
    void *                              user_arg)
{
    globus_xio_operation_t              op;
    globus_xio_context_t                context;
    globus_l_handle_t *                 handle;
    GlobusXIOName(globus_l_xio_file_system_close_cb);
    
    GlobusXIOFileDebugEnter();
    
    op = (globus_xio_operation_t) user_arg;
    
    context = GlobusXIOOperationGetContext(op);
    handle = GlobusXIOOperationGetDriverHandle(op);
    
    GlobusXIODriverFinishedClose(op, result);
    globus_xio_driver_context_close(context);
    globus_l_xio_file_handle_destroy(handle);
    
    GlobusXIOFileDebugExit();
}

/*
 *  close a file
 */
static
globus_result_t
globus_l_xio_file_close(
    void *                              driver_handle,
    void *                              attr,
    globus_xio_context_t                context,
    globus_xio_operation_t              op)
{
    globus_l_handle_t *                 handle;
    globus_result_t                     result;
    GlobusXIOName(globus_l_xio_file_close);

    GlobusXIOFileDebugEnter();
    
    handle = (globus_l_handle_t *) driver_handle;
        
    result = globus_xio_system_register_close(
        op,
        handle->handle,
        globus_l_xio_file_system_close_cb,
        op);
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusXIOErrorWrapFailed(
            "globus_xio_system_register_close", result);
        goto error_register;
    }
    
    GlobusXIOFileDebugExit();
    return GLOBUS_SUCCESS;
    
error_register:
    globus_xio_driver_context_close(context);
    globus_l_xio_file_handle_destroy(handle);
    
    GlobusXIOFileDebugExitWithError();
    return result;
}

static
void
globus_l_xio_file_system_read_cb(
    globus_result_t                     result,
    globus_size_t                       nbytes,
    void *                              user_arg)
{
    globus_xio_operation_t              op;
    GlobusXIOName(globus_l_xio_file_system_read_cb);
    
    GlobusXIOFileDebugEnter();
    
    op = (globus_xio_operation_t) user_arg;
    GlobusXIODriverFinishedRead(op, result, nbytes);
    
    GlobusXIOFileDebugExit();
}

/*
 *  read from a file
 */
static
globus_result_t
globus_l_xio_file_read(
    void *                              driver_handle,
    const globus_xio_iovec_t *          iovec,
    int                                 iovec_count,
    globus_xio_operation_t              op)
{
    globus_l_handle_t *                 handle;
    GlobusXIOName(globus_l_xio_file_read);

    GlobusXIOFileDebugEnter();
    
    handle = (globus_l_handle_t *) driver_handle;
    
    if(GlobusXIOOperationGetWaitFor(op) == 0)
    {
        globus_size_t                   nbytes;
        globus_result_t                 result;
        
        result = globus_xio_system_try_read(
            handle->handle, iovec, iovec_count, &nbytes);
        GlobusXIODriverFinishedRead(op, result, nbytes);
        /* dont want to return error here mainly because error could be eof, 
         * which is against our convention to return an eof error on async
         * calls.  Other than that, the choice is arbitrary
         */
        return GLOBUS_SUCCESS;
    }
    else
    {
        return globus_xio_system_register_read(
            op,
            handle->handle,
            iovec,
            iovec_count,
            GlobusXIOOperationGetWaitFor(op),
            globus_l_xio_file_system_read_cb,
            op);
    }
    
    GlobusXIOFileDebugExit();
}

static
void
globus_l_xio_file_system_write_cb(
    globus_result_t                     result,
    globus_size_t                       nbytes,
    void *                              user_arg)
{
    globus_xio_operation_t              op;
    GlobusXIOName(globus_l_xio_file_system_write_cb);
    
    GlobusXIOFileDebugEnter();
    
    op = (globus_xio_operation_t) user_arg;
    GlobusXIODriverFinishedWrite(op, result, nbytes);
    
    GlobusXIOFileDebugExit();
}

/*
 *  write to a file
 */
static
globus_result_t
globus_l_xio_file_write(
    void *                              driver_handle,
    const globus_xio_iovec_t *          iovec,
    int                                 iovec_count,
    globus_xio_operation_t              op)
{
    globus_l_handle_t *                 handle;
    GlobusXIOName(globus_l_xio_file_write);
    
    GlobusXIOFileDebugEnter();
    
    handle = (globus_l_handle_t *) driver_handle;
    
    if(GlobusXIOOperationGetWaitFor(op) == 0)
    {
        globus_size_t                   nbytes;
        globus_result_t                 result;
        
        result = globus_xio_system_try_write(
            handle->handle, iovec, iovec_count, &nbytes);
        GlobusXIODriverFinishedWrite(op, result, nbytes);
        /* Since I am finishing the request in the callstack,
         * the choice to pass the result in the finish instead of below
         * is arbitrary.
         */
        return GLOBUS_SUCCESS;
    }
    else
    {
        return globus_xio_system_register_write(
            op,
            handle->handle,
            iovec,
            iovec_count,
            GlobusXIOOperationGetWaitFor(op),
            globus_l_xio_file_system_write_cb,
            op);
    }
    
    GlobusXIOFileDebugExit();
}

static
globus_result_t
globus_l_xio_file_cntl(
    void *                              driver_handle,
    int                                 cmd,
    va_list                             ap)
{
    globus_l_handle_t *                 handle;
    globus_off_t *                      offset;
    int                                 whence;
    GlobusXIOName(globus_l_xio_file_cntl);
    
    GlobusXIOFileDebugEnter();
    
    handle = (globus_l_handle_t *) driver_handle;
    switch(cmd)
    {
      /* globus_off_t *                 in_out_offset */
      /* globus_xio_file_whence_t       whence */
      case GLOBUS_XIO_FILE_SEEK:
        offset = va_arg(ap, globus_off_t *);
        whence = va_arg(ap, int);
        *offset = lseek(handle->handle, *offset, whence);
        if(*offset < 0)
        {
            return GlobusXIOErrorSystemError("lseek", errno);
        }
        break;

      default:
        return GlobusXIOErrorInvalidCommand(cmd);
        break;
    }
    
    GlobusXIOFileDebugExit();
    return GLOBUS_SUCCESS;
}

static
globus_result_t
globus_l_xio_file_init(
    globus_xio_driver_t *               out_driver,
    va_list                             ap)
{
    globus_xio_driver_t                 driver;
    globus_result_t                     result;
    GlobusXIOName(globus_l_xio_file_init);
    
    GlobusXIOFileDebugEnter();
    
    /* I dont support any driver options, so I'll ignore the ap */
    
    result = globus_xio_driver_init(&driver, "file", GLOBUS_NULL);
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusXIOErrorWrapFailed(
            "globus_l_xio_file_handle_init", result);
        goto error_init;
    }

    globus_xio_driver_set_transport(
        driver,
        globus_l_xio_file_open,
        globus_l_xio_file_close,
        globus_l_xio_file_read,
        globus_l_xio_file_write,
        globus_l_xio_file_cntl);

    globus_xio_driver_set_client(
        driver,
        globus_l_xio_file_target_init,
        GLOBUS_NULL,
        globus_l_xio_file_target_destroy);

    globus_xio_driver_set_attr(
        driver,
        globus_l_xio_file_attr_init,
        globus_l_xio_file_attr_copy,
        globus_l_xio_file_attr_cntl,
        globus_l_xio_file_attr_destroy);
    
    *out_driver = driver;
    
    GlobusXIOFileDebugExit();
    return GLOBUS_SUCCESS;

error_init:
    GlobusXIOFileDebugExitWithError();
    return result;
}

static
void
globus_l_xio_file_destroy(
    globus_xio_driver_t                 driver)
{
    GlobusXIOName(globus_l_xio_file_destroy);
    
    GlobusXIOFileDebugEnter();
    
    globus_xio_driver_destroy(driver);
    
    GlobusXIOFileDebugExit();
}

GlobusXIODefineDriver(
    file,
    &globus_i_xio_file_module,
    globus_l_xio_file_init,
    globus_l_xio_file_destroy);
