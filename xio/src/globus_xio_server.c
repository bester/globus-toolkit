#include "globus_xio.h"
#include "globus_i_xio.h"

/*
 *  note:
 *
 *  Cancel Process
 *  --------------
 *  The only exception to when the accept can finish before the callback occurs
 *  is when a cancel happens.  In the case the driver should stop what it is
 *  doing and finish with a canceled error.  All drivers above it will get
 *  this error and should finish in the same manner, by cleaning up resources
 *  involved with this accpet and the calling finish with the cancel error.
 *  Once the error reaches the top xio will find all drivers were not notified
 *  of the cancel and ask them to destroy their targets.
 *
 *  Errors
 *  ------
 *  There are two times when an error can happens.  The first is in the 
 *  callback.  The process for dealing with this is exctly as a canceled
 *  accept...  The top callback comes back with an error and all successfully
 *  created driver targets are destroyed.
 *
 *  The second case is if the Pass fails.  Once a driver calls pass it must
 *  only return the error code that pass returns.  If that error code is 
 *  GLOBUS_SUCESS then the driver should expect a callback, if it is not 
 *  the driver will not receive the callback for which it registered.  This
 *  rule allows the framework to know that if it receives an error from
 *  pass at the top level that no driver has an outstanding callback.
 *  
 */

#define GlobusIXIOServerDec(free, _in_s)                                \
do                                                                      \
{                                                                       \
    globus_i_xio_server_t *                         _s;                 \
                                                                        \
    _s = (_in_s);                                                       \
    _s->ref--;                                                          \
    if(_s->ref == 0)                                                    \
    {                                                                   \
        /* if the handle ref gets down to zero we must be in one        \
         * of the followninf staes.  The statement is that the handle   \
         * only goes away when it is closed or a open fails             \
         */                                                             \
        globus_assert(_s->state == GLOBUS_XIO_SERVER_STATE_CLOSED);     \
        free = GLOBUS_TRUE;                                             \
    }                                                                   \
    else                                                                \
    {                                                                   \
        free = GLOBUS_FALSE;                                            \
    }                                                                   \
} while (0)

#define GlobusIXIOServerDestroy(_in_s)                                  \
do                                                                      \
{                                                                       \
    globus_i_xio_server_t *                         _s;                 \
                                                                        \
    _s = (_in_s);                                                       \
    globus_mutex_destroy(&_s->mutex);                                   \
    globus_free(_s);                                                    \
} while (0)
/**************************************************************************
 *                       Internal functions
 *                       ------------------
 *************************************************************************/
globus_result_t
globus_l_xio_close_server(
    globus_i_xio_server_t *                 xio_server);

void
globus_i_xio_server_post_accept(
    globus_i_xio_op_t *                     xio_op);

void
globus_i_xio_server_will_block_cb(
    globus_thread_callback_index_t          wb_ndx,
    globus_callback_space_t                 space,
    void *                                  user_args)
{
    globus_i_xio_op_t *                     xio_op;

    xio_op = (globus_i_xio_op_t *) user_args;

    xio_op->restarted = GLOBUS_TRUE;
    xio_op->ref++;

    globus_thread_blocking_callback_pop(&wb_ndx);

    globus_i_xio_server_post_accept(xio_op);
}

/*
 *  this is the only mechanism for delivering a callback to the user 
 */
void
globus_l_xio_server_accept_kickout(
    void *                                  user_arg)
{
    int                                     ctr;
    int                                     wb_ndx;
    globus_i_xio_target_t *                 xio_target = NULL;
    globus_i_xio_server_t *                 xio_server;
    globus_i_xio_op_t *                     xio_op;
    globus_bool_t                           destroy_server = GLOBUS_FALSE;
    GlobusXIOName(globus_l_xio_server_accept_kickout);

    GlobusXIODebugInternalEnter();

    xio_op = (globus_i_xio_op_t *) user_arg;
    xio_server = xio_op->_op_server;

    /* create the structure if successful, otherwise the target is null */
    if(xio_op->cached_res == GLOBUS_SUCCESS)
    {
        xio_target = globus_malloc(sizeof(globus_i_xio_target_t) +
                        (sizeof(globus_i_xio_target_entry_t) * 
                            (xio_op->stack_size - 1)));
        if(xio_target == NULL)
        {
            xio_op->cached_res = GlobusXIOErrorMemory("target");
        }
        xio_target->type = GLOBUS_XIO_TARGET_TYPE_SERVER;
        /* initialize the target structure */
        xio_target->stack_size = xio_op->stack_size;
        for(ctr = 0;  ctr < xio_target->stack_size; ctr++)
        {
            xio_target->entry[ctr].target = 
                    xio_op->entry[ctr].target;
            xio_target->entry[ctr].driver = 
                    xio_server->entry[ctr].driver;
        }
    }
    /* if failed clean up the operation */
    else
    {
        for(ctr = 0;  ctr < xio_op->stack_size; ctr++)
        {
            if(xio_op->entry[ctr].target != NULL)
            {
                /* ignore result code.  user should be more interested in
                    result from callback */
                xio_server->entry[ctr].driver->target_destroy_func(
                    xio_op->entry[ctr].target);
            }
        }
    }

    globus_thread_blocking_space_callback_push(
        globus_i_xio_server_will_block_cb,
        (void *) xio_op,
        xio_op->blocking ? GLOBUS_CALLBACK_GLOBAL_SPACE: xio_server->space,
        &wb_ndx);

    /* call the users callback */
    xio_op->_op_accept_cb(
        xio_server,
        xio_target,
        xio_op->cached_res,
        xio_op->user_arg);
    if(xio_op->restarted)
    {
        globus_mutex_lock(&xio_server->mutex);
        {
            xio_op->ref--;

            if(xio_op->ref == 0)
            {
                /* remove the reference for the target on the server */
                GlobusIXIOServerDec(destroy_server, xio_server);
            }
        }
        globus_mutex_unlock(&xio_server->mutex);
        if(destroy_server)
        {
            GlobusIXIOServerDestroy(xio_server);
        }
        return;
    }

    globus_thread_blocking_callback_pop(&wb_ndx);

    globus_i_xio_server_post_accept(xio_op);

    GlobusXIODebugInternalExit();
}

