#define GLOBUS_XIO_HANDLE_DEFAULT_OPERATION_COUNT   2

/********************************************************************
 *                   data structure macros
 *******************************************************************/
#define Globus_I_XIO_Handle_Destroy(h)                              \
{                                                                   \
    globus_i_xio_handle_t *                         _h;             \
                                                                    \
    _h = (h);                                                       \
    assert(_h->ref == 0);                                           \
    globus_mutex_destroy(_h->mutex);                                \
    globus_memory_destroy(_h->op_memory);                           \
    /* TODO what about context array */                             \
                                                                    \
    globus_free(_h);                                                \
    h = NULL;                                                       \
}

#define Globus_I_XIO_Handle_Create(h, t, c)                         \
{                                                                   \
    globus_i_xio_target_t *                         _t;             \
    globus_i_xio_handle_t *                         _h;             \
    globus_i_xio_context_t *                        _c;             \
                                                                    \
    _t = (t);                                                       \
    _c = (c);                                                       \
                                                                    \
    /* allocate and intialize the handle structure */               \
    _h = (struct globus_i_xio_handle_s *) globus_malloc(            \
                    sizeof(struct globus_i_xio_handle_s));          \
    if(_h != NULL)                                                  \
    {                                                               \
        globus_mutex_init(&_h->mutex, NULL);                        \
        /*                                                          \
         *  initialize memory for the operation structure           \
         *  The operation is a stretchy array.  The size of the     \
         *  operation structure plus the size of the entry array    \
         */                                                         \
        globus_memory_init(                                         \
            &_h->op_memory,                                         \
            sizeof(globus_i_xio_operation_t) +                      \
                (sizeof(globus_i_xio_op_entry_s) *                  \
                    _t->stack_size - 1),                            \
        GLOBUS_XIO_HANDLE_DEFAULT_OPERATION_COUNT);                 \
        _h->stack_size = _t->stack_size;                            \
        /* context should up ref count for this assignment */       \
        _h->context = _c;                                           \
        _h->ref = 1; /* set count for its own reference */          \
        _h->op_list = NULL;                                         \
        _h->close_op = NULL;                                        \
        _h->open_timeout = NULL;                                    \
        _h->close_timeout = NULL;                                   \
        _h->read_timeout = NULL;                                    \
        _h->write_timeout = NULL;                                   \
    }                                                               \
    h = _h;                                                         \
}

#define Globus_I_XIO_Context_Create(c, t, a)                        \
{                                                                   \
    globus_i_xio_context_t *                        _c;             \
    globus_i_xio_target_t *                         _t;             \
    globus_i_xio_attr_t *                           _a;             \
                                                                    \
    _t = (t);                                                       \
    _a = (a);                                                       \
                                                                    \
    /* allocate and initialize context */                           \
    _c = (globus_i_xio_context_t *)                                 \
        globus_malloc(sizeof(globus_i_xio_context_t) +              \
            (sizeof(globus_i_xio_context_entry_t)                   \
                * _t->stack_size - 1));                             \
    _c->size = _t->stack_size;                                      \
    globus_mutex_init(&_c->mutex, NULL);                            \
    /* set reference count to 1 for this structure */               \
    _c->ref = 1;                                                    \
                                                                    \
    for(ctr = 0; ctr < _c->size; ctr++)                             \
    {                                                               \
        _c->entry_array[ctr].driver = _t->target_stack[ctr].driver; \
        _c->entry_array[ctr].target = t->target_stack[ctr].target;  \
        _c->entry_array[ctr].driver_handle = NULL;                  \
        _c->entry_array[ctr].driver_attr =                          \
            globus_l_xio_attr_find_driver(_a,                       \
                t->target_stack[ctr].driver);                       \
        _c->entry_array[ctr].is_limited = GLOBUS_FALSE;             \
    }                                                               \
}

