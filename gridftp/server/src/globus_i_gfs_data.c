
#include "globus_i_gridftp_server.h"
/* provides local_extensions */
#include "extensions.h"

static globus_gfs_storage_iface_t *     globus_l_gfs_dsi = NULL;
globus_extension_registry_t             globus_i_gfs_dsi_registry;
globus_extension_handle_t               globus_i_gfs_active_dsi_handle;

void
globus_i_gfs_monitor_init(
    globus_i_gfs_monitor_t *            monitor)
{
    globus_mutex_init(&monitor->mutex, NULL);
    globus_cond_init(&monitor->cond, NULL);
    monitor->done = GLOBUS_FALSE;
}

void
globus_i_gfs_monitor_wait(
    globus_i_gfs_monitor_t *            monitor)
{
    globus_mutex_lock(&monitor->mutex);
    {
        while(!monitor->done)
        {
            globus_cond_wait(&monitor->cond, &monitor->mutex);
        }
    }
    globus_mutex_unlock(&monitor->mutex);
}

void
globus_i_gfs_monitor_destroy(
    globus_i_gfs_monitor_t *            monitor)
{
    globus_mutex_destroy(&monitor->mutex);
    globus_cond_destroy(&monitor->cond);
}

void
globus_i_gfs_monitor_signal(
    globus_i_gfs_monitor_t *            monitor)
{
    globus_mutex_lock(&monitor->mutex);
    {
        monitor->done = GLOBUS_TRUE;
        globus_cond_signal(&monitor->cond);
    }
    globus_mutex_unlock(&monitor->mutex);
}

void
globus_i_gfs_data_init()
{
    char *                              dsi_name;
    
    dsi_name = globus_i_gfs_config_string("dsi");
    
    globus_extension_register_builtins(local_extensions);
    
    globus_l_gfs_dsi = (globus_gfs_storage_iface_t *) globus_extension_lookup(
        &globus_i_gfs_active_dsi_handle, GLOBUS_GFS_DSI_REGISTRY, dsi_name);
    if(!globus_l_gfs_dsi)
    {
        char                            buf[256];
        
        snprintf(buf, 256, "globus_gridftp_server_%s", dsi_name);
        buf[255] = 0;
    
        if(globus_extension_activate(buf) != GLOBUS_SUCCESS)
        {
            globus_i_gfs_log_message(
                GLOBUS_I_GFS_LOG_ERR, "Unable to activate %s\n", buf);
            exit(1);
        }
        
        globus_l_gfs_dsi = (globus_gfs_storage_iface_t *) globus_extension_lookup(
            &globus_i_gfs_active_dsi_handle, GLOBUS_GFS_DSI_REGISTRY, dsi_name);
    }
    
    if(!globus_l_gfs_dsi)
    {
        globus_i_gfs_log_message(
           GLOBUS_I_GFS_LOG_ERR, "Couldn't find the %s extension\n", dsi_name);
        exit(1);
    }
}

typedef enum
{
    GLOBUS_L_GFS_DATA_REQUESTING,
    GLOBUS_L_GFS_DATA_PENDING,
    GLOBUS_L_GFS_DATA_COMPLETE,
    GLOBUS_L_GFS_DATA_ERROR,
    GLOBUS_L_GFS_DATA_ERROR_COMPLETE
} globus_l_gfs_data_state_t;

typedef struct
{
    globus_gfs_operation_t   op;
    
    union
    {
        globus_gridftp_server_write_cb_t write;
        globus_gridftp_server_read_cb_t  read;
    } callback;
    void *                              user_arg;
} globus_l_gfs_data_bounce_t;

typedef struct globus_l_gfs_data_operation_s
{
    globus_i_gfs_server_instance_t *    instance;
    globus_l_gfs_data_state_t           state;
    globus_mutex_t                      lock;
    globus_i_gfs_data_handle_t *        data_handle;
    globus_bool_t                       sending;
    
    int                                 id;
    globus_gfs_ipc_handle_t             ipc_handle;
    
    uid_t                               uid;
    /* transfer stuff */
    globus_range_list_t                 range_list;
    globus_off_t                        partial_offset;
    globus_off_t                        partial_length;
    const char *                        list_type;

    globus_off_t                        max_offset;
    globus_off_t                        recvd_bytes;
    globus_range_list_t                 recvd_ranges;
    
    int                                 nstreams;
    int                                 stripe_count;
    int *                               eof_count;
    int                                 node_count;
    int                                 node_ndx;
    int                                 write_stripe;
    
    int                                 write_delta;
    int                                 stripe_chunk;
    globus_range_list_t                 stripe_range_list;
    
    /* command stuff */
    globus_gfs_command_type_t           command;
    char *                              pathname;
    globus_off_t                        cksm_offset;
    globus_off_t                        cksm_length;
    char *                              cksm_alg;
    char *                              cksm_response;
    mode_t                              chmod_mode;
    char *                              rnfr_pathname;    
    /**/
    
    globus_i_gfs_data_callback_t        callback;
    globus_i_gfs_data_event_callback_t  event_callback;
    void *                              user_arg;
} globus_l_gfs_data_operation_t;

globus_i_gfs_data_attr_t                globus_i_gfs_data_attr_defaults = 
{
    GLOBUS_FALSE,                       /* ipv6 */
    1,                                  /* nstreams */
    'S',                                /* mode */
    'A',                                /* type */
    0,                                  /* tcp_bufsize (sysdefault) */
    256 * 1024                          /* blocksize */
};

static
globus_result_t
globus_l_gfs_data_operation_init(
    globus_l_gfs_data_operation_t **    u_op)
{
    globus_l_gfs_data_operation_t *     op;
    globus_result_t                     result;
    GlobusGFSName(globus_l_gfs_operation_init);
    
    op = (globus_l_gfs_data_operation_t *) 
        globus_calloc(1, sizeof(globus_l_gfs_data_operation_t));
    if(!op)
    {
        result = GlobusGFSErrorMemory("op");
        goto error_alloc;
    }
    
    globus_mutex_init(&op->lock, GLOBUS_NULL);
    op->recvd_ranges = GLOBUS_NULL;
    globus_range_list_init(&op->recvd_ranges);
    globus_range_list_init(&op->stripe_range_list);
    op->recvd_bytes = 0;
    op->max_offset = -1;
    
    *u_op = op;
    return GLOBUS_SUCCESS;
    
error_alloc:
    return result;
}

static
void
globus_l_gfs_data_operation_destroy(
    globus_l_gfs_data_operation_t *     op)
{
/*
    if(op->if(op->recvd_ranges)
    {
        globus_range_list_destroy(op->recvd_ranges);
    }
    globus_mutex_destroy(&op->lock);
    globus_free(op);
*/
}

    
globus_result_t
globus_i_gfs_data_request_stat(
    globus_gfs_ipc_handle_t             ipc_handle,
    int                                 session_id,
    int                                 id,
    globus_gfs_stat_info_t *            stat_info,
    globus_i_gfs_data_callback_t        cb,
    void *                              user_arg)
{
    globus_l_gfs_data_operation_t *     op;
    globus_result_t                     result;
    GlobusGFSName(globus_i_gfs_data_stat_request);

    result = globus_l_gfs_data_operation_init(&op);
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusGFSErrorWrapFailed(
            "globus_l_gfs_data_operation_init", result);
        goto error_op;
    }
    
    op->ipc_handle = ipc_handle;
    op->id = id;
    op->uid = getuid();
    
    op->state = GLOBUS_L_GFS_DATA_REQUESTING;
    op->callback = cb;
    op->user_arg = user_arg;
    
    result = globus_l_gfs_dsi->stat_func(op, stat_info, (void *) session_id);
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusGFSErrorWrapFailed("hook", result);
        goto error_hook;
    }
    
    globus_mutex_lock(&op->lock);
    {
        if(op->state == GLOBUS_L_GFS_DATA_REQUESTING)
        {
            op->state = GLOBUS_L_GFS_DATA_PENDING;
        }
    }
    globus_mutex_unlock(&op->lock);
    
    return GLOBUS_SUCCESS;

error_hook:
    globus_l_gfs_data_operation_destroy(op);
    
error_op:
    return result;
}

typedef struct
{
    globus_l_gfs_data_operation_t *     op;
    globus_object_t *                   error;
    int                                 stat_count;
    globus_gfs_stat_t *                 stat_array;
} globus_l_gfs_data_stat_bounce_t;

