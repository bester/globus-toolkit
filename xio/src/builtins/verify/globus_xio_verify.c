#include "globus_xio_driver.h"
#include "globus_xio_load.h"
#include "globus_i_xio.h"
#include "globus_common.h"
#include "globus_xio_verify.h"

#define _SERVER "test_server_string"
#define _TARGET "test_target_string"
#define _HANDLE "test_handle_string"
#define _ATTR   "test_attr_string"

static int
globus_l_xio_verify_activate();

static int
globus_l_xio_verify_deactivate();

#include "version.h"

globus_module_descriptor_t  globus_i_xio_verify_module =
{
    "globus_xio_verify",
    globus_l_xio_verify_activate,
    globus_l_xio_verify_deactivate,
    GLOBUS_NULL,
    GLOBUS_NULL,
    &local_version
};


static globus_result_t
globus_l_xio_verify_attr_init(
    void **                             out_attr)
{
    *out_attr = (void *) strdup(_ATTR);

    return GLOBUS_SUCCESS;
}

static globus_result_t
globus_l_xio_verify_attr_destroy(
    void *                              driver_attr)
{
    char *                              tst_str;

    tst_str = (char *) driver_attr;

    if(strcmp(tst_str, _ATTR) != 0)
    {
        globus_assert(!"Attr string doesn't match");
    }
    return GLOBUS_SUCCESS;
}

static globus_result_t
globus_l_xio_verify_attr_cntl(
    void *                              driver_attr,
    int                                 cmd,
    va_list                             ap)
{
    char *                              tst_str;

    tst_str = (char *) driver_attr;

    if(strcmp(tst_str, _ATTR) != 0)
    {
        globus_assert(!"Attr string doesn't match");
    }

    return GLOBUS_SUCCESS;
}

static globus_result_t
globus_l_xio_verify_attr_copy(
    void **                             dst,
    void *                              src)
{
    char *                              tst_str;

    tst_str = (char *) dst;

    if(strcmp(tst_str, _ATTR) != 0)
    {
        globus_assert(!"Attr string doesn't match");
    }

    *dst = (void *) strdup(_ATTR);
    return GLOBUS_SUCCESS;
}

static globus_result_t
globus_l_xio_verify_server_init(
    void **                             out_server,
    void *                              driver_attr)
{
    *out_server = (void *)strdup(_SERVER);

    return GLOBUS_SUCCESS;
}

static globus_result_t
globus_l_xio_verify_target_init(
    void **                                 out_driver_target,
    globus_xio_operation_t                  target_op,
    const globus_xio_contact_t *            contact_info,
    void *                                  driver_attr)
{
    globus_result_t                         result;
    
    *out_driver_target = (void *)strdup(_TARGET);
    result = globus_xio_driver_client_target_pass(target_op, contact_info);
    if(result != GLOBUS_SUCCESS)
    {
        globus_free(*out_driver_target);
    }
    
    return result;
}

static globus_result_t
globus_l_xio_verify_target_destroy(
    void *                              driver_target)
{
    char *                              tst_str;

    tst_str = (char *) driver_target;
    if(strcmp(tst_str, _TARGET) != 0)
    {
        globus_assert(!"Target string doesn't match");
    }
    free(tst_str);

    return GLOBUS_SUCCESS;
}


void
globus_l_xio_verify_accept_cb(
    globus_i_xio_op_t *                 op,
    globus_result_t                     result,
    void *                              user_arg)
{
    globus_xio_driver_finished_accept(op, strdup(_TARGET), result);
}

static globus_result_t
globus_l_xio_verify_accept(
    void *                              driver_server,
    void *                              driver_attr,
    globus_xio_operation_t              accept_op)
{
    globus_result_t                     res;
    char *                              tst_str;

    tst_str = (char *) driver_server;
    if(strcmp(tst_str, _SERVER) != 0)
    {
        globus_assert(!"Server string doesn't match");
    }

    res = globus_xio_driver_pass_accept(accept_op,
        globus_l_xio_verify_accept_cb, NULL);

    return res;
}

static globus_result_t
globus_l_xio_verify_server_cntl(
    void *                              driver_server,
    int                                 cmd,
    va_list                             ap)
{
    char *                              tst_str;

    tst_str = (char *) driver_server;
    if(strcmp(tst_str, _SERVER) != 0)
    {
        globus_assert(!"Server string doesn't match");
    }

    return GLOBUS_SUCCESS;
}

static globus_result_t
globus_l_xio_verify_server_destroy(
    void *                              driver_server)
{
    char *                              tst_str;

    tst_str = (char *) driver_server;
    if(strcmp(tst_str, _SERVER) != 0)
    {
        globus_assert(!"Server string doesn't match");
    }

    free(tst_str);

    return GLOBUS_SUCCESS;
}

/*
 *  open
 */
void
globus_l_xio_verify_open_cb(
    globus_xio_operation_t              op,
    globus_result_t                     result,
    void *                              user_arg)
{
    globus_xio_driver_handle_t          driver_handle;

    driver_handle = GlobusXIOOperationGetDriverHandle(op);

    globus_xio_driver_finished_open(driver_handle, strdup(_HANDLE), op, result);
}   

static
globus_result_t
globus_l_xio_verify_open(
    void *                              driver_target,
    void *                              driver_attr,
    globus_xio_operation_t              op)
{
    globus_result_t                     res;
    globus_xio_driver_handle_t          driver_handle;
    char *                              tst_str;

    tst_str = (char *) driver_target;
    if(strcmp(tst_str, _TARGET) != 0)
    {
        globus_assert(!"Server string doesn't match");
    }

    res = globus_xio_driver_pass_open(&driver_handle, op,
        globus_l_xio_verify_open_cb, NULL);

    return res;
}