#define Globus_I_XIO_Context_Destroy(c)                             \
{                                                                   \
    _c = (c);                                                       \
    globus_free(_c);                                                \
}
#define Globus_I_XIO_Operation_Create(op, type, h)                  \
do                                                                  \
{                                                                   \
    globus_i_xio_operation_t *                      _op;            \
    globus_i_xio_handle_t *                         _h;             \
                                                                    \
    _h = (h);                                                       \
    globus_mutex_lock(_h->mutex);                                   \
    /* create operation for the open */                             \
    globus_list_inset(&_h->op_list, _op);                           \
    _h->ref++;                                                      \
    _op = (globus_i_xio_operation_t * )                             \
            globus_memory_pop_node(&_h->op_memory);                 \
    _op->op_type = type;                                            \
    _op->xio_handle = _h;                                           \
    _op->timeout_cb = NULL;                                         \
    _op->op_list = NULL;                                            \
    _op->cached_res = GLOBUS_SUCCESS;                               \
    _op->stack_size = _h->stack_size;                               \
    _op->context = _h->context;                                     \
    _op->ndx = 0;                                                   \
    _op->ref = 0;                                                   \
    _op->space = GLOBUS_CALLBACK_GLOBAL_SPACE;                      \
    globus_mutex_unlock(_h->mutex);                                 \
} while(0)

#define Globus_I_XIO_Operation_Destroy(op)                          \
do                                                                  \
{                                                                   \
    globus_i_xio_operation_t *                      _op;            \
                                                                    \
    _op = (op);                                                     \
    assert(_op->ref == 0);                                          \
    globus_list_remove(&_op->xio_handle->op_list,                   \
        globus_list_search(_op->xio_handle->op_list, _op));         \
    _op->xio_handle->ref--;                                         \
    globus_memory_push_node(&_op->xio_handle->op_memory, _op);      \
} while(0)



#define GlobusIXIOOperationFree(op)                                 \
do                                                                  \
{                                                                   \
    globus_i_xio_operation_t *                          _op;        \
                                                                    \
    _op = (op);                                                     \
    assert(_op->ref == 0);                                          \
                                                                    \
    globus_list_remove(&_op->xio_handle->op_list,                   \
        globus_list_search(_op->xio_handle->op_list, _op));         \
    globus_memory_push(&_op->xio_handle->op_memory, _op);           \
} while(0)

/********************************************************************
 *                      Internal functions 
 *******************************************************************/

/*
 *   this is called locked
 *
 *   if RETURNS true then the user should free the structure
 */
globus_bool_t
globus_i_xio_handle_dec(
    globus_i_xio_handle_t *                     xio_handle)
{
    xio_handle->ref--;
    if(xio_handle->ref == 0)
    {
        /* if the handle ref gets down to zero we must be in one
         * of the followninf staes.  The statement is that the handle
         * only goes away when it is closed or a open fails
         */
        assert(xio_handle->state == GLOBUS_XIO_HANDLE_STATE_OPEN_FAILED ||
            xio_handle->state == GLOBUS_XIO_HANDLE_STATE_CLOSED);

        /* TODO: we may need  to kick out a callback */
        if(xio_handle->state == GLOBUS_XIO_HANDLE_STATE_CLOSED)
        {
        }

        /* TODO: dec context ref */
        return GLOBUS_TRUE;
    }

    return GLOBUS_FALSE;
}

/*
 *  this could be built into the finished macro
 */
void
globus_i_xio_top_open_callback(
    globus_xio_operation_t                      op,
    globus_result_t                             result,
    void *                                      user_arg)
{
    globus_bool_t                               destroy_handle = GLOBUS_FALSE;
    globus_i_xio_handle_t *                     xio_handle;
    globus_result_t                             res = GLOBUS_SUCCESS;

    xio_handle = op->xio_handle;
    /* set handle state to OPENED */
    globus_mutex_lock(&xio_handle->mutex);
    {
        if(result != GLOBUS_SUCCESS)
        {
            /* TODO: clean up the resources */
            xio_handle->state = GLOBUS_XIO_HANDLE_STATE_OPEN_FAILED;
            res = result;
        }
        else
        {
            xio_handle->state = GLOBUS_XIO_HANDLE_STATE_OPEN;
        }

        /* set to finished */
        op->op_type = GLOBUS_L_XIO_OPERATION_TYPE_FINISHED;

        if(op->timeout_cb != NULL)
        {
            /* 
             * unregister the cancel
             */
            op->timeout_set = GLOBUS_FALSE;

            /* if the unregister fails we will get the callback */
            if(globus_i_xio_timer_unregister_timeout(op))
            {
                /* at this point we know timeout won't happen */
                op->ref--;
            }
        }
    }
    globus_mutex_unlock(&xio_handle->mutex);

    /* 
     *  when at the top don't worry about the cancel
     *  just act as though we missed it
     */
    /*
     *  if in a space or within the register call stack
     *  we must register a one shot
     */
    op->cached_res = res;
    if(op->space != GLOBUS_CALLBACK_GLOBAL_SPACE || 
       entry->in_register)
    {
        /* register a oneshot callback */
        globus_callback_space_register_oneshot(
            NULL,
            NULL,
            globus_l_xio_user_callback_kickout,
            (void *)op,
            op->space);
    } 
    /* in all other cases we can just call callback */
    else
    {
        globus_l_xio_user_callback_kickout((void *)op);
    }
}