static
void
globus_l_gfs_data_stat_kickout(
    void *                              user_arg)
{
    globus_l_gfs_data_stat_bounce_t * bounce_info;
    globus_gfs_ipc_reply_t *            reply;   

    bounce_info = (globus_l_gfs_data_stat_bounce_t *) user_arg;

    reply = (globus_gfs_ipc_reply_t *) 
        globus_calloc(1, sizeof(globus_gfs_ipc_reply_t));
 
    reply->type = GLOBUS_GFS_OP_STAT;
    reply->id = bounce_info->op->id;
    reply->result = bounce_info->error ? 
        globus_error_put(bounce_info->error) : GLOBUS_SUCCESS;
    reply->info.stat.stat_array =  bounce_info->stat_array;
    reply->info.stat.stat_count =  bounce_info->stat_count;

    if(bounce_info->op->callback != NULL)
    {
        bounce_info->op->callback(
            reply,
            bounce_info->op->user_arg);
    }
    else
    {    
        globus_gfs_ipc_reply_finished(
            bounce_info->op->ipc_handle,
            reply);
    }
                
    globus_l_gfs_data_operation_destroy(bounce_info->op);
    globus_free(bounce_info);
}

void
globus_gridftp_server_operation_finished(
    globus_gfs_operation_t              op,
    globus_result_t                     result,
    globus_gfs_finished_info_t *        finished_info)
{
    finished_info->id = op->id;
    finished_info->result = result;
    
    if(op->callback != NULL)
    {
        op->callback(
            finished_info,
            op->user_arg);        
    }
    else
    {
        globus_gfs_ipc_reply_finished(
            op->ipc_handle,
            finished_info);
    }
    
    return;  
}
    
void
globus_gridftp_server_operation_event(
    globus_gfs_operation_t              op,
    globus_result_t                     result,
    globus_gfs_event_info_t *           event_info)
{
    event_info->id = op->id;

    if(op->event_callback != NULL)
    {
        op->event_callback(
            event_info,
            op->user_arg);        
    }
    else
    {
        globus_gfs_ipc_reply_event(
            op->ipc_handle,
            event_info);
    }

    return;
}


void
globus_gridftp_server_finished_command(
    globus_gfs_operation_t              op,
    globus_result_t                     result,
    char *                              command_data)
{
    GlobusGFSName(globus_gridftp_server_finished_command);
    
    globus_mutex_lock(&op->lock);
    {
        op->state = GLOBUS_L_GFS_DATA_COMPLETE;
    }
    globus_mutex_unlock(&op->lock);

    switch(op->command)
    {
      case GLOBUS_GFS_CMD_CKSM:
        op->cksm_response = globus_libc_strdup(command_data);
        break;      
      case GLOBUS_GFS_CMD_MKD:
      case GLOBUS_GFS_CMD_RMD:
      case GLOBUS_GFS_CMD_DELE:
      case GLOBUS_GFS_CMD_RNTO:
      case GLOBUS_GFS_CMD_SITE_CHMOD:
      default:
        break;
      
    }

    {
    globus_gfs_ipc_reply_t *            reply;   
    reply = (globus_gfs_ipc_reply_t *) 
        globus_calloc(1, sizeof(globus_gfs_ipc_reply_t));
 
    reply->type = GLOBUS_GFS_OP_COMMAND;
    reply->id = op->id;
    reply->result = result;
    reply->info.command.command = op->command;
    reply->info.command.checksum = op->cksm_response;
    reply->info.command.created_dir = op->pathname;

    if(op->callback != NULL)
    {
        op->callback(
            reply,
            op->user_arg);        
    }
    else
    {
        globus_gfs_ipc_reply_finished(
            op->ipc_handle,
            reply);
    }
    }
/*
    op->command_callback(
        op->instance,
        result,
        op->cmd_attr,
        op->user_arg);
*/        
    globus_l_gfs_data_operation_destroy(op);
    
    return;
}

void
globus_gridftp_server_finished_stat(
    globus_gfs_operation_t   op,
    globus_result_t                     result,
    globus_gfs_stat_t *      stat_array,
    int                                 stat_count)
{
    globus_bool_t                       delay;
    globus_l_gfs_data_stat_bounce_t * bounce_info;
    globus_gfs_stat_t *      stat_copy;
    GlobusGFSName(globus_gridftp_server_finished_stat);
    
    globus_mutex_lock(&op->lock);
    {
        if(op->state == GLOBUS_L_GFS_DATA_REQUESTING)
        {
            delay = GLOBUS_TRUE;
        }
        else
        {
            delay = GLOBUS_FALSE;
        }
        
        op->state = GLOBUS_L_GFS_DATA_COMPLETE;
    }
    globus_mutex_unlock(&op->lock);
    
    stat_copy = (globus_gfs_stat_t *)
        globus_malloc(sizeof(globus_gfs_stat_t) * stat_count);
    memcpy(
        stat_copy,
        stat_array,
        sizeof(globus_gfs_stat_t) * stat_count);

    if(delay)
    {
        bounce_info = (globus_l_gfs_data_stat_bounce_t *)
            globus_malloc(sizeof(globus_l_gfs_data_stat_bounce_t));
        if(!bounce_info)
        {
            result = GlobusGFSErrorMemory("bounce_info");
            goto error_alloc;
        }
        
        bounce_info->op = op;
        bounce_info->error = result == GLOBUS_SUCCESS 
            ? GLOBUS_NULL : globus_error_get(result);
        bounce_info->stat_count = stat_count;
        bounce_info->stat_array = stat_copy;
        result = globus_callback_register_oneshot(
            GLOBUS_NULL,
            GLOBUS_NULL,
            globus_l_gfs_data_stat_kickout,
            bounce_info);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusGFSErrorWrapFailed(
                "globus_callback_register_oneshot", result);
            goto error_oneshot;
        }
    }
    else
    {
        globus_gfs_ipc_reply_t *            reply;   
        reply = (globus_gfs_ipc_reply_t *) 
            globus_calloc(1, sizeof(globus_gfs_ipc_reply_t));
     
        reply->type = GLOBUS_GFS_OP_STAT;
        reply->id = op->id;
        reply->result = result;
        reply->info.stat.stat_array = stat_copy;
        reply->info.stat.stat_count = stat_count;
        reply->info.stat.uid = op->uid;
            
        if(op->callback != NULL)
        {
            op->callback(
                reply,
                op->user_arg);
        }
        else
        {
            globus_gfs_ipc_reply_finished(
                op->ipc_handle,
                reply);
        }
            
        globus_l_gfs_data_operation_destroy(op);
    }
    
    return;

error_oneshot:
error_alloc:
    globus_panic(
        GLOBUS_NULL,
        result,
        "[%s:%d] Unrecoverable error",
        _gfs_name,
        __LINE__);
}

    
globus_result_t
globus_i_gfs_data_request_command(
    globus_gfs_ipc_handle_t             ipc_handle,
    int                                 session_id,
    int                                 id,
    globus_gfs_command_info_t *         cmd_info,
    globus_i_gfs_data_callback_t        cb,
    void *                              user_arg)
{
    globus_l_gfs_data_operation_t *     op;
    globus_result_t                     result;
    GlobusGFSName(globus_i_gfs_data_command_request);

    result = globus_l_gfs_data_operation_init(&op);
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusGFSErrorWrapFailed(
            "globus_l_gfs_data_operation_init", result);
        goto error_op;
    }
    op->ipc_handle = ipc_handle;
    op->id = id;
    op->state = GLOBUS_L_GFS_DATA_REQUESTING;
    op->command = cmd_info->command;
    op->pathname = cmd_info->pathname;
    op->callback = cb;
    op->user_arg = user_arg;
    
    result = globus_l_gfs_dsi->command_func(op, cmd_info, (void *) session_id);
      
    if(result != GLOBUS_SUCCESS)
    {
        goto error_command;
    }    
    globus_mutex_lock(&op->lock);
    {
        if(op->state == GLOBUS_L_GFS_DATA_REQUESTING)
        {
            op->state = GLOBUS_L_GFS_DATA_PENDING;
        }
    }
    globus_mutex_unlock(&op->lock);
    
    return GLOBUS_SUCCESS;

error_command:
    globus_l_gfs_data_operation_destroy(op);
    
error_op:
    return result;
}