void
globus_i_xio_server_post_accept(
    globus_i_xio_op_t *                     xio_op)
{
    globus_bool_t                           destroy_server = GLOBUS_FALSE;
    globus_i_xio_server_t *                 xio_server;
    GlobusXIOName(globus_i_xio_server_post_accept);

    GlobusXIODebugInternalEnter();

    xio_server = xio_op->_op_server;
    /* if user called register accept in the callback we will be back from
        the completeing state and into the accepting state */
 
    /* lock up and do some clean up */
    globus_mutex_lock(&xio_server->mutex);
    {
        globus_assert(xio_op->state == GLOBUS_XIO_OP_STATE_FINISH_WAITING);

        xio_server->outstanding_operations--;

        switch(xio_server->state)
        {
            case GLOBUS_XIO_SERVER_STATE_COMPLETING:
                xio_server->op = NULL;
                xio_server->state = GLOBUS_XIO_SERVER_STATE_OPEN;
                break;

            case GLOBUS_XIO_SERVER_STATE_CLOSE_PENDING:
                if(xio_server->outstanding_operations == 0)
                {
                    xio_server->state = GLOBUS_XIO_SERVER_STATE_CLOSING;
                    globus_l_xio_close_server(xio_server);
                }
                break;

            case GLOBUS_XIO_SERVER_STATE_CLOSED:
            case GLOBUS_XIO_SERVER_STATE_CLOSING:
                xio_server->op = NULL;
                break;
                
            case GLOBUS_XIO_SERVER_STATE_ACCEPTING:
                break;

            default:
                globus_assert(0 && "Unexpected state after accept callback");
                break;
        }
        /* decrement reference for the callback if timeout has happened or
            isn't registered this will go to zero */
        xio_op->ref--;
        if(xio_op->ref == 0)
        {
            globus_free(xio_op);
            GlobusIXIOServerDec(destroy_server, xio_server);
        }
    }
    globus_mutex_unlock(&xio_server->mutex);

    if(destroy_server)
    {
        GlobusIXIOServerDestroy(xio_server);
    }

    GlobusXIODebugInternalExit();
}

/*
 *  internal top level accept callback
 */
void
globus_i_xio_server_accept_callback(
    globus_xio_operation_t                  op,
    globus_result_t                         result,
    void *                                  user_arg)
{
    globus_i_xio_server_t *                 xio_server;
    globus_i_xio_op_t *                     xio_op;
    globus_bool_t                           accept = GLOBUS_TRUE;
    GlobusXIOName(globus_i_xio_server_accept_callback);

    GlobusXIODebugInternalEnter();

    xio_op = op;
    xio_server = xio_op->_op_server;

    globus_mutex_lock(&xio_server->mutex);
    {
        /* if in this state it means that the user either has or is about to
           get a cancel callback.  we must delay the delivery of this
           callback until that returns */
        xio_op->cached_res = result;
        if(xio_op->state == GLOBUS_XIO_OP_STATE_TIMEOUT_PENDING)
        {
            accept = GLOBUS_FALSE;
        }
        else
        {
            /* if there is an outstanding accept callback */
            if(xio_op->_op_server_timeout_cb != NULL)
            {
                if(globus_i_xio_timer_unregister_timeout(
                        &globus_l_xio_timeout_timer, xio_op))
                {
                    xio_op->ref--;
                    globus_assert(xio_op->ref > 0);
                }
            }
        }
        xio_op->state = GLOBUS_XIO_OP_STATE_FINISH_WAITING;

        switch(xio_server->state)
        {
            case GLOBUS_XIO_SERVER_STATE_ACCEPTING:
                xio_server->state = GLOBUS_XIO_SERVER_STATE_COMPLETING;
                break;

            /* nothing to do for this case */
            case GLOBUS_XIO_SERVER_STATE_CLOSE_PENDING:
                break;

            default:
                globus_assert(0);
        }
    }
    globus_mutex_unlock(&xio_server->mutex);

    /* we may be delaying the callback until cancel returns */
    if(accept)
    {
        globus_l_xio_server_accept_kickout((void *)xio_op);
    }

    GlobusXIODebugInternalExit();
}

