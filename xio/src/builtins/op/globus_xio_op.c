#include "globus_xio_driver.h"
#include "globus_xio_load.h"
#include "globus_i_xio.h"
#include "globus_common.h"
#include "globus_xio_op.h"

static int
globus_l_xio_op_activate();

static int
globus_l_xio_op_deactivate();

void
globus_l_xio_op_close_cb(
    globus_xio_operation_t                  op,
    globus_result_t                         result,
    void *                                  user_arg);

void
globus_l_xio_op_obb_read_cb(
    globus_xio_operation_t              op,
    globus_result_t                     result,
    globus_size_t                       nbytes,
    void *                              user_arg);

typedef struct globus_l_xio_op_handle_s
{
    int                                     count;
    globus_mutex_t                          mutex;
    globus_xio_iovec_t                      iovec;
    globus_byte_t                           bs_buf[1];

    globus_xio_operation_t                  close_op;
    globus_xio_context_t                    context;
} globus_l_xio_op_handle_t;

#include "version.h"

static globus_module_descriptor_t  globus_i_xio_op_module =
{
    "globus_xio_op",
    globus_l_xio_op_activate,
    globus_l_xio_op_deactivate,
    GLOBUS_NULL,
    GLOBUS_NULL,
    &local_version
};

globus_result_t
globus_l_xio_op_target_destroy(
    void *                              driver_target)
{
    return GLOBUS_SUCCESS;
}


/*
 *  read
 */
void
globus_l_xio_op_obb_write_cb(
    globus_xio_operation_t              op,
    globus_result_t                     result,
    globus_size_t                       nbytes,
    void *                              user_arg)
{
    globus_l_xio_op_handle_t *          op_handle;
    globus_result_t                     res;
    globus_bool_t                       done = GLOBUS_FALSE;

    op_handle = (globus_l_xio_op_handle_t *) user_arg;

    globus_mutex_lock(&op_handle->mutex);
    {
        op_handle->count++;
        if(op_handle->count < 10 && result == GLOBUS_SUCCESS)
        {
            GlobusXIODriverPassRead(res, op, 
                &op_handle->iovec, 1, 1,
                globus_l_xio_op_obb_read_cb, op_handle);

            if(res != GLOBUS_SUCCESS)
            {
                result = res;
                done = GLOBUS_TRUE;
            }
        }
        else
        {
            done = GLOBUS_TRUE;
        }

        if(done)
        {
            op_handle->count = 10; /* fake close into working */
            globus_xio_driver_operation_destroy(op);
            if(op_handle->close_op != NULL)
            {
                GlobusXIODriverPassClose(res, op_handle->close_op,
                    globus_l_xio_op_close_cb, op_handle);
            }
        }
    }
    globus_mutex_unlock(&op_handle->mutex);
}

void
globus_l_xio_op_obb_read_cb(
    globus_xio_operation_t              op,
    globus_result_t                     result,
    globus_size_t                       nbytes,
    void *                              user_arg)
{
    globus_l_xio_op_handle_t *          op_handle;
    globus_result_t                     res;

    op_handle = (globus_l_xio_op_handle_t *) user_arg;

    GlobusXIODriverPassWrite(res, op, 
        &op_handle->iovec, 
        1, 1,
        globus_l_xio_op_obb_write_cb, op_handle);

    if(res != GLOBUS_SUCCESS)
    {
        globus_mutex_lock(&op_handle->mutex);
        {
            op_handle->count = 10; /* fake close into working */
            globus_xio_driver_operation_destroy(op);
            if(op_handle->close_op != NULL)
            {
                GlobusXIODriverPassClose(res, op_handle->close_op,
                    globus_l_xio_op_close_cb, op_handle);
            }
        }
        globus_mutex_unlock(&op_handle->mutex);
    }
}


/*
 *  open
 */
void
globus_l_xio_op_open_cb(
    globus_xio_operation_t              op,
    globus_result_t                     result,
    void *                              user_arg)
{
    globus_xio_context_t                context;
    globus_l_xio_op_handle_t *          op_handle = NULL;
    globus_xio_operation_t              driver_op;

    context = GlobusXIOOperationGetContext(op);

    if(result == GLOBUS_SUCCESS)
    {
        op_handle = (globus_l_xio_op_handle_t *) 
            globus_malloc(sizeof(globus_l_xio_op_handle_t));
        op_handle->count = 0;
        op_handle->iovec.iov_base = &op_handle->bs_buf;
        op_handle->iovec.iov_len = 1;
        op_handle->close_op = NULL;
        op_handle->context = context;

        globus_mutex_init(&op_handle->mutex, NULL);

        result = globus_xio_driver_operation_create(
            &driver_op, op_handle->context);
        if(result == GLOBUS_SUCCESS)
        {
            GlobusXIODriverPassRead(result, driver_op, 
                &op_handle->iovec, 1, 1,
                globus_l_xio_op_obb_read_cb, op_handle);
        }
    }

    GlobusXIODriverFinishedOpen(context, op_handle, op, result);
}   