static
globus_result_t
globus_l_gfs_data_handle_init(
    globus_i_gfs_data_handle_t **       u_handle,
    globus_gfs_data_info_t *           data_info)
{
    globus_i_gfs_data_handle_t *        handle;
    globus_result_t                     result;
    globus_i_gfs_data_attr_t            attr;
    GlobusGFSName(globus_l_gfs_data_handle_init);
    
    handle = (globus_i_gfs_data_handle_t *) 
        globus_malloc(sizeof(globus_i_gfs_data_handle_t));
    if(!handle)
    {
        result = GlobusGFSErrorMemory("handle");
        goto error_alloc;
    }
    
    if(!data_info)
    {
        attr = globus_i_gfs_data_attr_defaults;
    }
    else
    {
        attr.delegated_cred = NULL;
        attr.ipv6 = data_info->ipv6;       
        attr.nstreams = data_info->nstreams;   
        attr.mode = data_info->mode;       
        attr.type = data_info->type;       
        attr.tcp_bufsize = data_info->tcp_bufsize;
        attr.blocksize = data_info->blocksize;  
        attr.prot = data_info->prot;       
        attr.dcau.subject.subject = data_info->subject;
        attr.dcau.mode = data_info->dcau;
    }

    memcpy(&handle->attr, &attr, sizeof(globus_i_gfs_data_attr_t));
    
    result = globus_ftp_control_handle_init(&handle->data_channel);
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusGFSErrorWrapFailed(
            "globus_ftp_control_handle_init", result);
        goto error_data;
    }
    
    result = globus_ftp_control_local_mode(
        &handle->data_channel, handle->attr.mode);
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusGFSErrorWrapFailed(
            "globus_ftp_control_local_mode", result);
        goto error_control;
    }
    
    result = globus_ftp_control_local_type(
        &handle->data_channel, handle->attr.type, 0);
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusGFSErrorWrapFailed(
            "globus_ftp_control_local_type", result);
        goto error_control;
    }
    
    if(handle->attr.tcp_bufsize > 0)
    {
        globus_ftp_control_tcpbuffer_t  tcpbuffer;
        
        tcpbuffer.mode = GLOBUS_FTP_CONTROL_TCPBUFFER_FIXED;
        tcpbuffer.fixed.size = handle->attr.tcp_bufsize;
        
        result = globus_ftp_control_local_tcp_buffer(
            &handle->data_channel, &tcpbuffer);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusGFSErrorWrapFailed(
                "globus_ftp_control_local_tcp_buffer", result);
            goto error_control;
        }
    }
    
    if(handle->attr.mode == 'S')
    {
        handle->attr.nstreams = 1;
    }
    else
    {
        globus_ftp_control_parallelism_t  parallelism;
        
        globus_assert(handle->attr.mode == 'E');
        
        parallelism.mode = GLOBUS_FTP_CONTROL_PARALLELISM_FIXED;
        parallelism.fixed.size = handle->attr.nstreams;
        
        result = globus_ftp_control_local_parallelism(
            &handle->data_channel, &parallelism);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusGFSErrorWrapFailed(
                "globus_ftp_control_local_parallelism", result);
            goto error_control;
        }

        result = globus_ftp_control_local_send_eof(
            &handle->data_channel, GLOBUS_FALSE);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusGFSErrorWrapFailed(
                "globus_ftp_control_local_send_eof", result);
            goto error_control;
        }
    }

    result = globus_ftp_control_local_dcau(
        &handle->data_channel, &handle->attr.dcau, handle->attr.delegated_cred);
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusGFSErrorWrapFailed(
            "globus_ftp_control_local_dcau", result);
        goto error_control;
    }
    if(handle->attr.dcau.mode != GLOBUS_FTP_CONTROL_DCAU_NONE)
    {
        result = globus_ftp_control_local_prot(
            &handle->data_channel, handle->attr.prot);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusGFSErrorWrapFailed(
                "globus_ftp_control_local_prot", result);
            goto error_control;
        }
    }
    
    globus_mutex_init(&handle->lock, GLOBUS_NULL);
    
    *u_handle = handle;
    return GLOBUS_SUCCESS;

error_control:
    globus_ftp_control_handle_destroy(&handle->data_channel);
    
error_data:
    globus_free(handle);
    
error_alloc:
    return result;
}

static
void
globus_l_gfs_data_close_cb(
    void *                              callback_arg,
    globus_ftp_control_handle_t *       ftp_handle,
    globus_object_t *                   error)
{
    globus_i_gfs_data_handle_t *        handle;
    
    handle = (globus_i_gfs_data_handle_t *) callback_arg;
    
    globus_mutex_destroy(&handle->lock);
    globus_ftp_control_handle_destroy(&handle->data_channel);
    globus_free(handle);
}

void
globus_i_gfs_data_destroy_handle(
    globus_gfs_ipc_handle_t             ipc_handle,
    int                                 session_id,
    int                                 data_connection_id)
{
    globus_result_t                     result;
    globus_i_gfs_data_handle_t *        handle;
    GlobusGFSName(globus_i_gfs_data_handle_destroy);
    
    if(globus_l_gfs_dsi->data_destroy_func != NULL)
    {
        globus_l_gfs_dsi->data_destroy_func(data_connection_id, (void *) session_id);
    }
    else
    {
        handle = (globus_i_gfs_data_handle_t *) data_connection_id;
        if(handle == GLOBUS_NULL)
        {
            goto error;
        }
        result = globus_ftp_control_data_force_close(
            &handle->data_channel, globus_l_gfs_data_close_cb, handle);
        if(result != GLOBUS_SUCCESS)
        {
            globus_mutex_destroy(&handle->lock);
            globus_ftp_control_handle_destroy(&handle->data_channel);
            globus_free(handle);
        }
    }

error:
    return;    
}

typedef struct
{
    globus_gfs_ipc_handle_t             ipc_handle;
    int                                 id;
    globus_i_gfs_data_handle_t *        handle;
    globus_bool_t                       bi_directional;
    char *                              contact_string;
    globus_i_gfs_data_callback_t      callback;
    void *                              user_arg;
} globus_l_gfs_data_passive_bounce_t;

static
void
globus_l_gfs_data_passive_kickout(
    void *                              user_arg)
{
    globus_l_gfs_data_passive_bounce_t * bounce_info;
    
    bounce_info = (globus_l_gfs_data_passive_bounce_t *) user_arg;
    
    {
    globus_gfs_ipc_reply_t *            reply;   
    reply = (globus_gfs_ipc_reply_t *) 
        globus_calloc(1, sizeof(globus_gfs_ipc_reply_t));
    reply->info.data.contact_strings = (const char **) 
        globus_calloc(1, sizeof(char *));
 
    reply->type = GLOBUS_GFS_OP_PASSIVE;
    reply->id = bounce_info->id;
    reply->result = GLOBUS_SUCCESS;
    reply->info.data.data_handle_id = (int) bounce_info->handle;
    reply->info.data.bi_directional = bounce_info->bi_directional;
    reply->info.data.cs_count = 1;
    *reply->info.data.contact_strings = (const char *) 
        globus_libc_strdup(bounce_info->contact_string);;
    
    if(bounce_info->callback != NULL)
    {
        bounce_info->callback(
            reply,
            bounce_info->user_arg);        
    }
    else
    {
        globus_gfs_ipc_reply_finished(
            bounce_info->ipc_handle,
            reply);
    }
    }

/*
    bounce_info->callback(
        bounce_info->instance,
        GLOBUS_SUCCESS,
        bounce_info->handle,
        bounce_info->bi_directional,
        (const char **) &bounce_info->contact_string,
        1,
        bounce_info->user_arg);
*/
    
    globus_free(bounce_info->contact_string);
    globus_free(bounce_info);
}

globus_result_t
globus_i_gfs_data_request_passive(
    globus_gfs_ipc_handle_t             ipc_handle,
    int                                 session_id,
    int                                 id,
    globus_gfs_data_info_t *            data_info,
    globus_i_gfs_data_callback_t        cb,
    void *                              user_arg)
{
    globus_i_gfs_data_handle_t *        handle;
    globus_result_t                     result;
    globus_ftp_control_host_port_t      address;
    globus_sockaddr_t                   addr;
    char *                              cs;
    globus_l_gfs_data_passive_bounce_t * bounce_info;
    globus_l_gfs_data_operation_t *     op;
    GlobusGFSName(globus_i_gfs_data_request_passive);

    if(globus_l_gfs_dsi->active_func != NULL)
    {
        result = globus_l_gfs_data_operation_init(&op);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusGFSErrorWrapFailed(
                "globus_l_gfs_data_operation_init", result);
            goto error_op;
        }
        
        op->ipc_handle = ipc_handle;
        op->id = id;
        op->state = GLOBUS_L_GFS_DATA_REQUESTING;
        op->pathname = data_info->pathname;
        op->callback = cb;
        op->user_arg = user_arg;
        result = globus_l_gfs_dsi->passive_func(op, data_info, (void *) session_id);
    }
    else
    {
    
        result = globus_l_gfs_data_handle_init(&handle, data_info);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusGFSErrorWrapFailed(
                "globus_l_gfs_data_handle_init", result);
            goto error_handle;
        }
        
        address.host[0] = 1; /* prevent address lookup */
        address.port = 0;
        result = globus_ftp_control_local_pasv(&handle->data_channel, &address);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusGFSErrorWrapFailed(
                "globus_ftp_control_local_pasv", result);
            goto error_control;
        }
              
        GlobusLibcSockaddrSetFamily(addr, AF_INET);
        GlobusLibcSockaddrSetPort(addr, address.port);
        result = globus_libc_addr_to_contact_string(
            &addr, GLOBUS_LIBC_ADDR_LOCAL | GLOBUS_LIBC_ADDR_NUMERIC, &cs);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusGFSErrorWrapFailed(
                "globus_libc_addr_to_contact_string", result);
            goto error_control;
        }
        
        bounce_info = (globus_l_gfs_data_passive_bounce_t *)
            globus_malloc(sizeof(globus_l_gfs_data_passive_bounce_t));
        if(!bounce_info)
        {
            result = GlobusGFSErrorMemory("bounce_info");
            goto error_alloc;
        }
        
        bounce_info->ipc_handle = ipc_handle;
        bounce_info->id = id;
        bounce_info->handle = handle;
        bounce_info->bi_directional = GLOBUS_TRUE; /* XXX MODE S only */
        bounce_info->contact_string = cs;
        bounce_info->callback = cb;
        bounce_info->user_arg = user_arg;
        
        result = globus_callback_register_oneshot(
            GLOBUS_NULL,
            GLOBUS_NULL,
            globus_l_gfs_data_passive_kickout,
            bounce_info);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusGFSErrorWrapFailed(
                "globus_callback_register_oneshot", result);
            goto error_oneshot;
        }
    }
    return result;