globus_bool_t
globus_l_xio_accept_timeout_callback(
    void *                                  user_arg)
{
    globus_i_xio_op_t *                     xio_op;
    globus_i_xio_server_t *                 xio_server;
    globus_bool_t                           rc;
    globus_bool_t                           cancel;
    globus_bool_t                           accept;
    globus_bool_t                           timeout = GLOBUS_FALSE;
    globus_bool_t                           destroy_server = GLOBUS_FALSE;
    globus_callback_space_t                 space =
        GLOBUS_CALLBACK_GLOBAL_SPACE;
    GlobusXIOName(globus_l_xio_accept_timeout_callback);

    GlobusXIODebugInternalEnter();

    xio_op = (globus_i_xio_op_t *) user_arg;
    xio_server = xio_op->_op_server;

    globus_mutex_lock(&xio_server->mutex);
    {
        switch(xio_op->state)
        {
            /* 
             * this case happens when a serverwas successfully created but 
             * we were unable to unregister the callback and when the first
             * pass fails and we are unable to cancel the timeout callback
             */
            case GLOBUS_XIO_OP_STATE_FINISHED:
            case GLOBUS_XIO_OP_STATE_FINISH_WAITING:

                /* decerement the reference for the timeout callback */
                xio_op->ref--;
                if(xio_op->ref == 0)
                {
                    /* remove the reference for the target on the server */
                    GlobusIXIOServerDec(destroy_server, xio_server);
                }

                /* remove it from the timeout list */
                rc = GLOBUS_TRUE;
                break;

            /* this case happens when we actually want to cancel the operation
                The timeout code should insure that prograess is false if this
                gets called in this state */
            case GLOBUS_XIO_OP_STATE_OPERATING:
                /* it is up to the timeout callback to set this to true */
                rc = GLOBUS_FALSE;
                /* cancel the sucker */
                globus_assert(!xio_op->progress);
                globus_assert(xio_op->_op_server_timeout_cb != NULL);

                if(!xio_op->block_timeout)
                {
                    timeout = GLOBUS_TRUE;
                    /* we don't need to cache the server object in local stack
                       because state insures that it will not go away */
                    /* put in canceling state to delay the accept callback */
                    xio_op->state = GLOBUS_XIO_OP_STATE_TIMEOUT_PENDING;
                }
                break;

            /* fail on any ohter case */
            default:
                globus_assert(0);
        }
    }
    globus_mutex_unlock(&xio_server->mutex);

    /* if in cancel state, verfiy with user that they want to cancel */
    if(timeout)
    {
        cancel = xio_op->_op_server_timeout_cb(
                    xio_server, xio_op->type);
    }
    /* all non time out casses can just return */
    else
    {
        if(destroy_server)
        {
            GlobusIXIOServerDestroy(xio_server);
        }
        goto exit;
    }

    globus_mutex_lock(&xio_server->mutex);
    {
        /* if canceling set the res and we will remove this timer event */
        if(cancel)
        {
            xio_op->cached_res = GlobusXIOErrorTimedout();
            rc = GLOBUS_TRUE;
            xio_op->canceled = GLOBUS_TRUE;
            if(xio_op->cancel_cb)
            {
                xio_op->cancel_cb(xio_op, xio_op->cancel_arg);
            }            
        }

        /* if an accept callback has already arriverd set flag to later
            call accept callback and set rc to remove timed event */
        if(xio_op->state == GLOBUS_XIO_OP_STATE_FINISH_WAITING)
        {
            accept = GLOBUS_TRUE;
            rc = GLOBUS_TRUE;
        }
        /* if no accept is waiting, set state back to operating */
        else
        {
            xio_op->state = GLOBUS_XIO_OP_STATE_OPERATING;
        }

        /* if we are remvoing the timed event */
        if(rc)
        {
            /* decremenet the target reference count and insist that it is
               not zero yet */
            xio_op->ref--;
            globus_assert(xio_op->ref > 0);
        }
    }
    globus_mutex_unlock(&xio_server->mutex);

    /* if the accpet was pending we must call it */
    if(accept)
    {
        if(!xio_op->blocking)
        {
            space = xio_server->space;
        }
        if(space != GLOBUS_CALLBACK_GLOBAL_SPACE)
        {
            /* register a oneshot callback */
            globus_i_xio_register_oneshot(
                NULL,
                globus_l_xio_server_accept_kickout,
                (void *)xio_op,
                space);
        }
        /* in all other cases we can just call callback */
        else
        {
            globus_l_xio_server_accept_kickout((void *)xio_op);
        }
    }

  exit:

    GlobusXIODebugInternalExit();
    return rc;
}



void
globus_l_xio_server_close_kickout(
    void *                                  user_arg)
{
    globus_i_xio_server_t *                 xio_server;
    globus_bool_t                           destroy_server = GLOBUS_FALSE;

    xio_server = (globus_i_xio_server_t *) user_arg;

    xio_server->cb(xio_server, xio_server->user_arg);

    globus_mutex_lock(&xio_server->mutex);
    {
        xio_server->state = GLOBUS_XIO_SERVER_STATE_CLOSED;
        /* dec refrence count then free.  this makes sure we don't
           free while in a user callback */
        GlobusIXIOServerDec(destroy_server, xio_server);
    }
    globus_mutex_unlock(&xio_server->mutex);

    if(destroy_server)
    {
        GlobusIXIOServerDestroy(xio_server);
    }
}
/*
 *  this is called locked
 */