/*
 *   this is the notification callback telling xio that all drivers have 
 * 
 */
void
globus_i_xio_top_close_callback(
    globus_xio_driver_operation_t               op,
    globus_result_t                             result,
    void *                                      user_arg);
{
    globus_i_xio_handle_t *                     xio_handle;
    globus_result_t                             res;

    xio_handle = op->xio_handle;

    globus_mutex_lock(&xio_handle->mutex);
    {
        xio_handle->state = GLOBUS_XIO_HANDLE_STATE_CLOSED;
        /* set to finished */
        op->op_type = GLOBUS_L_XIO_OPERATION_TYPE_FINISHED;

        if(op->timeout_cb != NULL)
        {
            /* unregister the cancel */
            op->timeout_set = GLOBUS_FALSE;

            /* if the unregister fails we will get the callback */
            if(globus_i_xio_timer_unregister_timeout(op))
            {
                /* at this point we know timeout won't happen */
                op->ref--;
            }
        }
    }
    globus_mutex_unlock(&xio_handle->mutex);

    /* 
     *  when at the top don't worry about the cancel
     *  just act as though we missed it
     */
    /*
     *  if in a space or within the register call stack
     *  we must register a one shot
     */
    op->cached_res = res;
    if(op->space != GLOBUS_CALLBACK_GLOBAL_SPACE ||
       entry->in_register)
    {
        /* register a oneshot callback */
        globus_callback_space_register_oneshot(
            NULL,
            NULL,
            globus_l_xio_user_callback_kickout,
            (void *)op,
            op->space);
    }
    /* in all other cases we can just call callback */
    else
    {
        globus_l_xio_user_callback_kickout((void *)op);
    }
}


/*
 *   called by the callback code.
 *   registerd by finished op when the final (user) callback
 *   is in a callback space, or if it is under the registraton
 *   call within the same callstack
 */
void
globus_l_xio_user_callback_kickout(
    void *                                      user_arg)
{
    globus_i_xio_operation_t *                  op;
    globus_i_xio_handle_t *                     xio_handle;

    op = (globus_i_xio_operation_t *) user_arg;
    xio_handle = op->xio_handle;

    op->cb(xio_handle, op->cached_res, op->user_arg);

    globus_mutex_lock(&xio_handle->mutex);
    {
        /* decrement reference for the open operation */
        op->ref--;
        if(op->ref == 0)
        {
            GlobusIXIOOperationFree(op);
            /* remove refrence for operation */
            destroy_handle = globus_i_xio_handle_dec(xio_handle);
            /* destroy handle cannot possibly be true yet */
            assert(!destroy_handle);
        }

        /* if the open failed or we are closing */
        if(res != GLOBUS_SUCCESS && 
           (xio_handle->state == OPEN_FAILED ||
            xio_handle->state == CLOSED))
        {
            destroy_handle = globus_i_xio_handle_dec(xio_handle);
        }
        else if(xio_handle->state = STATE_CLOSING)
        {
            /* in this state we WILL have a close_op */
            assert(xio_handle->close_op);

            /* allow the close operation to be passed down the stack */
            globus_l_xio_start_close(xio_handle->close_op);
        }
    }
    globus_mutex_unlock(&xio_handle->mutex);

    if(destroy_handle)
    {
        Globus_I_XIO_Handle_Destroy(xio_handle);
    }
}

/*
 *   this function is called by the callback code.
 *
 *   it is registered as a oneshot to get out of the callstack
 *   when operation is finished (finish_op is called) from within 
 *   the same callstack in which it was registered
 */
void
globus_l_xio_open_driver_kickout(
    void *                                      user_arg)
{
    globus_i_xio_operation_t *                  op;

    op = (globus_i_xio_operation_t *) user_arg;
    op->entry_array[op->ndx].cb(op, op->cached_res, 
        op->entry_array[op->ndx].user_arg);
}