error_oneshot:
    globus_free(bounce_info);
    
error_alloc:
    globus_free(cs);
    
error_control:
    //globus_i_gfs_data_destroy_handle(handle);
    
error_handle:
error_op:
    return result;
}

typedef struct
{
    globus_gfs_ipc_handle_t             ipc_handle;
    int                                 id;
    globus_i_gfs_data_handle_t *        handle;
    globus_bool_t                       bi_directional;
    globus_i_gfs_data_callback_t       callback;
    void *                              user_arg;
} globus_l_gfs_data_active_bounce_t;

static
void
globus_l_gfs_data_active_kickout(
    void *                              user_arg)
{
    globus_l_gfs_data_active_bounce_t * bounce_info;
    
    bounce_info = (globus_l_gfs_data_active_bounce_t *) user_arg;

    {
    globus_gfs_ipc_reply_t *            reply;   
    reply = (globus_gfs_ipc_reply_t *) 
        globus_calloc(1, sizeof(globus_gfs_ipc_reply_t));

    reply->type = GLOBUS_GFS_OP_ACTIVE;
    reply->id = bounce_info->id;
    reply->result = GLOBUS_SUCCESS;
    reply->info.data.data_handle_id = (int) bounce_info->handle;
    reply->info.data.bi_directional = bounce_info->bi_directional;
    
    if(bounce_info->callback != NULL)
    {
        bounce_info->callback(
            reply,
            bounce_info->user_arg);        
    }
    else
    {
        globus_gfs_ipc_reply_finished(
            bounce_info->ipc_handle,
            reply);
    }
    }

/*    
    bounce_info->callback(
        bounce_info->instance,
        GLOBUS_SUCCESS,
        bounce_info->handle,
        bounce_info->bi_directional,
        bounce_info->user_arg);
*/  
  
    globus_free(bounce_info);
}

globus_result_t
globus_i_gfs_data_request_active(
    globus_gfs_ipc_handle_t             ipc_handle,
    int                                 session_id,
    int                                 id,
    globus_gfs_data_info_t *            data_info,
    globus_i_gfs_data_callback_t        cb,
    void *                              user_arg)
{
    globus_i_gfs_data_handle_t *        handle;
    globus_result_t                     result;
    globus_ftp_control_host_port_t *    addresses;
    int                                 i;
    globus_l_gfs_data_active_bounce_t * bounce_info;
    globus_l_gfs_data_operation_t *     op;
    GlobusGFSName(globus_i_gfs_data_request_active);

    if(globus_l_gfs_dsi->active_func != NULL)
    {
        result = globus_l_gfs_data_operation_init(&op);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusGFSErrorWrapFailed(
                "globus_l_gfs_data_operation_init", result);
            goto error_op;
        }
        
        op->ipc_handle = ipc_handle;
        op->id = id;
        op->state = GLOBUS_L_GFS_DATA_REQUESTING;
        op->pathname = data_info->pathname;
        op->callback = cb;
        op->user_arg = user_arg;
        result = globus_l_gfs_dsi->active_func(op, data_info, (void *) session_id);
    }
    else
    {
        result = globus_l_gfs_data_handle_init(&handle, data_info);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusGFSErrorWrapFailed(
                "globus_l_gfs_data_handle_init", result);
            goto error_handle;
        }
        addresses = (globus_ftp_control_host_port_t *)
            globus_malloc(sizeof(globus_ftp_control_host_port_t) * 
                data_info->cs_count);
        if(!addresses)
        {
            result = GlobusGFSErrorMemory("addresses");
            goto error_addresses;
        }
        
        for(i = 0; i < data_info->cs_count; i++)
        {
            int                             rc;
            
            rc = sscanf(
                data_info->contact_strings[i],
                "%d.%d.%d.%d:%hu",
                &addresses[i].host[0],
                &addresses[i].host[1],
                &addresses[i].host[2], 
                &addresses[i].host[3], 
                &addresses[i].port);
            if(rc < 5)
            {
                result = GlobusGFSErrorGeneric("Bad contact string");
                goto error_format;
            }
        }
        
        if(data_info->cs_count == 1)
        {
            result = globus_ftp_control_local_port(
                &handle->data_channel, addresses);
        }
        else
        {
            result = globus_ftp_control_local_spor(
                &handle->data_channel, addresses, data_info->cs_count);
        }
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusGFSErrorWrapFailed(
                "globus_ftp_control_local_port/spor", result);
            goto error_control;
        }
        
        bounce_info = (globus_l_gfs_data_active_bounce_t *)
            globus_malloc(sizeof(globus_l_gfs_data_active_bounce_t));
        if(!bounce_info)
        {
            result = GlobusGFSErrorMemory("bounce_info");
            goto error_alloc;
        }
        
        bounce_info->ipc_handle = ipc_handle;
        bounce_info->id = id;
        bounce_info->handle = handle;
        bounce_info->bi_directional = GLOBUS_TRUE; /* XXX MODE S only */
        bounce_info->callback = cb;
        bounce_info->user_arg = user_arg;
        
        result = globus_callback_register_oneshot(
            GLOBUS_NULL,
            GLOBUS_NULL,
            globus_l_gfs_data_active_kickout,
            bounce_info);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusGFSErrorWrapFailed(
                "globus_callback_register_oneshot", result);
            goto error_oneshot;
        }
        
        globus_free(addresses);
    }
    return result;

error_oneshot:
    globus_free(bounce_info);
    
error_alloc:
error_control:
error_format:
    globus_free(addresses);
    
error_addresses:
   // globus_i_gfs_data_destroy_handle(handle);
error_handle:
error_op:
    return result;
}

    
globus_result_t
globus_i_gfs_data_request_recv(
    globus_gfs_ipc_handle_t             ipc_handle,
    int                                 session_id,
    int                                 id,
    globus_gfs_transfer_info_t *        recv_info,
    globus_i_gfs_data_callback_t        cb,
    globus_i_gfs_data_event_callback_t  event_cb,
    void *                              user_arg)
{
    globus_l_gfs_data_operation_t *     op;
    globus_result_t                     result;
    globus_i_gfs_data_handle_t *        data_handle;
    GlobusGFSName(globus_i_gfs_data_recv_request);

    data_handle = (globus_i_gfs_data_handle_t *)
        recv_info->data_handle_id;

    if(data_handle == NULL)
    {
        result = GlobusGFSErrorData("Data handle not found");
        goto error_handle;
    }
    result = globus_l_gfs_data_operation_init(&op);
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusGFSErrorWrapFailed(
            "globus_l_gfs_data_operation_init", result);
        goto error_op;
    }

    op->ipc_handle = ipc_handle;    
    op->id = id;
    op->state = GLOBUS_L_GFS_DATA_REQUESTING;
    op->data_handle = data_handle;
    op->sending = GLOBUS_FALSE;
    op->range_list = recv_info->range_list;
    op->partial_offset = recv_info->partial_offset;
    op->callback = cb;
    op->event_callback = event_cb;
    op->user_arg = user_arg;
    op->node_ndx = recv_info->node_ndx;
    op->node_count = recv_info->node_count;    
    op->stripe_count = recv_info->stripe_count;
    
    /* XXX */
    result = globus_l_gfs_dsi->recv_func(op, recv_info, (void *) session_id);
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusGFSErrorWrapFailed("recv_hook", result);
        goto error_hook;
    }
    
    globus_mutex_lock(&op->lock);
    {
        if(op->state == GLOBUS_L_GFS_DATA_REQUESTING)
        {
            op->state = GLOBUS_L_GFS_DATA_PENDING;
        }
    }
    globus_mutex_unlock(&op->lock);
    
    return GLOBUS_SUCCESS;

error_hook:
    globus_l_gfs_data_operation_destroy(op);