globus_result_t
globus_l_xio_close_server(
    globus_i_xio_server_t *                 xio_server)
{
    int                                     ctr;
    globus_result_t                         res = GLOBUS_SUCCESS;
    globus_result_t                         tmp_res;
    globus_callback_space_t                 space = 
                                                GLOBUS_CALLBACK_GLOBAL_SPACE;
    GlobusXIOName(globus_l_xio_close_server);

    for(ctr = 0; ctr < xio_server->stack_size; ctr++)
    {
        if(xio_server->entry[ctr].driver->server_destroy_func != NULL)
        {
            /* possible to lose a driver res, but you know.. so what? */
            tmp_res = xio_server->entry[ctr].driver->server_destroy_func(
                        xio_server->entry[ctr].server_handle);
            if(tmp_res != GLOBUS_SUCCESS)
            {
                res = GlobusXIOErrorWrapFailed("server_destroy", tmp_res);
            }
        }
    }

    if(!xio_server->blocking)
    {
        space = xio_server->space;
    }
    globus_i_xio_register_oneshot(
        NULL,
        globus_l_xio_server_close_kickout,
        (void *)xio_server,
        space);

    return res;
}

void
globus_l_xio_server_close_cb(
    globus_xio_server_t                     server,
    void *                                  user_arg)
{
    globus_i_xio_blocking_t *               info;

    info = (globus_i_xio_blocking_t *) user_arg;

    globus_mutex_lock(&info->mutex);
    {
        info->done = GLOBUS_TRUE;
        globus_cond_signal(&info->cond);
    }
    globus_mutex_unlock(&info->mutex);
}

void
globus_l_server_accept_cb(
    globus_xio_server_t                     server,
    globus_xio_target_t                     target,
    globus_result_t                         result,
    void *                                  user_arg)
{
    globus_i_xio_blocking_t *               info;

    info = (globus_i_xio_blocking_t *) user_arg;

    globus_mutex_lock(&info->mutex);
    {
        info->res = result;
        info->target = target;
        info->done = GLOBUS_TRUE;
        globus_cond_signal(&info->cond);
    }
    globus_mutex_unlock(&info->mutex);
}


globus_result_t
globus_l_xio_server_register_accept(
    globus_i_xio_op_t *                     xio_op,
    globus_xio_attr_t                       accept_attr)
{
    globus_i_xio_server_t *                 xio_server;
    int                                     ctr;
    globus_result_t                         res = GLOBUS_SUCCESS;
    globus_bool_t                           free_server = GLOBUS_FALSE;
    GlobusXIOName(globus_xio_server_register_accept);

    GlobusXIODebugInternalEnter();

    xio_server = xio_op->_op_server;
    globus_mutex_lock(&xio_server->mutex);
    {
        if(xio_server->state != GLOBUS_XIO_SERVER_STATE_OPEN &&
           xio_server->state != GLOBUS_XIO_SERVER_STATE_COMPLETING)
        {
            res = GlobusXIOErrorInvalidState(xio_server->state);
            goto state_err;
        }

        xio_server->state = GLOBUS_XIO_SERVER_STATE_ACCEPTING;
        xio_server->outstanding_operations++;

        xio_op->type = GLOBUS_XIO_OPERATION_TYPE_ACCEPT;
        xio_op->state = GLOBUS_XIO_OP_STATE_OPERATING;
        xio_op->ref = 1;
        xio_op->cancel_cb = NULL;
        xio_op->canceled = GLOBUS_FALSE;
        xio_op->_op_server_timeout_cb = xio_server->accept_timeout;
        xio_op->ndx = 0;
        xio_op->stack_size = xio_server->stack_size;

        xio_server->op = xio_op;
        /* get all the driver specific attrs and put htem in the 
            correct place */
        for(ctr = 0; ctr < xio_op->stack_size; ctr++)
        {
            GlobusIXIOAttrGetDS(xio_op->entry[ctr].attr,     \
                accept_attr, xio_server->entry[ctr].driver);
        }

        /*i deal with timeout if there is one */
        if(xio_op->_op_server_timeout_cb != NULL)
        {
            xio_op->ref++;
            globus_i_xio_timer_register_timeout(
                &globus_l_xio_timeout_timer,
                xio_op,
                &xio_op->progress,
                globus_l_xio_accept_timeout_callback,
                &xio_server->accept_timeout_period);
        }

        /* add a reference to the server for the op */
        xio_server->ref++;
    }
    globus_mutex_unlock(&xio_server->mutex);

    /* add reference count for the pass.  does not need to be done locked
       since no one has op until it is passed  */
    xio_op->ref++;
    GlobusXIODriverPassAccept(res, xio_op, \
            globus_i_xio_server_accept_callback, NULL);

    if(res != GLOBUS_SUCCESS)
    {
        goto err;
    }

    globus_mutex_lock(&xio_server->mutex);
    {
        xio_op->ref--;
        if(xio_op->ref == 0)
        {
            GlobusIXIOServerDec(free_server, xio_server);
            globus_assert(!free_server);
            globus_free(xio_op);
        }
    }
    globus_mutex_unlock(&xio_server->mutex);

    GlobusXIODebugInternalExit();
    return GLOBUS_SUCCESS;

  err:

    globus_mutex_lock(&xio_server->mutex);
    {
        xio_op->ref--; /* dec for the register */
        globus_assert(xio_op->ref > 0);

        /* set target to invalid type */
        xio_op->state = GLOBUS_XIO_OP_STATE_FINISHED;

        /* if a timeout was registered we must unregister it */
        if(xio_op->_op_server_timeout_cb != NULL)
        {
            if(globus_i_xio_timer_unregister_timeout(
                    &globus_l_xio_timeout_timer, xio_op))
            {
                xio_op->ref--;
                globus_assert(xio_op->ref > 0);
            }
        }
        xio_op->ref--;
        if(xio_op == 0)
        {
            GlobusIXIOServerDec(free_server, xio_server);
            globus_assert(!free_server);
            globus_free(xio_op);
        }
    }
  state_err:
    globus_mutex_unlock(&xio_server->mutex);


    GlobusXIODebugInternalExitWithError();
    return res;
}