void
globus_l_xio_data_driver_kickout(
    void *                                      user_arg)
{
    globus_i_xio_operation_t *                  op;

    op = (globus_i_xio_operation_t *) user_arg;
    op->entry_array[op->ndx].data_cb(
        op, 
        op->cached_res, 
        op->entry_array[op->ndx].nbytes,
        op->entry_array[op->ndx].user_arg);
}

/*
 *  this starts the cancel processes for the given operation.
 * 
 *  the cancel flag is to true.  every pass checks this flag and
 *  if cancel is true the pass fails and the above driver should
 *  do what it needs to do to cancel the operation.  If a driver
 *  is able to cancel it will call GlobusXIODriverCancelEanble()
 *  if at the time this function is called the operations cancel
 *  flag is set to true we register a oneshot for the cancel.
 * 
 *  If a cencel occurs while a driver is registered to receive 
 *  cancel notification then the callback is delivered to it.
 *
 *  The framework has little else to do with cancel.  The operation
 *  will come back up via the normal routes with an error.
 */
globus_result_t
globus_l_xio_operation_cancel(
    globus_i_xio_operation_t *                  op)
{
    globus_bool_t                               tmp_rc;

    /* 
     * if the user oks the cancel then remove the timeout from 
     * the poller
     */
    tmp_rc = globus_i_xio_timer_unregister_timeout(op);
    /* since in callback this will always be true */
    assert(tmp_rc);

    /*
     * set cancel flag
     * if a driver has a registered callback it will be called
     * if it doesn't the next pass or finished will pick it up
     */
    op->canceled = GLOBUS_TRUE;
    if(op->cancel_callback != NULL)
    {
        op->cancel_callback(op);
    }

    return GLOBUS_SUCCESS;
}

globus_bool_t
globus_l_xio_timeout_callback(
    void *                                      user_arg)
{
    globus_i_xio_operation_t *                  op;
    globus_bool_t                               rc = GLOBUS_FALSE;
    globus_bool_t                               punt;
    globus_bool_t                               destroy_handle = GLOBUS_FALSE;
    globus_i_xio_handle_t *                     xio_handle;

    op = (globus_i_xio_operation_t *) user_arg;
    xio_handle = op->xio_handle;

    globus_mutex_lock(&xio_handle->mutex);
    {
        /* user alread got finished callback, nothing to cancel */
        if(op->op_type == GLOBUS_L_XIO_OPERATION_TYPE_FINISHED)
        {
            rc = GLOBUS_TRUE;
            punt = GLOBUS_TRUE;

            /* if finished we will not call callback but we need to doc
               the reference and free the operation.  if the handle is
               in the open failed or close state we may need to 
               free the memory associated with it */
            op->ref--;
            if(op->ref == 0)
            {
                GlobusIXIOOperationFree(op);
                destroy_handle = globus_i_xio_handle_dec(xio_handle);
            }
        }
        else if(op->block_timeout)
        {
            rc = GLOBUS_FALSE;
            punt = GLOBUS_TRUE;
        }
    }
    globus_mutex_unlock(&xio_handle->mutex);

    if(destroy_handle)
    {
        Globus_I_XIO_Handle_Destroy(xio_handle);
    }

    if(punt)
    {
        return rc;
    }

    /* if we get here there beter be user interest */
    assert(op->user_timeout_callback != NULL);

    if(op->user_timeout_callback(
            op->xio_handle, 
            op->op_type, 
            op->user_timeout_arg))
    {
        /*  
         * lock the op for the duration of the cancel notification
         * process.  The driver notificaiotn callback os called locked
         * this insures that the driver will not receive a callback 
         * once CancelDisallow is called 
         */
        globus_mutex_lock(&xio_handle->cancel_mutex);
        {
            globus_l_xio_operation_cancel(op);
        }
        globus_mutex_lock(&xio_handle->cancel_mutex);
        rc = GLOBUS_FALSE;
    }

    return rc;
}