error_op:
error_handle:
    return result;
}

    
globus_result_t
globus_i_gfs_data_request_send(
    globus_gfs_ipc_handle_t             ipc_handle,
    int                                 session_id,
    int                                 id,
    globus_gfs_transfer_info_t *        send_info,
    globus_i_gfs_data_callback_t        cb,
    globus_i_gfs_data_event_callback_t  event_cb,
    void *                              user_arg)   
{
    globus_l_gfs_data_operation_t *     op;
    globus_result_t                     result;
    globus_i_gfs_data_handle_t *        data_handle;
    GlobusGFSName(globus_i_gfs_data_send_request);

    data_handle = (globus_i_gfs_data_handle_t *)
        send_info->data_handle_id;

    if(data_handle == NULL)
    {
        result = GlobusGFSErrorData("Data handle not found");
        goto error_handle;
    }
    result = globus_l_gfs_data_operation_init(&op);
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusGFSErrorWrapFailed(
            "globus_l_gfs_data_operation_init", result);
        goto error_op;
    }
    op->ipc_handle = ipc_handle;    
    op->id = id;
    op->state = GLOBUS_L_GFS_DATA_REQUESTING;
    op->data_handle = data_handle;
    op->sending = GLOBUS_TRUE;
    op->range_list = send_info->range_list;
    op->partial_length = send_info->partial_length;
    op->partial_offset = send_info->partial_offset;
    op->callback = cb;
    op->event_callback = event_cb;
    op->user_arg = user_arg;
    op->node_ndx = send_info->node_ndx;
    op->write_stripe = 0;
    op->stripe_chunk = send_info->node_ndx;
    op->node_count = send_info->node_count;    
    op->stripe_count = send_info->stripe_count;
    op->nstreams = send_info->nstreams;
    op->eof_count = (int *) globus_malloc(op->stripe_count * sizeof(int));

    
    /* XXX */
    result = globus_l_gfs_dsi->send_func(op, send_info, (void *) session_id);
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusGFSErrorWrapFailed("send_hook", result);
        goto error_hook;
    }
    
    globus_mutex_lock(&op->lock);
    {
        if(op->state == GLOBUS_L_GFS_DATA_REQUESTING)
        {
            op->state = GLOBUS_L_GFS_DATA_PENDING;
        }
    }
    globus_mutex_unlock(&op->lock);
    
    return GLOBUS_SUCCESS;

error_hook:
    globus_l_gfs_data_operation_destroy(op);

error_op:
error_handle:
    return result;
}

static
void
globus_l_gfs_data_list_write_cb(
    globus_gfs_operation_t   op,
    globus_result_t                     result,
    globus_byte_t *                     buffer,
    globus_size_t                       nbytes,
    void *                              user_arg)
{
    globus_gridftp_server_control_list_buffer_free(buffer);
    
    globus_gridftp_server_finished_transfer(op, result); 
}


static
void
globus_l_gfs_data_list_stat_cb(
    globus_gfs_data_reply_t *           reply,
    void *                              user_arg)
{
    GlobusGFSName(globus_l_gfs_data_list_stat_cb);
    globus_gfs_operation_t   op;
    globus_byte_t *                     list_buffer;
    globus_size_t                       buffer_len;
    globus_l_gfs_data_bounce_t *        bounce_info;
    globus_result_t                     result;
 
    op = (globus_gfs_operation_t) user_arg;
    bounce_info = (globus_l_gfs_data_bounce_t *) op->user_arg;

    result = globus_gridftp_server_control_list_buffer_alloc(
            op->list_type,
            op->uid,
            reply->info.stat.stat_array, 
            reply->info.stat.stat_count,
            &list_buffer,
            &buffer_len);
    
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusGFSErrorWrapFailed(
           "globus_gridftp_server_control_list_buffer_alloc", result);
        goto error;
    }
    
    globus_gridftp_server_begin_transfer(op);
    
    result = globus_gridftp_server_register_write(
        op,
        list_buffer,  
        buffer_len,
        0,
        -1,
        globus_l_gfs_data_list_write_cb,
        bounce_info);

    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusGFSErrorWrapFailed(
            "globus_gridftp_server_register_write", result);
        goto error;
    }

    return;
    
error:
    op->state = GLOBUS_L_GFS_DATA_ERROR;
    globus_gridftp_server_finished_transfer(op, result); 

    return;
}

globus_result_t
globus_i_gfs_data_request_list(
    globus_gfs_ipc_handle_t             ipc_handle,
    int                                 session_id,
    int                                 id,
    globus_gfs_transfer_info_t *        list_info,
    globus_i_gfs_data_callback_t        cb,
    globus_i_gfs_data_event_callback_t  event_cb,
    void *                              user_arg)
{
    globus_l_gfs_data_operation_t *     stat_op;
    globus_l_gfs_data_operation_t *     data_op;
    globus_result_t                     result;
    globus_i_gfs_data_handle_t *        data_handle;
    globus_gfs_stat_info_t *       stat_info;
    GlobusGFSName(globus_i_gfs_data_list_request);

    data_handle = (globus_i_gfs_data_handle_t *)
        list_info->data_handle_id;

    if(data_handle == NULL)
    {
        result = GlobusGFSErrorData("Data handle not found");
        goto error_handle;
    }

    result = globus_l_gfs_data_operation_init(&data_op);
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusGFSErrorWrapFailed(
            "globus_l_gfs_data_operation_init", result);
        goto error_op;
    }

    data_op->ipc_handle = ipc_handle;    
    data_op->id = id;
    data_op->state = GLOBUS_L_GFS_DATA_PENDING;
    data_op->data_handle = data_handle;
    data_op->sending = GLOBUS_TRUE;
    data_op->list_type = list_info->list_type;
    data_op->uid = getuid();
    /* XXX */
    data_op->callback = cb;
    data_op->event_callback = event_cb;
    data_op->user_arg = user_arg;
    
    if(globus_l_gfs_dsi->list_func != NULL)
    {
        result = globus_l_gfs_dsi->list_func(data_op, list_info, (void *) session_id);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusGFSErrorWrapFailed("list_hook", result);
            goto error_hook1;
        }
    }
    else
    {    
        result = globus_l_gfs_data_operation_init(&stat_op);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusGFSErrorWrapFailed(
                "globus_l_gfs_data_operation_init", result);
            goto error_op;
        }
        stat_op->state = GLOBUS_L_GFS_DATA_REQUESTING;
        stat_op->callback = globus_l_gfs_data_list_stat_cb;
        stat_op->user_arg = data_op;
        
        stat_info = (globus_gfs_stat_info_t *) 
            globus_calloc(1, sizeof(globus_gfs_stat_info_t));
        
        stat_info->pathname = list_info->pathname;
        stat_info->file_only = GLOBUS_FALSE;
    
        /* XXX */
        result = globus_l_gfs_dsi->stat_func(stat_op, stat_info, (void *) session_id);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusGFSErrorWrapFailed("list_hook", result);
            goto error_hook2;
        }
        globus_mutex_lock(&stat_op->lock);
        {
            if(stat_op->state == GLOBUS_L_GFS_DATA_REQUESTING)
            {
                stat_op->state = GLOBUS_L_GFS_DATA_PENDING;
            }
        }
        globus_mutex_unlock(&stat_op->lock);
    }

    
    return GLOBUS_SUCCESS;

error_hook2:
    globus_l_gfs_data_operation_destroy(stat_op);
error_handle:
error_hook1:
    globus_l_gfs_data_operation_destroy(data_op);

error_op:
    return result;
}


void
globus_gridftp_server_begin_transfer(
    globus_gfs_operation_t              op)
{
    globus_result_t                     result;
    GlobusGFSName(globus_gridftp_server_begin_transfer);
    
    if(op->sending)
    {
        result = globus_ftp_control_data_connect_write(
            &op->data_handle->data_channel, GLOBUS_NULL, GLOBUS_NULL);
        if(result != GLOBUS_SUCCESS)
        {
            goto error_connect;
        }
    }
    else
    {
        result = globus_ftp_control_data_connect_read(
            &op->data_handle->data_channel, GLOBUS_NULL, GLOBUS_NULL);
        if(result != GLOBUS_SUCCESS)
        {
            goto error_connect;
        }
    }    

    {
    globus_gfs_ipc_event_reply_t *            event_reply;   
    event_reply = (globus_gfs_ipc_event_reply_t *) 
        globus_calloc(1, sizeof(globus_gfs_ipc_event_reply_t));

    event_reply->type = GLOBUS_GFS_EVENT_TRANSFER_BEGIN;
    event_reply->id = op->id;
    event_reply->transfer_id = (int) op;
    if(op->event_callback != NULL)
    {
        op->event_callback(
            event_reply,
            op->user_arg);        
    }
    else
    {
        globus_gfs_ipc_reply_event(
            op->ipc_handle,
            event_reply);
    }

    }
/*
    op->event_callback(
        op->instance,
        GLOBUS_GFS_EVENT_TRANSFER_BEGIN,
        GLOBUS_NULL,
        op->user_arg);
*/    
    return;
    
error_connect:
    op->state = GLOBUS_L_GFS_DATA_ERROR;
    globus_gridftp_server_finished_transfer(op, result);
}