/**************************************************************************
 *                         API functions
 *                         -------------
 *************************************************************************/

/*
 *  initialize a server structure
 */
globus_result_t
globus_xio_server_create(
    globus_xio_server_t *                   server,
    globus_xio_attr_t                       server_attr,
    globus_xio_stack_t                      stack)
{
    globus_list_t *                         list;
    globus_i_xio_server_t *                 xio_server = NULL;
    globus_result_t                         res;
    globus_bool_t                           done = GLOBUS_FALSE;
    int                                     ctr;
    int                                     ctr2;
    int                                     tmp_size;
    int                                     stack_size;
    void *                                  ds_attr = NULL;
    GlobusXIOName(globus_xio_server_init);

    GlobusXIODebugEnter();
    if(server == NULL)
    {
        res = GlobusXIOErrorParameter("server");
        goto err;
    }
    if(stack == NULL)
    {
        res = GlobusXIOErrorParameter("stack");
        goto err;
    }
    if(globus_list_empty(stack->driver_stack))
    {
        res = GlobusXIOErrorParameter("stack is empty");
        goto err;
    }

    /* take what the user stack has at the time of registration */
    globus_mutex_lock(&stack->mutex);
    {
        stack_size = globus_list_size(stack->driver_stack);
        tmp_size = sizeof(globus_i_xio_server_t) +
                    (sizeof(globus_i_xio_server_entry_t) * (stack_size - 1));
        xio_server = (globus_i_xio_server_t *) globus_malloc(tmp_size);
        if(xio_server == NULL)
        {
            globus_mutex_lock(&stack->mutex);
            res = GlobusXIOErrorMemory("server");
            goto err;
        }

        memset(xio_server, '\0', tmp_size);
        xio_server->stack_size = globus_list_size(stack->driver_stack);
        xio_server->ref = 1;
        xio_server->state = GLOBUS_XIO_SERVER_STATE_OPEN;
        xio_server->space = GLOBUS_CALLBACK_GLOBAL_SPACE;
        globus_mutex_init(&xio_server->mutex, NULL);
        xio_server->accept_timeout = NULL;

        /* timeout handling */
        if(server_attr != NULL)
        {
            xio_server->accept_timeout = server_attr->accept_timeout_cb;
            xio_server->space = server_attr->space;
        }
        /* walk through the stack and add each entry to the array */
        ctr = 0;
        for(list = stack->driver_stack;
            !globus_list_empty(list) && !done;
            list = globus_list_rest(list))
        {
            xio_server->entry[ctr].driver = (globus_xio_driver_t)
                globus_list_first(list);

            /* no sense bothering if attr is NULL */
            if(server_attr != NULL)
            {
                GlobusIXIOAttrGetDS(ds_attr, server_attr,               \
                    xio_server->entry[ctr].driver);
            }
            /* call the driver init function */
            xio_server->entry[ctr].server_handle = NULL;
            if(xio_server->entry[ctr].driver->server_init_func != NULL)
            {
                res = xio_server->entry[ctr].driver->server_init_func(
                        &xio_server->entry[ctr].server_handle,
                        ds_attr);
                if(res != GLOBUS_SUCCESS)
                {
                    /* clean up all the initialized servers */
                    for(ctr2 = 0; ctr2 < ctr; ctr2++)
                    {
                        xio_server->entry[ctr].driver->server_destroy_func(
                            xio_server->entry[ctr].server_handle);
                    }
                    done = GLOBUS_TRUE;
                }
            }
            ctr++;
        }
    }
    globus_mutex_unlock(&stack->mutex);

    if(res != GLOBUS_SUCCESS)
    {
        goto err;
    }
    *server = xio_server;

    GlobusXIODebugExit();
    return GLOBUS_SUCCESS;

  err:
    *server = NULL;

    GlobusXIODebugExitWithError();
    return res;
}

/*
 *
 */
globus_result_t
globus_xio_server_cntl(
    globus_xio_server_t                     server,
    globus_xio_driver_t                     driver,
    int                                     cmd,
    ...)
{
    globus_bool_t                           found = GLOBUS_FALSE;
    int                                     ctr;
    globus_result_t                         res = GLOBUS_SUCCESS;
    va_list                                 ap;
    globus_i_xio_server_t *                 xio_server;
    GlobusXIOName(globus_xio_server_cntl);

    GlobusXIODebugEnter();
    if(server == NULL)
    {
        res = GlobusXIOErrorParameter("server");
        goto err;
    }

    xio_server = (globus_i_xio_server_t *) server;

    globus_mutex_lock(&xio_server->mutex);
    {
        if(driver == NULL)
        {
            /* do general things */
        }
        else
        {
            for(ctr = 0; 
                !found && ctr < xio_server->stack_size; 
                ctr++)
            {
                if(xio_server->entry[ctr].driver == driver)
                {
                    found = GLOBUS_TRUE;

                    va_start(ap, cmd);
                    res = xio_server->entry[ctr].driver->server_cntl_func(
                            xio_server->entry[ctr].server_handle,
                            cmd,
                            ap);
                    va_end(ap);
                }
            }
            if(!found)
            {
                res = GlobusXIOErrorInvalidDriver("not found");
            }
        }
    }
    globus_mutex_unlock(&xio_server->mutex);

    if(res != GLOBUS_SUCCESS)
    {
        goto err;
    }

    GlobusXIODebugExit();
    return GLOBUS_SUCCESS;

  err:

    GlobusXIODebugExitWithError();
    return res;
}