/*
 *  close
 */
void
globus_l_xio_verify_close_cb(
    globus_xio_operation_t              op,
    globus_result_t                     result,
    void *                              user_arg)
{   
    globus_xio_driver_handle_t          driver_handle;

    driver_handle = GlobusXIOOperationGetDriverHandle(op);
    globus_xio_driver_finished_close(op, result);
    globus_xio_driver_handle_close(driver_handle);
}   

static
globus_result_t
globus_l_xio_verify_close(
    void *                              driver_specific_handle,
    void *                              attr,
    globus_xio_driver_handle_t          driver_handle,
    globus_xio_operation_t              op)
{
    globus_result_t                     res;
    char *                              tst_str;

    tst_str = (char *) driver_specific_handle;
    if(strcmp(tst_str, _HANDLE) != 0)
    {
        globus_assert(!"Handle string doesn't match");
    }

    res = globus_xio_driver_pass_close(op,
        globus_l_xio_verify_close_cb, NULL);

    globus_free(tst_str);

    return res;
}

/*
 *  read
 */
void
globus_l_xio_verify_read_cb(
    globus_xio_operation_t              op,
    globus_result_t                     result,
    globus_size_t                       nbytes,
    void *                              user_arg)
{
    globus_xio_driver_finished_read(op, result, nbytes);
}

static
globus_result_t
globus_l_xio_verify_read(
    void *                              driver_specific_handle,
    const globus_xio_iovec_t *          iovec,
    int                                 iovec_count,
    globus_xio_operation_t              op)
{
    globus_result_t                     res;
    globus_size_t                       wait_for;
    char *                              tst_str;

    tst_str = (char *) driver_specific_handle;
    if(strcmp(tst_str, _HANDLE) != 0)
    {
        globus_assert(!"Handle string doesn't match");
    }

    wait_for = GlobusXIOOperationGetWaitFor(op);

    res = globus_xio_driver_pass_read(op, (globus_xio_iovec_t *)iovec, 
        iovec_count, wait_for,
        globus_l_xio_verify_read_cb, NULL);

    return res;
}

/*
 *  write
 */
void
globus_l_xio_verify_write_cb(
    globus_xio_operation_t              op,
    globus_result_t                     result,
    globus_size_t                       nbytes,
    void *                              user_arg)
{
    globus_xio_driver_finished_write(op, result, nbytes);
}

static
globus_result_t
globus_l_xio_verify_write(
    void *                              driver_specific_handle,
    const globus_xio_iovec_t *          iovec,
    int                                 iovec_count,
    globus_xio_operation_t              op)
{
    globus_result_t                     res;
    globus_size_t                       wait_for;
    char *                              tst_str;

    tst_str = (char *) driver_specific_handle;
    if(strcmp(tst_str, _HANDLE) != 0)
    {
        globus_assert(!"Handle string doesn't match");
    }

    wait_for = GlobusXIOOperationGetWaitFor(op);

    res = globus_xio_driver_pass_write( 
        op, (globus_xio_iovec_t *) iovec, iovec_count, wait_for, 
        globus_l_xio_verify_write_cb, NULL);

    return res;
}

static
globus_result_t
globus_l_xio_verify_cntl(
    void *                              driver_specific_handle,
    int                                 cmd,
    va_list                             ap)
{
    char *                              tst_str;

    tst_str = (char *) driver_specific_handle;
    if(strcmp(tst_str, _HANDLE) != 0)
    {
        globus_assert(!"Handle string doesn't match");
    }
    return GLOBUS_SUCCESS;
}

static globus_result_t
globus_l_xio_verify_load(
    globus_xio_driver_t *               out_driver,
    va_list                             ap)
{
    globus_xio_driver_t                 driver;
    globus_result_t                     res;

    res = globus_xio_driver_init(&driver, "verify", NULL);
    if(res != GLOBUS_SUCCESS)
    {
        return res;
    }

    globus_xio_driver_set_transform(
        driver,
        globus_l_xio_verify_open,
        globus_l_xio_verify_close,
        globus_l_xio_verify_read,
        globus_l_xio_verify_write,
        globus_l_xio_verify_cntl,
        NULL);

    globus_xio_driver_set_client(
        driver,
        globus_l_xio_verify_target_init,
        NULL,
        globus_l_xio_verify_target_destroy);

    globus_xio_driver_set_server(
        driver,
        globus_l_xio_verify_server_init,
        globus_l_xio_verify_accept,
        globus_l_xio_verify_server_destroy,
        globus_l_xio_verify_server_cntl,
        globus_l_xio_verify_target_destroy);

    globus_xio_driver_set_attr(
        driver,
        globus_l_xio_verify_attr_init,
        globus_l_xio_verify_attr_copy,
        globus_l_xio_verify_attr_cntl,
        globus_l_xio_verify_attr_destroy);

    *out_driver = driver;

    return GLOBUS_SUCCESS;
}

static void
globus_l_xio_verify_unload(
    globus_xio_driver_t                 driver)
{
    globus_xio_driver_destroy(driver);
}


static
int
globus_l_xio_verify_activate(void)
{
    int                                 rc;

    rc = globus_module_activate(GLOBUS_COMMON_MODULE);

    return rc;
}

static
int
globus_l_xio_verify_deactivate(void)
{
    return globus_module_deactivate(GLOBUS_COMMON_MODULE);
}

GlobusXIODefineDriver(
    verify,
    &globus_i_xio_verify_module,
    globus_l_xio_verify_load,
    globus_l_xio_verify_unload);