static
void
globus_l_gfs_data_send_eof_cb(
    void *                                      callback_arg,
    struct globus_ftp_control_handle_s *        handle,
    globus_object_t *				error)
{

}


static
void
globus_l_gfs_data_write_eof_cb(
    void *                              user_arg,
    globus_ftp_control_handle_t *       handle,
    globus_object_t *                   error,
    globus_byte_t *                     buffer,
    globus_size_t                       length,
    globus_off_t                        offset,
    globus_bool_t                       eof)
{    
    /* XXX mode s only */
    /* racey shit here */
    globus_gfs_ipc_reply_t *            reply;   
    globus_gfs_ipc_event_reply_t *      event_reply; 
    globus_result_t                     result;  
    int                                 i;
    globus_gfs_operation_t              op;
    GlobusGFSName(globus_l_gfs_data_write_eof_cb);
    
    op = (globus_gfs_operation_t) user_arg;

    if(op->data_handle->attr.mode == 'E')
    {        
        for(i = 0; i < op->stripe_count; i++)
        {
            op->eof_count[i] = 
                (op->node_ndx == 0) ?
                (op->node_count - 1) * op->data_handle->attr.nstreams :
                0;
        }

        result = globus_ftp_control_data_send_eof(
            &op->data_handle->data_channel,
            op->eof_count,
            op->stripe_count,
            (op->node_ndx == 0) ? GLOBUS_TRUE : GLOBUS_FALSE,
            globus_l_gfs_data_send_eof_cb,
            op);
        if(result != GLOBUS_SUCCESS)
        {
            globus_i_gfs_log_result(
                "ERROR", result);
        }
    }
    reply = (globus_gfs_ipc_reply_t *) 
        globus_calloc(1, sizeof(globus_gfs_ipc_reply_t));
    event_reply = (globus_gfs_ipc_event_reply_t *) 
        globus_calloc(1, sizeof(globus_gfs_ipc_event_reply_t));

    event_reply->type = GLOBUS_GFS_EVENT_DISCONNECTED;
    event_reply->id = op->id;

    if(op->event_callback != NULL)
    {
        op->event_callback(
            event_reply,
            op->user_arg);        
    }
    else
    {
        globus_gfs_ipc_reply_event(
            op->ipc_handle,
            event_reply);
    }

    /*
    op->event_callback(
        op->instance,
        GLOBUS_GFS_EVENT_DISCONNECTED,
        op->data_handle,
        op->user_arg);
    */
    reply->type = GLOBUS_GFS_OP_TRANSFER;
    reply->id = op->id;
    reply->result = error ? 
        globus_error_put(globus_object_copy(error)) : GLOBUS_SUCCESS;
             
    if(op->callback != NULL)
    {
        op->callback(
            reply,
            op->user_arg);        
    }
    else
    {
        globus_gfs_ipc_reply_finished(
            op->ipc_handle,
            reply);
    }
    /*            
    op->transfer_callback(
        op->instance,
        error ? globus_error_put(globus_object_copy(error)) : GLOBUS_SUCCESS,
        op->user_arg);
    */
    
    globus_l_gfs_data_operation_destroy(op);
}

void
globus_gridftp_server_finished_transfer(
    globus_gfs_operation_t   op,
    globus_result_t                     result)
{
    GlobusGFSName(globus_gridftp_server_finished_transfer);
    
    switch(op->state)
    {
      case GLOBUS_L_GFS_DATA_PENDING:
      case GLOBUS_L_GFS_DATA_REQUESTING:
        op->state = GLOBUS_L_GFS_DATA_COMPLETE;
        
        if(result == GLOBUS_SUCCESS && op->sending)
        {
            result = globus_ftp_control_data_write(
                &op->data_handle->data_channel,
                "",
                0,
                0,
                GLOBUS_TRUE,
                globus_l_gfs_data_write_eof_cb,
                op);
        }
        if(result == GLOBUS_SUCCESS && !op->sending)
        {
            globus_gfs_ipc_event_reply_t *      event_reply;   
            globus_gfs_ipc_reply_t *            reply;   
            reply = (globus_gfs_ipc_reply_t *) 
                globus_calloc(1, sizeof(globus_gfs_ipc_reply_t));
            event_reply = (globus_gfs_ipc_event_reply_t *) 
                globus_calloc(1, sizeof(globus_gfs_ipc_event_reply_t));
         
            event_reply->id = op->id;
            event_reply->recvd_bytes = op->recvd_bytes;
            event_reply->recvd_ranges = op->recvd_ranges;
            
            event_reply->type = GLOBUS_GFS_EVENT_BYTES_RECVD;
            if(op->event_callback != NULL)
            {
                op->event_callback(
                    event_reply,
                    op->user_arg);        
            }
            else
            {
                globus_gfs_ipc_reply_event(
                    op->ipc_handle,
                    event_reply);
            }

            event_reply->type = GLOBUS_GFS_EVENT_RANGES_RECVD;
            if(op->event_callback != NULL)
            {
                op->event_callback(
                    event_reply,
                    op->user_arg);        
            }
            else
            {
                globus_gfs_ipc_reply_event(
                    op->ipc_handle,
                    event_reply);
            }
        }                
        if(result != GLOBUS_SUCCESS || !op->sending)
        {
            globus_gfs_ipc_event_reply_t *      event_reply;   
            globus_gfs_ipc_reply_t *            reply;   
            reply = (globus_gfs_ipc_reply_t *) 
                globus_calloc(1, sizeof(globus_gfs_ipc_reply_t));
            event_reply = (globus_gfs_ipc_event_reply_t *) 
                globus_calloc(1, sizeof(globus_gfs_ipc_event_reply_t));
         
            event_reply->id = op->id;
            
            /* XXX mode s only */
            /* racey shit here */
         
            event_reply->type = GLOBUS_GFS_EVENT_DISCONNECTED;        
            if(op->event_callback != NULL)
            {
                op->event_callback(
                    event_reply,
                    op->user_arg);        
            }
            else
            {
                globus_gfs_ipc_reply_event(
                    op->ipc_handle,
                    event_reply);
            }
        
            /*
            op->event_callback(
                op->instance,
                GLOBUS_GFS_EVENT_DISCONNECTED,
                op->data_handle,
                op->user_arg);
            */
            reply->type = GLOBUS_GFS_OP_TRANSFER;
            reply->id = op->id;
            reply->result = result;
                     
            if(op->callback != NULL)
            {
                op->callback(
                    reply,
                    op->user_arg);        
            }
            else
            {
                globus_gfs_ipc_reply_finished(
                    op->ipc_handle,
                    reply);
            }
            /*
            op->transfer_callback(
                op->instance,
                result,
                op->user_arg);
            */
            
            globus_l_gfs_data_operation_destroy(op);
        }
        break;
        
      case GLOBUS_L_GFS_DATA_ERROR_COMPLETE:
        op->state = GLOBUS_L_GFS_DATA_COMPLETE;
        
        globus_l_gfs_data_operation_destroy(op);
        break;
      
      /* this state always means this was called internally */
      case GLOBUS_L_GFS_DATA_ERROR:
        {
        /* racey shit here */
        globus_gfs_ipc_reply_t *            reply;   
        globus_gfs_ipc_event_reply_t *            event_reply;   
        reply = (globus_gfs_ipc_reply_t *) 
            globus_calloc(1, sizeof(globus_gfs_ipc_reply_t));
        event_reply = (globus_gfs_ipc_event_reply_t *) 
            globus_calloc(1, sizeof(globus_gfs_ipc_event_reply_t));

        event_reply->type = GLOBUS_GFS_EVENT_DISCONNECTED;
        event_reply->id = op->id;
    
        if(op->event_callback != NULL)
        {
            op->event_callback(
                event_reply,
                op->user_arg);        
        }
        else
        {
            globus_gfs_ipc_reply_event(
                op->ipc_handle,
                event_reply);
        }
    
        /*
        op->event_callback(
            op->instance,
            GLOBUS_GFS_EVENT_DISCONNECTED,
            op->data_handle,
            op->user_arg);
        */
        reply->type = GLOBUS_GFS_OP_TRANSFER;
        reply->id = op->id;
        reply->result = result;
                     
        if(op->callback != NULL)
        {
            op->callback(
                reply,
                op->user_arg);        
        }
        else
        {
            globus_gfs_ipc_reply_finished(
                op->ipc_handle,
                reply);
        }
        /*
        op->transfer_callback(
            op->instance,
            result,
            op->user_arg);
        */
        op->state = GLOBUS_L_GFS_DATA_ERROR_COMPLETE;
        break;
      }
      default:
        globus_assert(0 && "Invalid state");
        break;
    }
}