/*
 *  register an accept
 */
globus_result_t
globus_xio_server_register_accept(
    globus_xio_server_t                     server,
    globus_xio_attr_t                       accept_attr,
    globus_xio_accept_callback_t            cb,
    void *                                  user_arg)
{
    int                                     tmp_size;
    globus_result_t                         res = GLOBUS_SUCCESS;
    globus_i_xio_server_t *                 xio_server;
    globus_i_xio_op_t *                     xio_op;
    GlobusXIOName(globus_xio_server_register_accept);

    GlobusXIODebugEnter();
    if(server == NULL)
    {
        return GlobusXIOErrorParameter("server");
    }
    
    xio_server = (globus_i_xio_server_t *) server;

    tmp_size = sizeof(globus_i_xio_op_t) + 
                (sizeof(globus_i_xio_op_entry_t) * 
                    (xio_server->stack_size - 1));
    xio_op = (globus_i_xio_op_t *) globus_malloc(tmp_size);

    if(xio_op == NULL)
    {
        globus_mutex_unlock(&xio_server->mutex);
        res = GlobusXIOErrorMemory("operation");
        goto err;
    }
    memset(xio_op, '\0', tmp_size);

    xio_op->_op_accept_cb = cb;
    xio_op->user_arg = user_arg;
    xio_op->_op_server = xio_server;
    xio_op->stack_size = xio_server->stack_size;

    res = globus_l_xio_server_register_accept(xio_op, accept_attr);

    if(res != GLOBUS_SUCCESS)
    {
        goto err;
    }

    GlobusXIODebugExit();
    return GLOBUS_SUCCESS;

  err:

    GlobusXIODebugExitWithError();
    return res;
}

/*
 *  cancel the server
 */
globus_result_t
globus_xio_server_cancel_accept(
    globus_xio_server_t                     server)
{
    globus_result_t                         res = GLOBUS_SUCCESS;
    globus_i_xio_server_t *                 xio_server;
    GlobusXIOName(globus_xio_server_cancel_accept);

    GlobusXIODebugEnter();
    xio_server = (globus_i_xio_server_t *)  server;

    globus_mutex_lock(&xio_server->mutex);
    {
        if(xio_server->state != GLOBUS_XIO_SERVER_STATE_ACCEPTING &&
           xio_server->state != GLOBUS_XIO_SERVER_STATE_COMPLETING)
        {
            res = GlobusXIOErrorInvalidState(xio_server->state);
        }
        else if(xio_server->op->canceled)
        {
            res = GlobusXIOErrorCanceled();
        }
        else
        {
            /* the callback is called locked.  within it the driver is
                allowed limited functionality.  by calling this locked
                can more efficiently pass the operation down the stack */
            xio_server->op->canceled = GLOBUS_TRUE;
            if(xio_server->op->cancel_cb)
            {
                xio_server->op->cancel_cb(xio_server->op,
                    xio_server->op->cancel_arg);
            }            
        }
    }
    globus_mutex_unlock(&xio_server->mutex);

    if(res != GLOBUS_SUCCESS)
    {
        goto err;
    }

    GlobusXIODebugExit();
    return GLOBUS_SUCCESS;

  err:

    GlobusXIODebugExitWithError();
    return res;
}

globus_result_t
globus_xio_server_accept(
    globus_xio_target_t *                   out_target,
    globus_xio_server_t                     server,
    globus_xio_attr_t                       accept_attr)
{
    int                                     tmp_size;
    globus_result_t                         res = GLOBUS_SUCCESS;
    globus_i_xio_server_t *                 xio_server;
    globus_i_xio_op_t *                     xio_op;
    globus_i_xio_blocking_t *               info;
    GlobusXIOName(globus_xio_server_accept);
    
    GlobusXIODebugEnter();
    if(server == NULL)
    {
        return GlobusXIOErrorParameter("server");
    }
    
    xio_server = (globus_i_xio_server_t *) server;

    tmp_size = sizeof(globus_i_xio_op_t) +
                (sizeof(globus_i_xio_op_entry_t) *
                    (xio_server->stack_size - 1));
    xio_op = (globus_i_xio_op_t *) globus_malloc(tmp_size);
    
    if(xio_op == NULL)
    {
        globus_mutex_unlock(&xio_server->mutex);
        res = GlobusXIOErrorMemory("operation");
        goto err;
    }
    memset(xio_op, '\0', tmp_size);
    
    info = globus_i_xio_blocking_alloc();
    if(info == NULL)
    {
        res = GlobusXIOErrorMemory("internal strucature");
        goto info_alloc_err;
    }

    xio_op->_op_accept_cb = globus_l_server_accept_cb;
    xio_op->user_arg = info;
    xio_op->_op_server = xio_server;
    xio_op->stack_size = xio_server->stack_size;
    xio_op->blocking = GLOBUS_TRUE;
   
    globus_mutex_lock(&info->mutex);
    {
        res = globus_l_xio_server_register_accept(xio_op, accept_attr);
        if(res != GLOBUS_SUCCESS)
        {
            goto register_error;
        }

        while(!info->done)
        {
            globus_cond_wait(&info->cond, &info->mutex);
        }
    }
    globus_mutex_unlock(&info->mutex);

    if(info->res != GLOBUS_SUCCESS)
    {
        res = info->res;
        goto register_error;
    }

    *out_target = info->target;
    globus_i_xio_blocking_destroy(info);

    GlobusXIODebugExit();
    return GLOBUS_SUCCESS;

  register_error:
    globus_mutex_unlock(&info->mutex);
    globus_i_xio_blocking_destroy(info);
    
  info_alloc_err:
    globus_free(xio_op);

  err:
    *out_target = NULL;
    GlobusXIODebugExitWithError();
    return res;
}