globus_result_t
globus_l_xio_register_open(
    globus_i_xio_handle_t **                    handle,
    globus_i_xio_attr_t *                       attr,
    globus_xio_callback_t                       cb,
    void *                                      user_arg)
{
    globus_i_xio_operation_t *                  op;
    globus_i_xio_context_t *                    tmp_context;

    /* set state to opening */
    l_handle->GLOBUS_XIO_HANDLE_STATE_OPENING;

    /* get timeouts */
    l_handle->open_timeout = attr->open_timeout;
    GlobusTimeReltimeCopy(l_handle->open_timeout_period,                \
        attr->open_timeout_period);
    l_handle->read_timeout = attr->read_timeout;
    GlobusTimeReltimeCopy(l_handle->read_timeout_period,                \
        attr->read_timeout_period);
    l_handle->write_timeout = attr->write_timeout;
    GlobusTimeReltimeCopy(l_handle->write_timeout_period,               \
        attr->write_timeout_period);
    l_handle->close_timeout = attr->close_timeout;
    GlobusTimeReltimeCopy(l_handle->close_timeout_period,               \
        attr->close_timeout_period);

    /* 
     * create operation for open.  this will add operation to the handles
     * op list and increase the reference count
     */
    Globus_I_XIO_Operation_Create(op, GLOBUS_XIO_OPERATION_TYPE_OPEN, l_handle);
    if(op == NULL)
    {
        res = GlobusXIOErrorMemoryAlloc("globus_xio_register_open");
        goto err;
    }
    op->space = Globus_XIO_Attr_Get_Space(attr);
    op->op_type = GLOBUS_L_XIO_OPERATION_TYPE_OPEN;

    
    /* register timeout */
    if(l_handle->open_timeout_cb != NULL)
    {
        /* op the operatin reference count for this */
        op->ref++;
        op->timeout_cb = l_handle->open_timeout_cb;
        res = globus_i_xio_timer_register_timeout(
                g_globus_l_xio_timeout_timer,
                op,
                &op->progress,
                globus_l_xio_timeout_callback,
                &l_handle->open_timeout_period);
        if(res != GLOBUS_SUCCESS)
        {
            goto err;
        }
    }

    Globus_XIO_Driver_Pass_Open(res, tmp_context, \
        globus_i_xio_top_open_callback, cb, user_arg);

    if(res != GLOBUS_SUCCESS)
    {
        goto err;
    }

    *handle = l_handle;

    return GLOBUS_SUCCESS;

    /*
     * error handling 
     */
  err:
    /* TODO: make sure this doesn't trip an assertion */
    /* do not properly destry since this is a malloc failure */
    if(op != NULL)
    {
        Globus_I_XIO_Operation_Destroy(op);
        Globus_I_XIO_Handle_Destroy(l_handle);
        Globus_I_XIO_Context_Destroy(l_context);
    }

    return res;
}

/*
 *  this is called locked
 */
globus_result_t
globus_l_xio_register_writev(
    globus_xio_handle_t                         xio_handle,
    globus_iovec_t *                            iovec,
    int                                         iovec_count,
    globus_i_xio_data_descriptor_t              dd,
    globus_xio_callback_t                       cb,
    void *                                      user_arg)
{
    globus_i_xio_operation_t *                  l_op;
    globus_result_t                             res;

    /* 
     * create operation for open.  this will add operation to the handles
     * op list and increase the reference count
     */
    Globus_I_XIO_Operation_Create(l_op, GLOBUS_XIO_OPERATION_TYPE_WRITE, \
        xio_handle);
    if(l_op == NULL)
    {
        return GlobusXIOErrorMemoryAlloc("globus_xio_register_open");
    }
    op->space = Globus_XIO_DD_Get_Space(dd);
    op->op_type = GLOBUS_XIO_OPERATION_TYPE_WRITE;

    /* register timeout */
    if(xio_handle->write_timeout_cb != NULL)
    {
        /* op the operatin reference count for this */
        op->ref++;
        op->timeout_cb = xio_handle->write_timeout_cb;
        res = globus_i_xio_timer_register_timeout(
                g_globus_l_xio_timeout_timer,
                op,
                &op->progress,
                globus_l_xio_timeout_callback,
                &l_handle->open_timeout_period);
        if(res != GLOBUS_SUCCESS)
        {
            goto err;
        }
    }

    Globus_XIO_Driver_Pass_Write(res, l_op, iovec, iovec_count,         \
        globus_i_xio_top_open_callback, (void *)l_op);
    if(res != GLOBUS_SUCCESS)
    {
        goto err;
    }

    return GLOBUS_SUCCESS;

  err:

    if(l_op != NULL)
    {
        l_op->ref--;
        Globus_I_XIO_Operation_Destroy(l_op);

        /* remove the refernce of the operation, but assert thta it is not 0 */
        destroy_handle = globus_i_xio_handle_dec(xio_handle); 
        assert(!destroy_handle);
    }

    return res;
}