static
void
globus_l_gfs_data_write_cb(
    void *                              user_arg,
    globus_ftp_control_handle_t *       handle,
    globus_object_t *                   error,
    globus_byte_t *                     buffer,
    globus_size_t                       length,
    globus_off_t                        offset,
    globus_bool_t                       eof)
{
    globus_l_gfs_data_bounce_t *        bounce_info;
    GlobusGFSName(globus_l_gfs_data_write_cb);
    
    bounce_info = (globus_l_gfs_data_bounce_t *) user_arg;
    
    bounce_info->callback.write(
        bounce_info->op,
        error ? globus_error_put(globus_object_copy(error)) : GLOBUS_SUCCESS,
        buffer,
        length,
        bounce_info->user_arg);
        
    globus_free(bounce_info);
}

globus_result_t
globus_gridftp_server_register_write(
    globus_gfs_operation_t   op,
    globus_byte_t *                     buffer,  
    globus_size_t                       length,  
    globus_off_t                        offset,  
    int                                 stripe_ndx,  
    globus_gridftp_server_write_cb_t    callback,  
    void *                              user_arg)
{
    globus_result_t                     result;
    globus_l_gfs_data_bounce_t *        bounce_info;
    GlobusGFSName(globus_gridftp_server_register_write);
    
    bounce_info = (globus_l_gfs_data_bounce_t *)
        globus_malloc(sizeof(globus_l_gfs_data_bounce_t));
    if(!bounce_info)
    {
        result = GlobusGFSErrorMemory("bounce_info");
        goto error_alloc;
    }
    
    bounce_info->op = op;
    bounce_info->callback.write = callback;
    bounce_info->user_arg = user_arg;

    if(op->data_handle->attr.mode == 'E')
    {
        globus_mutex_lock(&op->lock);
        {
            if(stripe_ndx != -1)
            {
                op->write_stripe = stripe_ndx;
            }
            if(op->write_stripe >= op->stripe_count)
            {
                op->write_stripe %= op->stripe_count;
            }    
            result = globus_ftp_control_data_write_stripe(
                &op->data_handle->data_channel,
                buffer,
                length,
                offset,
                GLOBUS_FALSE,
                op->write_stripe,
                globus_l_gfs_data_write_cb,
                bounce_info);
                
            op->write_stripe++;
        }    
        globus_mutex_unlock(&op->lock);
    }
    else
    {
        result = globus_ftp_control_data_write(
            &op->data_handle->data_channel,
            buffer,
            length,
            offset,
            GLOBUS_FALSE,
            globus_l_gfs_data_write_cb,
            bounce_info);
    }
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusGFSErrorWrapFailed(
            "globus_ftp_control_data_write", result);
        goto error_register;
    }
    
    return GLOBUS_SUCCESS;

error_register:
    globus_free(bounce_info);
    
error_alloc:
    return result;
}

static
void
globus_l_gfs_data_read_cb(
    void *                              user_arg,
    globus_ftp_control_handle_t *       handle,
    globus_object_t *                   error,
    globus_byte_t *                     buffer,
    globus_size_t                       length,
    globus_off_t                        offset,
    globus_bool_t                       eof)
{
    globus_l_gfs_data_bounce_t *        bounce_info;
    GlobusGFSName(globus_l_gfs_data_read_cb);
    
    bounce_info = (globus_l_gfs_data_bounce_t *) user_arg;
    
    bounce_info->callback.read(
        bounce_info->op,
        error ? globus_error_put(globus_object_copy(error)) : GLOBUS_SUCCESS,
        buffer,
        length,
        offset,
        eof,
        bounce_info->user_arg);
    
    globus_free(bounce_info);
}
    
globus_result_t
globus_gridftp_server_register_read(
    globus_gfs_operation_t   op,
    globus_byte_t *                     buffer,
    globus_size_t                       length,
    globus_gridftp_server_read_cb_t     callback,  
    void *                              user_arg)
{
    globus_result_t                     result;
    globus_l_gfs_data_bounce_t *        bounce_info;
    GlobusGFSName(globus_gridftp_server_register_read);
    
    bounce_info = (globus_l_gfs_data_bounce_t *)
        globus_malloc(sizeof(globus_l_gfs_data_bounce_t));
    if(!bounce_info)
    {
        result = GlobusGFSErrorMemory("bounce_info");
        goto error_alloc;
    }
    
    bounce_info->op = op;
    bounce_info->callback.read = callback;
    bounce_info->user_arg = user_arg;
    
    result = globus_ftp_control_data_read(
        &op->data_handle->data_channel,
        buffer,
        length,
        globus_l_gfs_data_read_cb,
        bounce_info);
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusGFSErrorWrapFailed(
            "globus_ftp_control_data_read", result);
        goto error_register;
    }
    
    return GLOBUS_SUCCESS;

error_register:
    globus_free(bounce_info);
    
error_alloc:
    return result;
}

void
globus_gridftp_server_update_bytes_written(
    globus_gfs_operation_t              op,
    globus_off_t                        offset,
    globus_off_t                        length)
{
    GlobusGFSName(globus_gridftp_server_update_bytes_written);

    globus_mutex_lock(&op->lock);
    {
        op->recvd_bytes += length;
        globus_range_list_insert(op->recvd_ranges, offset, length);
    }
    globus_mutex_unlock(&op->lock);

    return;
}

void
globus_gridftp_server_get_optimal_concurrency(
    globus_gfs_operation_t              op,
    int *                               count)
{
    GlobusGFSName(globus_gridftp_server_get_optimal_concurrency);
    
    *count = op->data_handle->attr.nstreams * op->stripe_count * 2;
}

void
globus_gridftp_server_get_block_size(
    globus_gfs_operation_t              op,
    globus_size_t *                     block_size)
{
    GlobusGFSName(globus_gridftp_server_get_block_size);
    
    *block_size = op->data_handle->attr.blocksize;
}


/* this is used to translate the restart and partial offset/lengths into
    a sets of ranges to transfer... storage interface shouldn't know about
    partial or restart semantics, it only needs to know which offsets to 
    read from the data source, and what offset to write to data sink
    (dest offset only matters for mode e, but again, storage interface 
    doesn't know about modes)
*/
void
globus_gridftp_server_get_write_range(
    globus_gfs_operation_t   op,
    globus_off_t *                      offset,
    globus_off_t *                      length,
    globus_off_t *                      write_delta,
    globus_off_t *                      transfer_delta)
{
    GlobusGFSName(globus_gridftp_server_get_write_range);
    globus_off_t                        tmp_off = 0;
    globus_off_t                        tmp_len = -1;
    globus_off_t                        tmp_write = 0;
    globus_off_t                        tmp_transfer = 0;
    int                                 rc;

    if(globus_range_list_size(op->range_list))
    {
        rc = globus_range_list_remove_at(
            op->range_list,
            0,
            &tmp_off,
            &tmp_len);
    }
    if(op->data_handle->attr.mode == 'S')
    {
        tmp_write = tmp_off;
    }
    if(op->partial_offset > 0)
    {
        tmp_off += op->partial_offset;
        tmp_write += op->partial_offset;
        tmp_transfer = 0 - op->partial_offset;
    }
    if(offset)
    {
        *offset = tmp_off;
    }
    if(length)
    {
        *length = tmp_len;
    }
    if(write_delta)
    {
        *write_delta = tmp_write;
    }
    if(transfer_delta)
    {
        *transfer_delta = tmp_transfer;
    }
    return;
}


/*
    stripe_block_size * node_ndx = start_offset
    start_offset + stripe_block_size - 1 = end_offset;


copy list
remove 0-start_offset
remove end_offset--1
use that list
if it has multiple set flag to not use next list i guess

*/



void
globus_gridftp_server_get_read_range(
    globus_gfs_operation_t              op,
    globus_off_t *                      offset,
    globus_off_t *                      length,
    globus_off_t *                      write_delta)
{
    globus_off_t                        tmp_off = 0;
    globus_off_t                        tmp_len = -1;
    globus_off_t                        tmp_write = 0;
    int                                 rc;
    globus_off_t                        start_offset;
    globus_off_t                        end_offset;
    globus_off_t                        stripe_block_size;
    globus_bool_t                       done = GLOBUS_FALSE;
    int                                 size;
    int                                 i;
    GlobusGFSName(globus_gridftp_server_get_read_range);
    
    globus_mutex_lock(&op->lock);
    {
        stripe_block_size = op->data_handle->attr.blocksize * 2;
        start_offset = op->stripe_chunk * stripe_block_size;
        end_offset = start_offset + stripe_block_size;
            
        if(globus_range_list_size(op->stripe_range_list))
        {
            rc = globus_range_list_remove_at(
                op->stripe_range_list,
                0,
                &tmp_off,
                &tmp_len);
        
            tmp_write = op->write_delta;
        }
        else if((size = globus_range_list_size(op->range_list)) != 0)
        {
            for(i = 0; i < size; i++)
            {
                rc = globus_range_list_at(
                    op->range_list,
                    i,
                    &tmp_off,
                    &tmp_len);
        
                if(op->partial_length != -1)
                {
                    if(tmp_len == -1)
                    {
                        tmp_len = op->partial_length;
                    }
                    if(tmp_off + tmp_len > op->partial_length)
                    {
                        tmp_len = op->partial_length - tmp_off;
                        if(tmp_len < 0)
                        {
                            tmp_len = 0;
                        }
                    }
                }
                
                if(op->partial_offset > 0)
                {
                    tmp_off += op->partial_offset;
                    tmp_write = 0 - op->partial_offset;
                }
                
                globus_range_list_insert(
                    op->stripe_range_list, tmp_off, tmp_len);
                op->write_delta = tmp_write;
            }
            globus_range_list_remove(
                op->stripe_range_list, 0, start_offset);
            globus_range_list_remove(
                op->stripe_range_list, end_offset, GLOBUS_RANGE_LIST_MAX);
            op->stripe_chunk += op->node_count;
            
            if(globus_range_list_size(op->stripe_range_list))
            {
                rc = globus_range_list_remove_at(
                    op->stripe_range_list,
                    0,
                    &tmp_off,
                    &tmp_len);
            
                tmp_write = op->write_delta;
            }
            else
            {
                tmp_len = 0;
                tmp_off = 0;
                tmp_write = 0;
            }
        }
        else
        {
            tmp_len = 0;
        }
    }
    globus_mutex_unlock(&op->lock);
    if(offset)
    {
        *offset = tmp_off;
    }
    if(length)
    {
        *length = tmp_len;
    }
    if(write_delta)
    {
        *write_delta = tmp_write;
    }
    
    return; 
}