/*
 *  destroy the server
 */
globus_result_t
globus_xio_server_register_close(
    globus_xio_server_t                     server,
    globus_xio_server_callback_t            cb,
    void *                                  user_arg)
{
    globus_i_xio_server_t *                 xio_server;
    globus_result_t                         res = GLOBUS_SUCCESS;
    GlobusXIOName(globus_xio_server_register_close);

    GlobusXIODebugEnter();
    if(server == NULL)
    {
        res = GlobusXIOErrorParameter("server");
        goto err;
    }

    xio_server = (globus_i_xio_server_t *) server;

    globus_mutex_lock(&xio_server->mutex);
    {
        if(xio_server->state == GLOBUS_XIO_SERVER_STATE_CLOSE_PENDING ||
           xio_server->state == GLOBUS_XIO_SERVER_STATE_CLOSING ||
           xio_server->state == GLOBUS_XIO_SERVER_STATE_CLOSED)
        {
            res = GlobusXIOErrorInvalidState(xio_server->state);
        }
        else
        {
           /* the callback is called locked.  within it the driver is
                allowed limited functionality.  by calling this locked
                can more efficiently pass the operation down the stack */
            if(xio_server->op != NULL)
            {
                xio_server->op->canceled = GLOBUS_TRUE;
                if(xio_server->op->cancel_cb)
                {
                    xio_server->op->cancel_cb(xio_server->op,
                        xio_server->op->cancel_arg);
                }            
            }

            xio_server->cb = cb;
            xio_server->user_arg = user_arg;
            switch(xio_server->state)
            {
                case GLOBUS_XIO_SERVER_STATE_ACCEPTING:
                case GLOBUS_XIO_SERVER_STATE_COMPLETING:
                    xio_server->state = GLOBUS_XIO_SERVER_STATE_CLOSE_PENDING;
                    break;

                case GLOBUS_XIO_SERVER_STATE_OPEN:
                    xio_server->state = GLOBUS_XIO_SERVER_STATE_CLOSING;
                    globus_l_xio_close_server(xio_server);
                    break;

                default:
                    globus_assert(0);
            }
        }
    }
    globus_mutex_unlock(&xio_server->mutex);

    if(res != GLOBUS_SUCCESS)
    {
        goto err;
    }

    GlobusXIODebugExit();
    return GLOBUS_SUCCESS;

  err:

    GlobusXIODebugExitWithError();
    return res;
}

/*  
 *  destroy the server
 */ 
globus_result_t
globus_xio_server_close(
    globus_xio_server_t                     server)
{
    globus_i_xio_server_t *                 xio_server;
    globus_result_t                         res = GLOBUS_SUCCESS;
    globus_i_xio_blocking_t *               info;
    GlobusXIOName(globus_xio_server_close);

    GlobusXIODebugEnter();
    if(server == NULL) 
    {
        res = GlobusXIOErrorParameter("server");
        goto err;
    }

    info = globus_i_xio_blocking_alloc();
    if(info == GLOBUS_NULL)
    {
        res = GlobusXIOErrorMemory("internal");
        goto err;
    }
    
    xio_server = (globus_i_xio_server_t *) server;

    globus_mutex_lock(&info->mutex);
    {
        xio_server->blocking = GLOBUS_TRUE;
        res = globus_xio_server_register_close(xio_server, 
                globus_l_xio_server_close_cb, info);

        while(!info->done)
        {
            globus_cond_wait(&info->cond, &info->mutex);
        }
    }
    globus_mutex_unlock(&info->mutex);

    if(res != GLOBUS_SUCCESS)
    {
        globus_i_xio_blocking_destroy(info);
        goto err;
    }

    GlobusXIODebugExit();
    return GLOBUS_SUCCESS;

  err:
    GlobusXIODebugExitWithError();
    return res;
}



globus_result_t
globus_xio_target_destroy(
    globus_xio_target_t                     target)
{   
    globus_i_xio_target_t *                 xio_target;
    globus_result_t                         res = GLOBUS_SUCCESS;
    globus_result_t                         tmp_res;
    int                                     ctr;
    GlobusXIOName(globus_xio_target_destroy);

    GlobusXIODebugEnter();
    /*
     *  parameter checking 
     */
    if(target == NULL)
    {
        res = GlobusXIOErrorParameter("target");
        goto err;
    }
    xio_target = (globus_i_xio_target_t *) target;
    if(xio_target->type != GLOBUS_XIO_TARGET_TYPE_SERVER &&
       xio_target->type != GLOBUS_XIO_TARGET_TYPE_CLIENT)
    {
        res = GlobusXIOErrorInvalidState(xio_target->type);
        goto err;
    }

    for(ctr = 0; ctr < xio_target->stack_size; ctr++)
    {
        if(xio_target->entry[ctr].driver->target_destroy_func != NULL)
        {
            tmp_res = xio_target->entry[ctr].driver->target_destroy_func(
                xio_target->entry[ctr].target);
            /* this will effectively report the last error detected */
            if(tmp_res != GLOBUS_SUCCESS)
            {
                res = tmp_res;
            }
        }
    }
    globus_free((void*)xio_target);

    if(res != GLOBUS_SUCCESS)
    {
        goto err;
    }

    GlobusXIODebugExit();
    return GLOBUS_SUCCESS;

  err:

    GlobusXIODebugExitWithError();
    return res;
}