globus_result_t
globus_l_xio_start_close(
    globus_i_xio_operation_t *                  op)
{
    globus_i_xio_handle_t *                     xio_handle;
    globus_result_t                             res;

    xio_handle = op->xio_handle;

   /* if the op list is empty we can push the close through the stack */
    /* register timeout */
    if(xio_handle->close_timeout_cb != NULL)
    {
        /* op the operatin reference count for this */
        op->ref++;
        op->timeout_cb = xio_handle->close_timeout_cb;
        res = globus_i_xio_timer_register_timeout(
                g_globus_l_xio_timeout_timer,
                op,
                &op->progress,
                globus_l_xio_timeout_callback,
                &xio_handle->close_timeout_period);
        if(res != GLOBUS_SUCCESS)
        {
            return res;
        }
    }

    Globus_XIO_Driver_Pass_Close(res, op, globus_i_xio_top_close_callback, \
        (void *)op);
    if(res != GLOBUS_SUCCESS)
    {
        return res;
    }

    return GLOBUS_SUCCESS;

}

globus_result_t
globus_l_xio_register_close(
    globus_i_xio_handle_t *                     xio_handle,
    globus_i_xio_attr_t *                       attr,
    globus_xio_callback_t                       cb,
    void *                                      user_arg)
{
    globus_i_xio_operation_t *                  op;
    globus_i_xio_operation_t *                  tmp_op;

    Globus_I_XIO_Operation_Create(op, GLOBUS_XIO_OPERATION_TYPE_WRITE, \
        xio_handle);
    if(op == NULL)
    {
        return GlobusXIOErrorMemoryAlloc("globus_xio_register_open");
    }
    op->space = Globus_XIO_DD_Get_Space(attr);
    op->op_type = GLOBUS_XIO_OPERATION_TYPE_CLOSE;

    xio_handle->state = GLOBUS_XIO_HANDLE_STATE_CLOSING;

    /* 
     *  if the user requests a cancel kill all open ops 
     */
    if(attr->close_cancel)
    {
        globus_mutex_lock(&xio_handle->cancel_mutex);

        for(list = xio_handle->op_list;
            !globus_list_empty(list);
            list = globus_list_rest(list))
        {
            tmp_op = (globus_i_xio_operation_t *) globus_list_first(list);
            globus_l_xio_operation_cancel(tmp_op);
        }

        globus_mutex_unlock(&xio_handle->cancel_mutex);
    }

    /* if the op list is not empty we must delay the close */
    if(!globus_list_empty(xio_handle->op_list))
    {
        xio_handle->close_op = op;
        return GLOBUS_SUCCESS;
    }

    res = globus_l_xio_start_close(op);
    if(res != GLOBUS_SUCCESS)
    {
        goto err;
    }
    return GLOBUS_SUCCESS;

  err:
    return res;
}

/********************************************************************
 *                      API functions 
 *******************************************************************/
globus_result_t
globus_xio_handle_cntl(
    globus_xio_handle_t                         handle,
    globus_xio_driver_t                         driver,
    int                                         cmd,
    ...)
{
}

globus_result_t
globus_xio_driver_open(
    globus_xio_driver_context_t *               context,
    globus_xio_driver_operation_t               op,
    globus_xio_driver_callback_t                cb,
    void *                                      user_arg);

globus_result_t
globus_xio_driver_finished_open(
    globus_xio_driver_context_t                 context,
    globus_xio_driver_operation_t               open_op);

typedef globus_result_t
(*globus_xio_driver_transform_open_t)(
    void **                                     driver_handle,
    void *                                      driver_handle_attr,
    void *                                      target,
    globus_xio_driver_operation_t               op);

/**
 *  transport open
 */
typedef globus_result_t
(*globus_xio_driver_transport_open_t)(
    void **                                     driver_handle,
    void *                                      driver_handle_attr,
    void *                                      target,
    globus_xio_driver_context_t                 context,
    globus_xio_driver_operation_t               op);


/*
 *  open
 */