typedef struct
{
    globus_l_gfs_data_operation_t *     op;
    int                                 event_type;
} globus_l_gfs_data_trev_bounce_t;


void
globus_l_gfs_data_transfer_event_kickout(
    void *                              user_arg)
{
    globus_l_gfs_data_trev_bounce_t *   bounce_info;
    globus_gfs_ipc_event_reply_t *      event_reply;
    GlobusGFSName(globus_l_gfs_data_transfer_event_kickout);

    bounce_info = (globus_l_gfs_data_trev_bounce_t *) user_arg;
    event_reply = (globus_gfs_ipc_event_reply_t *) 
        globus_calloc(1, sizeof(globus_gfs_ipc_event_reply_t));
         
    event_reply->id = bounce_info->op->id;
    event_reply->node_ndx = bounce_info->op->node_ndx;
    globus_mutex_lock(&bounce_info->op->lock);
    {    
        switch(bounce_info->event_type)
        {
          case GLOBUS_GRIDFTP_SERVER_CONTROL_EVENT_PERF:
                event_reply->recvd_bytes = bounce_info->op->recvd_bytes;
                bounce_info->op->recvd_bytes = 0;
                event_reply->type = GLOBUS_GFS_EVENT_BYTES_RECVD;
            break;
            
          case GLOBUS_GRIDFTP_SERVER_CONTROL_EVENT_RESTART:
                event_reply->type = GLOBUS_GFS_EVENT_RANGES_RECVD;
                event_reply->recvd_ranges = bounce_info->op->recvd_ranges;
            break;
            
          default:
            break;
        } 
        if(bounce_info->op->event_callback != NULL)
        {
            bounce_info->op->event_callback(
                event_reply,
                bounce_info->op->user_arg);        
        }
        else
        {
            globus_gfs_ipc_reply_event(
                bounce_info->op->ipc_handle,
                event_reply);
        }
        if(bounce_info->event_type == 
            GLOBUS_GRIDFTP_SERVER_CONTROL_EVENT_RESTART)
        {
            globus_range_list_remove(
                bounce_info->op->recvd_ranges, 0, GLOBUS_RANGE_LIST_MAX);
            if(globus_range_list_size(bounce_info->op->recvd_ranges))
            {
               globus_i_gfs_log_message(
                    GLOBUS_I_GFS_LOG_ERR, "range remove borken");
            }
        }
    }
    globus_mutex_unlock(&bounce_info->op->lock);
        
    globus_free(bounce_info);       
}

void
globus_i_gfs_data_request_transfer_event(
    globus_gfs_ipc_handle_t             ipc_handle,
    int                                 session_id,
    int                                 transfer_id,
    int                                 event_type)
{
    globus_result_t                     result;
    globus_l_gfs_data_trev_bounce_t *   bounce_info;
    GlobusGFSName(globus_i_gfs_data_kickoff_event);

    if(globus_l_gfs_dsi->trev_func != NULL)
    {
        globus_l_gfs_dsi->trev_func(transfer_id, event_type, (void *) session_id);
    }
    else
    {    
        bounce_info = (globus_l_gfs_data_trev_bounce_t *)
            globus_malloc(sizeof(globus_l_gfs_data_trev_bounce_t));
        if(!bounce_info)
        {
            result = GlobusGFSErrorMemory("bounce_info");
            goto error_alloc;
        }
        
        bounce_info->event_type = event_type;
        bounce_info->op = (globus_l_gfs_data_operation_t *) transfer_id;
        
        result = globus_callback_register_oneshot(
            GLOBUS_NULL,
            GLOBUS_NULL,
            globus_l_gfs_data_transfer_event_kickout,
            bounce_info);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusGFSErrorWrapFailed(
                "globus_callback_register_oneshot", result);
            goto error_oneshot;
        }
    }    
    return;
    
error_oneshot:
error_alloc:
    return;  
}

static
void
globus_l_gfs_data_ipc_error_cb(
    globus_gfs_ipc_handle_t             ipc_handle,
    globus_result_t                     result,
    void *                              user_arg)
{
    globus_i_gfs_log_result(
        "IPC ERROR", result);
    
    return;
}

static
void
globus_l_gfs_data_ipc_open_cb(
    globus_gfs_ipc_handle_t             ipc_handle,
    globus_result_t                     result,
    void *                              user_arg)
{
    globus_i_gfs_monitor_t *            monitor;

    monitor = (globus_i_gfs_monitor_t *) user_arg;

    globus_i_gfs_monitor_signal(monitor);
}


globus_result_t
globus_i_gfs_data_node_start(
    globus_xio_handle_t                 handle,
    globus_xio_system_handle_t          system_handle,
    const char *                        remote_contact)
{
    globus_result_t                     res;
    globus_i_gfs_monitor_t              monitor;

    globus_i_gfs_monitor_init(&monitor);
    
    res = globus_gfs_ipc_handle_create(
        &globus_gfs_ipc_default_iface,
        system_handle,
        globus_l_gfs_data_ipc_open_cb,
        &monitor,
        globus_l_gfs_data_ipc_error_cb,
        NULL);

    globus_i_gfs_monitor_wait(&monitor);
    globus_i_gfs_monitor_destroy(&monitor);

    return res;
}

void
globus_i_gfs_data_session_start(
    globus_gfs_ipc_handle_t             ipc_handle,
    int                                 id,
    const char *                        user_dn,
    globus_i_gfs_data_callback_t        cb,
    void *                              user_arg)
{
    globus_l_gfs_data_operation_t *     op;
    globus_result_t                     result;
    globus_gfs_finished_info_t *        finished_info;            
    GlobusGFSName(globus_i_gfs_data_session_start);

    result = globus_l_gfs_data_operation_init(&op);
    if(result != GLOBUS_SUCCESS)
    {
        result = GlobusGFSErrorWrapFailed(
            "globus_i_gfs_data_session_start", result);
        goto error_op;
    }
    
    op->ipc_handle = ipc_handle;
    op->id = id;
    op->uid = getuid();
    
    op->state = GLOBUS_L_GFS_DATA_REQUESTING;
    op->callback = cb;
    op->user_arg = user_arg;
    
    if(globus_l_gfs_dsi->init_func != NULL)
    {
        result = globus_l_gfs_dsi->init_func(op, user_dn);
        if(result != GLOBUS_SUCCESS)
        {
            result = GlobusGFSErrorWrapFailed("hook", result);
            goto error_hook;
        }
    }
    else
    {
        finished_info = (globus_gfs_finished_info_t *)            
            globus_calloc(1, sizeof(globus_gfs_finished_info_t)); 
                                                                  
        finished_info->type = GLOBUS_GFS_OP_SESSION_START;          
        finished_info->session_id = 0;                          
                                                                  
        globus_gridftp_server_operation_finished(                 
            op,                                                   
            GLOBUS_SUCCESS,                                               
            finished_info);                                       
    }    
    globus_mutex_lock(&op->lock);
    {
        if(op->state == GLOBUS_L_GFS_DATA_REQUESTING)
        {
            op->state = GLOBUS_L_GFS_DATA_PENDING;
        }
    }
    globus_mutex_unlock(&op->lock);
    
    return;

error_hook:
    globus_l_gfs_data_operation_destroy(op);
    
error_op:
    return;
}

void
globus_i_gfs_data_session_stop(
    globus_gfs_ipc_handle_t             ipc_handle,
    int                                 session_id)
{
    if(globus_l_gfs_dsi->destroy_func != NULL)
    {
        globus_l_gfs_dsi->destroy_func((void *) session_id);
    }
}