/*
 *  verify the driver is in this stack.
 *  call target control on the driver
 *
 *  if not driver specific there is nothing to do (yet)
 */
globus_result_t
globus_xio_target_cntl(
    globus_xio_target_t                     target,
    globus_xio_driver_t                     driver,
    int                                     cmd,
    ...)
{
    globus_i_xio_target_t *                 xio_target;
    int                                     ctr;
    globus_result_t                         res;
    va_list                                 ap;
    GlobusXIOName(globus_xio_target_cntl);

    GlobusXIODebugEnter();
    if(target == NULL)
    {
        res = GlobusXIOErrorParameter("target");
        goto err;
    }
    if(cmd < 0)
    {
        res = GlobusXIOErrorParameter("cmd");
        goto err;
    }
    
#   ifdef HAVE_STDARG_H
    {
        va_start(ap, cmd);
    }
#   else
    {
        va_start(ap);
    }
#   endif

    xio_target = (globus_i_xio_target_t *) target;

    if(driver != NULL)
    {
        for(ctr = 0; ctr < xio_target->stack_size; ctr++)
        {
            if(xio_target->entry[ctr].driver == driver)
            {
                res = driver->target_cntl_func(
                    xio_target->entry[ctr].target,
                    cmd,
                    ap);
                va_end(ap);

                goto err;
            }
        }
        return GlobusXIOErrorInvalidDriver("not found");
    }
    else
    {
        /* do general target modifications */
    }

    va_end(ap);

    GlobusXIODebugExit();
    return GLOBUS_SUCCESS;

  err:
    GlobusXIODebugExitWithError();
    return res;
}

/*
 *
 */
globus_result_t
globus_xio_target_init(
    globus_xio_target_t *                   target,
    globus_xio_attr_t                       target_attr,
    const char *                            contact_string,
    globus_xio_stack_t                      stack)
{
    globus_result_t                         res;
    globus_i_xio_target_t *                 xio_target;
    int                                     stack_size;
    int                                     ctr;
    int                                     ndx;
    globus_list_t *                         list;
    void *                                  driver_attr;
    GlobusXIOName(globus_xio_target_init);

    GlobusXIODebugEnter();
    /*
     *  parameter checking 
     */
    if(target == NULL)
    {
        res = GlobusXIOErrorParameter("target");
        goto err;
    }
    if(contact_string == NULL)
    {
        res = GlobusXIOErrorParameter("contact_string");
        goto err;
    }
    if(stack == NULL)
    {
        res = GlobusXIOErrorParameter("stack");
        goto err;
    }

    stack_size = globus_list_size(stack->driver_stack);
    if(stack_size == 0)
    {
        res = GlobusXIOErrorParameter("stack_size");
        goto err;
    }

    /* TODO: check stack, make sure it meets requirements */
    xio_target = (globus_i_xio_target_t *)
                    globus_malloc(sizeof(globus_i_xio_target_t) +
                        (sizeof(globus_i_xio_target_entry_t) * 
                            (stack_size - 1)));
    if(xio_target == NULL)
    {
        res = GlobusXIOErrorMemory("target");
        goto err;
    }
    memset(xio_target, '\0', sizeof(globus_i_xio_target_t) +
        (sizeof(globus_i_xio_target_entry_t) * (stack_size - 1)));

    xio_target->type = GLOBUS_XIO_TARGET_TYPE_CLIENT;
    /* initialize what we need of the target structure */
    xio_target->stack_size = stack_size;

    driver_attr = NULL;
    ndx = 0;
    for(list = stack->driver_stack;
    !globus_list_empty(list);
        list = globus_list_rest(list))
    {
        xio_target->entry[ndx].driver = (globus_xio_driver_t) 
                                        globus_list_first(list);

        if(xio_target->entry[ndx].driver->target_init_func != NULL)
        {
            /* pull driver specific info out of target attr */
            if(target_attr != NULL)
            {
                GlobusIXIOAttrGetDS(driver_attr, target_attr,               \
                    xio_target->entry[ndx].driver);
            }

            res = xio_target->entry[ndx].driver->target_init_func(
                    &xio_target->entry[ndx].target,
                    driver_attr,
                    contact_string);
            if(res != GLOBUS_SUCCESS)
            {
                /* loop back through and destroy all inited targets */
                for(ctr = 0; ctr < ndx; ctr++)
                {
                    /* ignore the result, but it must be passed */
                    xio_target->entry[ndx].driver->target_destroy_func(
                        xio_target->entry[ndx].target);
                }
                globus_free(xio_target);
                goto err;
            }
        }

        ndx++;
    }
    /* hell has broken loose if these are not equal */
    globus_assert(ndx == stack_size);

    *target = (globus_xio_target_t) xio_target;

    GlobusXIODebugExit();
    return GLOBUS_SUCCESS;

  err:

    GlobusXIODebugExitWithError();
    return res;
}