globus_result_t
globus_xio_register_open(
    globus_xio_handle_t *                       handle,
    globus_xio_attr_t                           attr,
    globus_xio_target_t                         target,
    globus_xio_callback_t                       cb,
    void *                                      user_arg)
{
    globus_i_xio_handle_t *                     l_handle = NULL;
    globus_i_xio_target_t *                     l_target;
    globus_i_xio_context_t *                    l_context = NULL;
    globus_result_t                             res = GLOBUS_SUCCESS;

    if(handle == NULL)
    {
        return GlobusXIOErrorBadParameter("globus_xio_register_open");
    }
    if(target == NULL)
    {
        return GlobusXIOErrorBadParameter("globus_xio_register_open");
    }

    *handle = NULL; /* initialze to be nice touser */
    l_target = (struct globus_i_xio_target_s *) target;

    /* this is gaurenteed to be greater than zero */
    assert(l_target->stack_size > 0);

    /* allocate and initialize context */
    Globus_I_XIO_Context_Create(l_context, l_target, attr);
    if(l_context == NULL)
    {
        res = GlobusXIOErrorMemoryAlloc("globus_xio_register_open");
        goto err;
    }

    /* 
     *  up reference count on the context to account for the handles
     *  association with it 
     */
    l_context->ref++;
    /* allocate and intialize the handle structure */
    Globus_I_XIO_Handle_Create(l_handle, l_target, l_context);
    if(l_handle == NULL)
    {
        res = GlobusXIOErrorMemoryAlloc("globus_xio_register_open");
        goto err;
    }

    res = globus_l_xio_register_open(
            handle,
            attr,
            cb,
            user_arg);
    if(res != GLOBUS_SUCCESS)
    {
        goto err;
    }

    return GLOBUS_SUCCESS;

  err:
    /* TODO: make sure this doesn't trip an assertion */
    /* do not properly destry since this is a malloc failure */
    if(l_context != NULL)
    {
        Globus_I_XIO_Handle_Destroy(l_handle);
        Globus_I_XIO_Context_Destroy(l_context);
    }
    if(l_handle != NULL)
    {
        Globus_I_XIO_Handle_Destroy(l_handle);
    }

    return res;
}

globus_result_t
globus_xio_register_read(
    globus_xio_handle_t                         handle,
    globus_byte_t *                             buffer,
    globus_size_t                               buffer_length,
    globus_xio_data_descriptor_t                data_desc,
    globus_xio_callback_t                       cb,
    void *                                      user_arg)
{        
}

globus_result_t
globus_xio_register_write(
    globus_xio_handle_t                         handle,
    globus_byte_t *                             buffer,
    globus_size_t                               buffer_length,
    globus_xio_data_descriptor_t                data_desc,
    globus_xio_callback_t                       cb,
    void *                                      user_arg)
{
    /* error echecking */
    if(handle == NULL)
    {
        return GlobusXIOErrorBadParameter("globus_xio_register_read");
    }
    if(buffer == NULL)
    {
        return GlobusXIOErrorBadParameter("globus_xio_register_read");
    }
    if(buffer_length <= 0)
    {
        return GlobusXIOErrorBadParameter("globus_xio_register_read");
    }

    if(xio_handle->state != GLOBUS_XIO_HANDLE_STATE_OPEN)
    {
    }
}

globus_result_t
globus_xio_register_writev(
    globus_xio_handle_t                         handle,
    globus_iovec_t *                            iovec,
    int                                         iovec_count,
    globus_xio_data_descriptor_t                data_desc,
    globus_xio_callback_t                       cb,
    void *                                      user_arg)
{
    globus_i_xio_handle_t *                     xio_handle;
    globus_result_t                             res;

    xio_handle = handle;
    /* error echecking */
    if(handle == NULL)
    {
        return GlobusXIOErrorBadParameter("globus_xio_register_read");
    }
    if(buffer == NULL)
    {
        return GlobusXIOErrorBadParameter("globus_xio_register_read");
    }
    if(buffer_length <= 0)
    {
        return GlobusXIOErrorBadParameter("globus_xio_register_read");
    }

    globus_mutex_lock(xio_handle->mutex);
    {
        if(xio_handle->state != GLOBUS_XIO_HANDLE_STATE_OPEN)
        {
            res = ERROR
        }
        else
        {
            res = globus_l_xio_register_writev(
                    xio_handle,
                    iovec,
                    iovec_count,
                    dd,
                    cb,
                    user_arg);
        }
    }
    globus_mutex_lock(xio_handle->mutex);

    return res;
}


globus_result_t
globus_xio_register_close(
    globus_xio_handle_t                         handle,
    int                                         how,
    globus_xio_callback_t                       cb,
    void *                                      user_arg)
{
}