static
globus_result_t
globus_l_xio_op_open(
    void *                              driver_target,
    void *                              driver_attr,
    globus_xio_operation_t              op)
{
    globus_result_t                     res;
    globus_xio_context_t                context;
  
    GlobusXIODriverPassOpen(res, context, op, \
        globus_l_xio_op_open_cb, NULL);

    return res;
}

/*
 *  close
 */
void
globus_l_xio_op_close_cb(
    globus_xio_operation_t              op,
    globus_result_t                     result,
    void *                              user_arg)
{   
    globus_xio_context_t                context;

    context = GlobusXIOOperationGetContext(op);
    GlobusXIODriverFinishedClose(op, result);
    globus_xio_driver_context_close(context);
}   

static
globus_result_t
globus_l_xio_op_close(
    void *                              driver_handle,
    void *                              attr,
    globus_xio_context_t                context,
    globus_xio_operation_t              op)
{
    globus_result_t                     res = GLOBUS_SUCCESS;
    globus_l_xio_op_handle_t *          op_handle;

    op_handle = (globus_l_xio_op_handle_t *) driver_handle;

    globus_mutex_lock(&op_handle->mutex);
    {
        op_handle->close_op = op;
        if(op_handle->count >= 10)
        {
            GlobusXIODriverPassClose(res, op_handle->close_op,
                globus_l_xio_op_close_cb, op_handle);
        }
    }
    globus_mutex_unlock(&op_handle->mutex);

    return res;
}

/*
 *  read
 */
void
globus_l_xio_op_read_cb(
    globus_xio_operation_t              op,
    globus_result_t                     result,
    globus_size_t                       nbytes,
    void *                              user_arg)
{
    GlobusXIODriverFinishedRead(op, result, nbytes);
}

static
globus_result_t
globus_l_xio_op_read(
    void *                              driver_handle,
    const globus_xio_iovec_t *          iovec,
    int                                 iovec_count,
    globus_xio_operation_t              op)
{
    globus_result_t                     res;
    globus_size_t                       wait_for;

    wait_for = GlobusXIOOperationGetWaitFor(op);

    GlobusXIODriverPassRead(res, op, iovec, iovec_count, wait_for, \
        globus_l_xio_op_read_cb, NULL);

    return res;
}

/*
 *  write
 */
void
globus_l_xio_op_write_cb(
    globus_xio_operation_t              op,
    globus_result_t                     result,
    globus_size_t                       nbytes,
    void *                              user_arg)
{
    GlobusXIODriverFinishedWrite(op, result, nbytes);
}

static
globus_result_t
globus_l_xio_op_write(
    void *                              driver_handle,
    const globus_xio_iovec_t *          iovec,
    int                                 iovec_count,
    globus_xio_operation_t              op)
{
    globus_result_t                     res;
    globus_size_t                       wait_for;

    wait_for = GlobusXIOOperationGetWaitFor(op);

    GlobusXIODriverPassWrite(res, op, iovec, iovec_count, wait_for, \
        globus_l_xio_op_write_cb, NULL);

    return res;
}

static globus_result_t
globus_l_xio_op_load(
    globus_xio_driver_t *               out_driver,
    va_list                             ap)
{
    globus_xio_driver_t                 driver;
    globus_result_t                     res;

    res = globus_xio_driver_init(&driver, "op", NULL);
    if(res != GLOBUS_SUCCESS)
    {
        return res;
    }

    globus_xio_driver_set_transform(
        driver,
        globus_l_xio_op_open,
        globus_l_xio_op_close,
        globus_l_xio_op_read,
        globus_l_xio_op_write,
        NULL);

    *out_driver = driver;

    return GLOBUS_SUCCESS;
}

static void
globus_l_xio_op_unload(
    globus_xio_driver_t                 driver)
{
    globus_xio_driver_destroy(driver);
}


static
int
globus_l_xio_op_activate(void)
{
    int                                 rc;

    rc = globus_module_activate(GLOBUS_COMMON_MODULE);

    return rc;
}

static
int
globus_l_xio_op_deactivate(void)
{
    return globus_module_deactivate(GLOBUS_COMMON_MODULE);
}

GlobusXIODefineDriver(
    op,
    &globus_i_xio_op_module,
    globus_l_xio_op_load,
    globus_l_xio_op_unload);
