#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file globus_ftp_client_handle.c FTP Handle Manipulation.
 *
 * $RCSfile$
 * $Revision$
 * $Date$
 */
#endif

#include "globus_i_ftp_client.h"

#include <string.h>


#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
typedef struct
{
    globus_url_t *				url;
    globus_i_ftp_client_operationattr_t *	attr;
    globus_bool_t				want_empty;
}
globus_l_ftp_client_target_search_t;

/* MODULE SPECIFIC PROTOTYPES */
static
globus_object_t *
globus_l_ftp_client_url_parse(
    const char *				url_string,
    globus_url_t *				url);

static
void
globus_l_ftp_client_quit_callback(
    void *					callback_arg,
    globus_ftp_control_handle_t *		handle,
    globus_object_t *				error,
    globus_ftp_control_response_t *		ftp_response);

static
globus_i_ftp_client_target_t *
globus_l_ftp_client_target_new(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr);

static
int
globus_l_ftp_client_compare_canonically(
    void *					datum,
    void *					arg);

static
void
globus_l_ftp_client_target_delete(
    globus_i_ftp_client_target_t *		target);
#endif

/**
 * @name Initialize
 */
/*@{*/
/**
 * Initialize a client FTP handle.
 * @ingroup globus_ftp_client_handle
 *
 * Initialize an FTP handle which can be used in subsequent get, put,
 * or transfer requests. A handle may have at most one get, put, or
 * third-party transfer in progress.
 *
 * @param handle
 *        The handle to be initialized.
 * @param attr
 *        Initial attributes to be used to create this handle.
 *
 * @see globus_ftp_client_handle_destroy()
 */
globus_result_t
globus_ftp_client_handle_init(
    globus_ftp_client_handle_t *		handle,
    globus_ftp_client_handleattr_t *		attr)
{
    static char * myname = "globus_ftp_client_handle_init";
    globus_i_ftp_client_handle_t *		i_handle;
    globus_i_ftp_client_handleattr_t *		i_attr;

    if(handle == GLOBUS_NULL)
    {
	return globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot initialize NULL handle at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname));
					  
    }

    i_handle = globus_libc_malloc(sizeof(globus_i_ftp_client_handle_t));

    if(i_handle == GLOBUS_NULL)
    {
	return globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Out of memory at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname));
    }
    *handle = i_handle;
    i_handle->handle = handle;
    if(attr)
    {
	i_attr = * attr;
    }
    else
    {
	i_attr = GLOBUS_NULL;
    }

    globus_mutex_init(&i_handle->mutex,
		      GLOBUS_NULL);
    globus_i_ftp_client_handle_lock(i_handle);

    strcpy(i_handle->magic, 
	   GLOBUS_FTP_CLIENT_MAGIC_STRING);

    i_handle->source = GLOBUS_NULL;
    i_handle->dest = GLOBUS_NULL;
    i_handle->op = GLOBUS_FTP_CLIENT_IDLE;
    i_handle->err = 0;

    if(attr)
    {
        globus_i_ftp_client_handleattr_copy(&i_handle->attr, i_attr);
    }
    else
    {
	globus_ftp_client_handleattr_t tmp;

	globus_ftp_client_handleattr_init(&tmp);
	i_handle->attr = *(globus_i_ftp_client_handleattr_t *) tmp;
    }
    globus_priority_q_init(&i_handle->stalled_blocks,
			   globus_i_ftp_client_data_cmp);

    globus_hashtable_init(&i_handle->active_blocks,
			  16,
			  globus_hashtable_voidp_hash,
			  globus_hashtable_voidp_keyeq);
    i_handle->pasv_address = 0;
    i_handle->num_pasv_addresses = 0;
    i_handle->num_active_blocks = 0;
    i_handle->restart_info = GLOBUS_NULL;
    i_handle->source_url = GLOBUS_NULL;
    i_handle->dest_url = GLOBUS_NULL;
    i_handle->notify_in_progress = 0;
    i_handle->notify_abort = GLOBUS_FALSE;
    i_handle->notify_restart = GLOBUS_FALSE;
    i_handle->source_size = 0;
    i_handle->read_all_biggest_offset = 0;
    i_handle->base_offset = 0;
    i_handle->user_pointer = GLOBUS_NULL;
    i_handle->partial_offset = -1;
    i_handle->partial_end_offset = -1;
    globus_ftp_client_restart_marker_init(&i_handle->restart_marker);
    i_handle->eret_alg_str = GLOBUS_NULL;
    i_handle->esto_alg_str = GLOBUS_NULL;
    i_handle->modification_time_pointer = GLOBUS_NULL;

    globus_i_ftp_client_handle_unlock(i_handle);

    return GLOBUS_SUCCESS;
}
/* globus_ftp_client_handle_init() */
/*@}*/

/**
 * @name Destroy
 */
/*@{*/
/**
 * Destroy a client FTP handle.
 * @ingroup globus_ftp_client_handle
 *
 * A FTP client handle may not be destroyed if a get, put, or
 * third-party transfer is in progress.
 *
 * @param handle
 *        The handle to be destroyed.
 *
 * @see globus_ftp_client_handle_init()
 */
globus_result_t
globus_ftp_client_handle_destroy(
    globus_ftp_client_handle_t *		handle)
{
    globus_i_ftp_client_handle_t *		i_handle;
    static char * myname = "globus_ftp_client_handle_destroy";

    if(GLOBUS_I_FTP_CLIENT_BAD_MAGIC(handle))
    {
 	return globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot destroy NULL handle at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname));

    }

    i_handle = *(globus_i_ftp_client_handle_t **) handle;

    globus_i_ftp_client_handle_lock(i_handle);
    if(i_handle->op != GLOBUS_FTP_CLIENT_IDLE)
    {
	globus_i_ftp_client_handle_unlock(i_handle);
 	return globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot destroy active handle at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname));
    }
    memset(i_handle->magic, '\0', sizeof(i_handle->magic));

    while(!globus_list_empty(i_handle->attr.url_cache))
    {
	globus_i_ftp_client_cache_entry_t *	cache_entry;

	cache_entry = (globus_i_ftp_client_cache_entry_t *)
	    globus_list_remove(&i_handle->attr.url_cache,
			       i_handle->attr.url_cache);
	if(cache_entry->target)
	{
	    globus_l_ftp_client_target_delete(cache_entry->target);
	}
	globus_url_destroy(&cache_entry->url);
	globus_libc_free(cache_entry);
    }
    
    globus_list_free(i_handle->attr.url_cache);

    if(i_handle->eret_alg_str != GLOBUS_NULL)
    {
        globus_free(i_handle->eret_alg_str);
    }
    if(i_handle->esto_alg_str != GLOBUS_NULL)
    {
        globus_free(i_handle->esto_alg_str);
    }
    if(i_handle->err != GLOBUS_NULL)
    {
        globus_object_free(i_handle->err);
    }

    memset(i_handle->magic, '\0', sizeof(i_handle->magic));
    globus_priority_q_destroy(&i_handle->stalled_blocks);
    globus_hashtable_destroy(&i_handle->active_blocks);
    globus_i_ftp_client_handle_unlock(i_handle);
    globus_mutex_destroy(&i_handle->mutex);

    globus_libc_free(i_handle);

    *handle = GLOBUS_NULL;

    return GLOBUS_SUCCESS;
}
/* globus_ftp_client_handle_destroy() */
/*@}*/

/**
 * @name URL Caching
 */
/*@{*/
/**
 * Cache connections to an FTP server.
 * @ingroup globus_ftp_client_handle
 *
 * Explicitly cache connections to URL server in an FTP handle. When
 * an URL is cached, the client library will not close the connection
 * to the URL server after a file transfer completes.
 *
 * @param handle
 *        Handle which will contain a cached connection to the URL server.
 * @param url
 *        The URL of the FTP or GSIFTP server to cache.
 *
 * @see globus_ftp_client_flush_url_state()
 */
globus_result_t
globus_ftp_client_handle_cache_url_state(
    globus_ftp_client_handle_t *		handle,
    const char *				url)
{
    globus_object_t *				err;
    globus_result_t				result;
    globus_i_ftp_client_handle_t *		i_handle;
    static char * myname = "globus_ftp_client_handle_cache_url_state";

    if(handle == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Cannot cache URL in NULL handle at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);

	goto error;
    }
    if(GLOBUS_I_FTP_CLIENT_BAD_MAGIC(handle))
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Cannot cache URL in uninitialized handle at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);
	goto error;
    }
    i_handle = *(globus_i_ftp_client_handle_t **) handle;
    globus_i_ftp_client_handle_lock(i_handle);

    result = globus_i_ftp_client_cache_add(&i_handle->attr.url_cache,
					   url);

    globus_i_ftp_client_handle_unlock(i_handle);

    return result;

error:
    return globus_error_put(err);
}

/**
 * Remove a cached connection from the FTP client handle.
 * @ingroup globus_ftp_client_handle
 *
 * Explicitly remove a cached connection to an FTP server from the FTP handle.
 * If an idle connection to an FTP server exists, it will be closed.
 *
 * @param handle
 *        Handle which will contain a cached connection to the URL server.
 * @param url
 *        The URL of the FTP or GSIFTP server to cache.
 */
globus_result_t
globus_ftp_client_handle_flush_url_state(
    globus_ftp_client_handle_t *		handle,
    const char *				url)
{
    globus_object_t *				err;
    globus_result_t				result;
    globus_i_ftp_client_handle_t *		i_handle;
    static char * myname = "globus_ftp_client_handle_flush_url_state";

    if(handle == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Cannot flush URL from NULL handle at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);

	goto error;
    }
    if(GLOBUS_I_FTP_CLIENT_BAD_MAGIC(handle))
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Cannot flush URL from uninitialized handle at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);

	goto error;
    }
    i_handle = *(globus_i_ftp_client_handle_t **) handle;
    globus_i_ftp_client_handle_lock(i_handle);

    result = globus_i_ftp_client_cache_remove(&i_handle->attr.url_cache,
					      url);
    globus_i_ftp_client_handle_unlock(i_handle);

    return result;

error:
    return globus_error_put(err);
}
/* globus_ftp_client_handle_flush_url_state() */
/*@}*/

/**
 * @name User Pointer
 */
/*@{*/
/**
 * Set/Get the user pointer field from an ftp client handle.
 * @ingroup globus_ftp_client_handle
 *
 * The user pointer is provided to all the user of the FTP client
 * library to assocate a pointer to any application-specific data to
 * an FTP client handle. This pointer is never internally used by the
 * client library.
 *
 * @param handle
 *        The FTP client handle to set or query.
 * @param user_pointer
 *        The value of the user pointer field.
 *
 * @note Access to the user_pointer are not synchronized, the user
 *       must take care to make sure that multiple threads are not modifying
 *       it's value.
 */
globus_result_t
globus_ftp_client_handle_set_user_pointer(
    globus_ftp_client_handle_t *		handle,
    void *					user_pointer)
{
    globus_object_t *				err;
    globus_i_ftp_client_handle_t *		i_handle;
    static char * myname = "globus_ftp_client_handle_set_user_pointer";

    if(handle == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Cannot set user pointer on a NULL handle at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);

	goto error;
    }
    if(GLOBUS_I_FTP_CLIENT_BAD_MAGIC(handle))
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Cannot set user pointer on an uninitialized handle at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);

	goto error;
    }

    i_handle = *(globus_i_ftp_client_handle_t **) handle;
    i_handle->user_pointer = user_pointer;

    return GLOBUS_SUCCESS;

error:
    return globus_error_put(err);
}
/* globus_ftp_client_handle_set_user_pointer() */

globus_result_t
globus_ftp_client_handle_get_user_pointer(
    const globus_ftp_client_handle_t *		handle,
    void **					user_pointer)
{
    globus_object_t *				err;
    globus_i_ftp_client_handle_t *		i_handle;
    static char * myname = "globus_ftp_client_handle_get_user_pointer";

    if(handle == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Cannot get user pointer from a NULL handle at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);

	goto error;
    }
    else if(user_pointer == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Null user_pointer argument to %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);

	goto error;
    }
    if(GLOBUS_I_FTP_CLIENT_BAD_MAGIC(handle))
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Cannot get user pointer from an uninitialized handle at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);

	goto error;
    }
    i_handle = *(globus_i_ftp_client_handle_t **) handle;
    *user_pointer = i_handle->user_pointer;

    return GLOBUS_SUCCESS;

error:
    return globus_error_put(err);
}
/* globus_ftp_client_handle_get_user_pointer() */
/*@}*/

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * Allocate and initialize a ftp client target.
 *
 * The FTP control state of the target will be set to reflect the
 * defaults of the FTP protocol. The desired attributes will be copied
 * into the target, and the target's state will be set to the starting
 * state.
 *
 * @param handle
 *        The handle that this target will be associated with.
 * @param url
 *        The URL that this target is going to be used for.
 * @param attr 
 *        The attributes to be used when processing the URL.
 *
 * @retval This function returns a pointer to a newly initialized
 *	   target structure, or GLOBUS_NULL, if an error occurred.
 */
static
globus_i_ftp_client_target_t *
globus_l_ftp_client_target_new(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr)
{
    globus_i_ftp_client_target_t *		target;
    globus_result_t				result;
    globus_object_t *				err;
    int						i;

    target = globus_libc_malloc(sizeof(globus_i_ftp_client_target_t));

    if(!target)
    {
	goto error_exit;
    }
    target->owner = handle;
    target->control_handle = (globus_ftp_control_handle_t *)
	globus_libc_malloc(sizeof(globus_ftp_control_handle_t));
    if(!target->control_handle)
    {
	goto free_target;
    }
    result = globus_ftp_control_handle_init(target->control_handle);
    globus_ftp_control_set_netlogger(
        target->control_handle,
        handle->attr.nl_handle);

    
    if(result)
    {
	goto free_control_handle;
    }
    target->url_string = globus_libc_strdup(url);

    if(!target->url_string)
    {
	goto destroy_control_handle;
    }
    err = globus_l_ftp_client_url_parse(url,
					&target->url);
    if(err)
    {
	globus_object_free(err);
	goto free_url_string;
    }
    /* Be noncommittal for now for SITE HELP and FEAT options. */
    for(i = 0; i < GLOBUS_FTP_CLIENT_FEATURE_MAX; i++)
    {
	target->features[i] = GLOBUS_FTP_CLIENT_MAYBE;
    }
    /*
     * Setup default setttings on the control handle values. We'll
     * adjust these to match the desired attributes after we've made
     * the connection. 
     */
    target->type = GLOBUS_FTP_CONTROL_TYPE_ASCII;
    target->dcau.mode = GLOBUS_FTP_CONTROL_DCAU_NONE;
    target->dcau.subject.subject = GLOBUS_NULL;
    target->tcp_buffer.mode = GLOBUS_FTP_CONTROL_TCPBUFFER_DEFAULT;
    target->tcp_buffer.fixed.size = 0;
    target->mode = GLOBUS_FTP_CONTROL_MODE_STREAM;
    target->structure = GLOBUS_FTP_CONTROL_STRUCTURE_NONE;
    target->layout.mode = GLOBUS_FTP_CONTROL_STRIPING_NONE;
    target->parallelism.mode = GLOBUS_FTP_CONTROL_PARALLELISM_NONE;
    target->data_prot = GLOBUS_FTP_CONTROL_PROTECTION_CLEAR;

    target->cached_data_conn.source = GLOBUS_NULL;
    target->cached_data_conn.dest = GLOBUS_NULL;
    target->cached_data_conn.operation = GLOBUS_FTP_CLIENT_IDLE;

    /* Make a local copy of the desired FTP client attributes */
    if(attr)
    {
	result = globus_ftp_client_operationattr_copy(&target->attr,
						      &attr);
	if(result)
	{
	    goto free_url;
	}
    }
    else
    {
	result = globus_ftp_client_operationattr_init(&target->attr);
	if(result)
	{
	    goto free_url;
	}
    }

    /* We bind the authentication state right away, however */
    if(target->url.scheme_type != GLOBUS_URL_SCHEME_GSIFTP)
    {
	result = globus_ftp_control_auth_info_init(&target->auth_info,
						   GSS_C_NO_CREDENTIAL,
						   GLOBUS_FALSE,
						   0,
						   0,
						   0,
						   0);
	if(result)
	{
	    goto destroy_attr;
	}

	result = 
	    globus_ftp_client_operationattr_get_authorization(
		&target->attr,
		&target->auth_info.credential_handle,
		&target->auth_info.user,
		&target->auth_info.password,
		&target->auth_info.account,
		&target->auth_info.auth_gssapi_subject);

	if(result)
	{
	    goto destroy_attr;
	}
    }
    else
    {
	/* Override default authorization attributes
	 * (anonymous/globus@ to be :globus-mapping:/
	 */
	if(target->attr->using_default_auth)
	{
	    result =
		globus_ftp_client_operationattr_set_authorization(
		    &target->attr,
		    GSS_C_NO_CREDENTIAL,
		    ":globus-mapping:",
		    "",
		    0,
		    0);
	    if(result)
	    {
		goto destroy_attr;
	    }
	}
	result =
	    globus_ftp_client_operationattr_get_authorization(
		&target->attr,
		&target->auth_info.credential_handle,
		&target->auth_info.user,
		&target->auth_info.password,
		&target->auth_info.account,
		&target->auth_info.auth_gssapi_subject);

	if(result)
	{
	    goto destroy_attr;
	}
    }
    
    /* Set the state of the new handle to the disconnected state */
    target->state = GLOBUS_FTP_CLIENT_TARGET_START;
    target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_NONE;

    return target;

destroy_attr:
    globus_ftp_client_operationattr_destroy(&target->attr);
free_url:
    globus_url_destroy(&target->url);
free_url_string:
    globus_libc_free(target->url_string);
destroy_control_handle:
    globus_ftp_control_handle_destroy(target->control_handle);
free_control_handle:
    globus_libc_free(target->control_handle);
free_target:
    globus_libc_free(target);
error_exit:
    return GLOBUS_NULL;
}
/* globus_l_ftp_client_target_new() */

/**
 * Free an ftp client target.
 *
 * Normally, this is done when the target is released, or a cache
 * entry is flushed. However, if the target was cached, and died
 * before the client library used it again, it will be called from the
 * callback of the failed NOOP.
 *
 * @param target
 *        Target previously allocated by globus_l_ftp_client_target_new().
 */
static
void
globus_l_ftp_client_target_delete(
    globus_i_ftp_client_target_t *		target)
{
    globus_result_t				result = GLOBUS_SUCCESS;
    globus_bool_t				connected;

    globus_ftp_client_operationattr_destroy(&target->attr);
    target->owner = GLOBUS_NULL;

    connected = (target->state != GLOBUS_FTP_CLIENT_TARGET_START ||
		 target->state != GLOBUS_FTP_CLIENT_TARGET_CLOSED);

    globus_i_ftp_client_control_is_active(target->control_handle);

    if(connected)
    {
	result = globus_ftp_control_quit(target->control_handle,
					 globus_l_ftp_client_quit_callback,
					 target);
    }
    if(result != GLOBUS_SUCCESS)
    {
	result = globus_ftp_control_force_close(target->control_handle,
						globus_l_ftp_client_quit_callback,
						target);
    }
	
    if(target->url_string)
    {
	globus_libc_free(target->url_string);
    }

    globus_url_destroy(&target->url);

    if(target->auth_info.user)
    {
	globus_libc_free(target->auth_info.user);
    }
    if(target->auth_info.password)
    {
	globus_libc_free(target->auth_info.password);
    }
    if(target->auth_info.account)
    {
	globus_libc_free(target->auth_info.account);
    }
    if((!connected) || result != GLOBUS_SUCCESS)
    {
	/* Pretend we closed, and free the control handle. */
	globus_l_ftp_client_quit_callback(target,
					  target->control_handle,
					  GLOBUS_SUCCESS,
					  (void*) 0);
    }
}
/* globus_l_ftp_client_target_delete() */

/**
 * Put an FTP target back into the cache or free it.
 *
 * @param handle
 *        The handle this target was associated
 * @param target
 *        The target to release
 */
void
globus_i_ftp_client_target_release(
    globus_i_ftp_client_handle_t *		handle,
    globus_i_ftp_client_target_t *		target) 
{
    globus_i_ftp_client_cache_entry_t *		cache_entry;
    globus_list_t *				node;
    globus_l_ftp_client_target_search_t         searcher;

    /* 
     * Remove the target mapping from the client handle's active
     * targets
     */
    if(handle->source == target)
    {
	handle->source = 0;
    }
    if(handle->dest == target)
    {
	handle->dest = 0;
    }
    if(target->state == GLOBUS_FTP_CLIENT_TARGET_COMPLETED_OPERATION)
    {
	target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;
    }

    globus_i_ftp_client_control_is_not_active(target->control_handle);

    searcher.url = &target->url;
    searcher.attr = target->attr;
    searcher.want_empty = GLOBUS_TRUE;

    /* Check to see if we should cache this target */
    if(target->state == GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION)
    {
	node = globus_list_search_pred(handle->attr.url_cache,
				       globus_l_ftp_client_compare_canonically,
				       &searcher);
	target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;

	if(node)
	{
	    cache_entry = (globus_i_ftp_client_cache_entry_t *) node->datum;
	    
	    if(cache_entry->target == GLOBUS_NULL)
	    {
		cache_entry->target = target;
	    }
	    return;
	}
    }

    globus_l_ftp_client_target_delete(target);
}
/* globus_i_ftp_client_target_release() */

/**
 * Allocate or reuse an FTP target to handle this operation.
 *
 * This function will either find a cached target and control
 * connection, or allocate a new one. This function returns the
 * target which should be used for the operation currently associated
 * with this handle.
 *
 * The target allocated by this function must be returned to the cache
 * or freed by calling globus_i_ftp_client_target_release().
 *
 * @note This function @a must be called with the client library
 * mutex locked.
 *
 * @param handle
 *        The handle containing the URL cache.
 * @param url
 *        The string representation of the URL we will access.
 * @param attr
 *        The attributes to be used to access the URL.
 * @param target
 *        A pointer to be set to the new target.
 */
globus_object_t *
globus_i_ftp_client_target_find(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr,
    globus_i_ftp_client_target_t **		target)
{
    globus_url_t				parsed_url;
    globus_object_t *				err;
    globus_list_t *				node;
    globus_i_ftp_client_cache_entry_t *		cache_entry;
    int						rc;
    globus_l_ftp_client_target_search_t         searcher;
    static char * myname = "globus_i_ftp_client_target_find";

    globus_assert(handle);
    globus_assert(url);

    /* Parse and verify the URL */
    err = globus_l_ftp_client_url_parse(url,
					&parsed_url);
    if(err != GLOBUS_SUCCESS)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    err,
	    "[%s] Cannot construct target for invalid URL at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);

	goto error_exit;
    }

    (*target) = GLOBUS_NULL;

    /* Search the cache for a target which matches this URL. */
    searcher.url = &parsed_url;
    searcher.attr = attr;
    searcher.want_empty = GLOBUS_FALSE;

    node = globus_list_search_pred(handle->attr.url_cache,
				   globus_l_ftp_client_compare_canonically,
				   &searcher);
    if(node)
    {
	cache_entry = (globus_i_ftp_client_cache_entry_t *) node->datum;
	if(cache_entry->target)
	{
	    (*target) = cache_entry->target;
	    cache_entry->target = GLOBUS_NULL;
	}
    }
    else if(handle->attr.cache_all)
    {
	globus_i_ftp_client_cache_add(&handle->attr.url_cache,
				      url);
    }
    if((*target) == GLOBUS_NULL)
    {
	/*
	 * Didn't find a cached connection, so we will create a new
	 * target
	 */	
	(*target) = globus_l_ftp_client_target_new(handle,
						   url,
						   attr);
    }
    else /* found copy in cache... update attrs, url */
    {
	globus_result_t				result;
	if(attr)
	{
	    globus_ftp_client_operationattr_destroy(&(*target)->attr);
	    result = globus_ftp_client_operationattr_copy(&(*target)->attr,
							  &attr);
	    if(result)
	    {
		err = globus_error_get(result);
		goto free_target;
	    }
	}
	else
	{
	    result = globus_ftp_client_operationattr_init(&(*target)->attr);
	    if(result)
	    {
		err = globus_error_get(result);
		goto free_target;
	    }
	}
	(*target)->url_string = globus_libc_strdup(url);
	if(!(*target)->url_string)
	{
	    err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Out of memory at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	    goto free_target;
	}
	globus_url_destroy(&(*target)->url);
	rc = globus_url_parse(url, &(*target)->url);
	if(rc)
	{
	    err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		err,
		"[%s] Cannot construct target at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);
	    goto free_target;
	}
	
    }
    
    if((*target) == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    err,
	    "[%s] Cannot construct target at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);
	goto free_url;
    }
    globus_url_destroy(&parsed_url);

    globus_i_ftp_client_control_is_active((*target)->control_handle);

    return GLOBUS_SUCCESS;

    /* Exception handling */
free_target:
    if(*target)
    {
	globus_l_ftp_client_target_delete(*target);
    }
free_url:
    globus_url_destroy(&parsed_url);
error_exit:
    return err;
}
/* globus_i_ftp_client_target_find() */

/**
 * Parse and check the given URL string.
 *
 * This function parses the url_string parameter and verifies that is
 * an FTP-type URL.
 *
 * @param url_string
 *        The string to parse.
 * @param url
 *        A pointer to the URL structure to contain the parsed URL.
 *
 * @retval GLOBUS_SUCCESS
 *         The url_string was successfully parsed, and is an FTP
 *	   URL. As a side effect, the url structure will be populated,
 *         and must be destroyed by an explicit call to globus_url_destroy().
 */
static
globus_object_t *
globus_l_ftp_client_url_parse(
    const char *				url_string,
    globus_url_t *				url)
{
    int rc;
    globus_object_t *				err = GLOBUS_NULL;
    static char * myname = "globus_l_ftp_client_url_parse";

    /* Try to parse url */
    rc = globus_url_parse(url_string,
			  url);
    if(rc != GLOBUS_SUCCESS)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    err,
	    "[%s] URL parser returned %d while parsing %s at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    rc,
	    url_string,
	    myname);
	return err;
    }

    /* 
     * Make sure that the URL is of a proper type (ftp or gsiftp) 
     * and make  that the port number is valid.
     */
    if(url->scheme_type == GLOBUS_URL_SCHEME_FTP)
    {
	if(url->port == 0)
	{
	    url->port = 21;	/* IANA-defined FTP control port */
	}
    } 
    else if(url->scheme_type == GLOBUS_URL_SCHEME_GSIFTP)
    {
	if(url->port == 0)
	{
	    url->port = 2811;	/* IANA-defined GSIFTP control port*/
	}
    }
    else
    {
	/* Unsupported URL scheme */
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    err,
	    "[%s] The URL scheme %s is not supported at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    url->scheme,
	    myname);
	globus_url_destroy(url);

	return err;
    }
    return GLOBUS_SUCCESS;
}
/* globus_l_ftp_client_url_parse() */

/**
 * Complete the processing of the quit for a client target.
 *
 * This callback is invoked once the quit or force_close is completed,
 * and the control handle is no longer associated with a target.
 * This function simply destroys and freeds the control handle.
 *
 * @param callback_arg
 *        The target which we are closing
 * @param handle
 *        The ftp client handle associated with the target.
 * @param error
 *        Error object or GLOBUS_SUCCESS from the control library.
 * @param ftp_response
 *        The FTP server's response to the QUIT, or GLOBUS_NULL
 *        if there is none.
 */
static
void
globus_l_ftp_client_quit_callback(
    void *					callback_arg,
    globus_ftp_control_handle_t *		handle,
    globus_object_t *				error,
    globus_ftp_control_response_t *		ftp_response)
{
    globus_result_t				result;
    globus_i_ftp_client_target_t *		target;
 
    target = (globus_i_ftp_client_target_t * ) callback_arg;

    if(error != GLOBUS_SUCCESS)
    {
	result = globus_ftp_control_force_close(handle, 
						globus_l_ftp_client_quit_callback,
						callback_arg);
	if(result != GLOBUS_SUCCESS)
	{
	    globus_i_ftp_client_control_is_not_active(target->control_handle);

	    globus_libc_free(target);
	}
    }
    else
    {
	result = globus_ftp_control_handle_destroy(handle);
	if(target)
	{
	    globus_i_ftp_client_control_is_not_active(target->control_handle);
	}

	if(result == GLOBUS_SUCCESS)
	{
	    globus_libc_free(handle);
	    globus_libc_free(target);
	}
	/* Else --> leak! */
    }
}
/* globus_l_ftp_client_quit_callback() */

/**
 * Comparison predicate for searching the URL cache.
 *
 * This function is called by globus_list_search_pred to locate
 * a cache entry node which matches the URL. Matching is done on
 * the URL scheme, host, and port, but the path is ignored.
 *
 * @param datum
 *        A pointer to a #globus_i_ftp_client_cache_entry_t stored
 *        in a cache list.
 * @param arg
 *        A pointer to a #globus_l_ftp_client_target_search_t containing
 *        the desired target URL and attributes.
 *
 * @todo Add control channel protection matching.
 */
static
int
globus_l_ftp_client_compare_canonically(
    void *					datum,
    void *					arg)
{
    globus_l_ftp_client_target_search_t *	key;
    globus_i_ftp_client_cache_entry_t *		cache_entry;

    key = (globus_l_ftp_client_target_search_t *) arg;
    cache_entry = (globus_i_ftp_client_cache_entry_t *) datum;

    if(cache_entry->url.scheme_type == key->url->scheme_type &&
       cache_entry->url.port == key->url->port &&
       (strcmp(cache_entry->url.host, key->url->host) == 0))
    {
        if(cache_entry->target && !key->want_empty)
	{
	    if(globus_ftp_control_auth_info_compare(
	           &cache_entry->target->attr->auth_info,
		   &key->attr->auth_info) == 0)
            {
	        return GLOBUS_TRUE;
            }
	    else if(key->attr->using_default_auth &&
	            key->url->scheme_type == GLOBUS_URL_SCHEME_GSIFTP &&
		    globus_ftp_control_auth_info_compare(
		        &cache_entry->target->attr->auth_info,
			&globus_i_ftp_client_default_auth_info) == 0)
	    {
	        return GLOBUS_TRUE;
	    }
	}
	else
	{
	    return GLOBUS_TRUE;
	}
    }
    return GLOBUS_FALSE;
}
/* globus_l_ftp_client_compare_canonically() */

/**
 * Delete a globus_i_ftp_client_restart_info_t structure.
 *
 * This function deletes the data fields of a
 * globus_i_ftp_client_restart_info_t structure, and then frees the structure
 * itself.
 *
 * @param restart_info
 *        The structure to free.
 */
void
globus_i_ftp_client_restart_info_delete(
    globus_i_ftp_client_restart_t *		restart_info)
{
    if(restart_info->source_url)
    {
	globus_libc_free(restart_info->source_url);
	restart_info->source_url = GLOBUS_NULL;
    }
    if(restart_info->source_attr)
    {
	globus_ftp_client_operationattr_destroy(&restart_info->source_attr);
	globus_libc_free(restart_info->source_attr);
	restart_info->source_attr = GLOBUS_NULL;
    }
    if(restart_info->dest_url)
    {
	globus_libc_free(restart_info->dest_url);
	restart_info->dest_url = GLOBUS_NULL;
    }
    if(restart_info->dest_attr)
    {
	globus_ftp_client_operationattr_destroy(&restart_info->dest_attr);
	globus_libc_free(restart_info->dest_attr);
	restart_info->dest_attr = GLOBUS_NULL;
    }
    globus_libc_free(restart_info);
}

/**
 *
 * Determine whether the client handle can re-use a data connection.
 *
 * Data connection caching is possible when the previous data connection
 * associated with the URL or URLs involved in the data transfer was done
 * in extended block mode with compatible attributes.
 *
 * Currently, data connection caching is only supported for GET, PUT,
 * and 3rd party transfers.
 *
 * @param client_handle
 *        The handle to check.
 *
 * @todo When DCAU is implemented, add checks for that here as well.
 */
globus_bool_t
globus_i_ftp_client_can_reuse_data_conn(
    globus_i_ftp_client_handle_t *		client_handle)
{
    globus_i_ftp_client_target_t *		source;
    globus_i_ftp_client_target_t *		dest;

    source = client_handle->source;
    dest = client_handle->dest;

    switch(client_handle->op)
    {
    case GLOBUS_FTP_CLIENT_GET:
	if(source == source->cached_data_conn.source &&
	   source->mode == GLOBUS_FTP_CONTROL_MODE_EXTENDED_BLOCK &&
	   source->cached_data_conn.operation == client_handle->op)
	{
	    return GLOBUS_TRUE;
	}
	break;
    case GLOBUS_FTP_CLIENT_PUT:
	if(dest == dest->cached_data_conn.dest &&
	   dest->mode == GLOBUS_FTP_CONTROL_MODE_EXTENDED_BLOCK &&
	   dest->cached_data_conn.operation == client_handle->op)
	{
	    return GLOBUS_TRUE;
	}
	break;
    case GLOBUS_FTP_CLIENT_TRANSFER:
	if(source == source->cached_data_conn.source &&
	   source == dest->cached_data_conn.source &&
	   dest == source->cached_data_conn.dest &&
	   dest == dest->cached_data_conn.dest &&
	   dest->cached_data_conn.operation == client_handle->op &&
	   source->cached_data_conn.operation == client_handle->op &&
	   source->mode == GLOBUS_FTP_CONTROL_MODE_EXTENDED_BLOCK &&
	   source->attr->mode == GLOBUS_FTP_CONTROL_MODE_EXTENDED_BLOCK &&
	   dest->mode == GLOBUS_FTP_CONTROL_MODE_EXTENDED_BLOCK &&
	   dest->attr->mode == GLOBUS_FTP_CONTROL_MODE_EXTENDED_BLOCK)
	{
	    return GLOBUS_TRUE;
	}
	break;
    default:
	;
    }
    return GLOBUS_FALSE;
}
/* globus_i_ftp_client_can_reuse_data_conn() */

/**
 * Add a URL to a cache list.
 *
 * This function is used to add a URL to an attribute or handle's URL
 * cache. Currently, it will only add one instance of a URL to a cache.
 *
 * @param cache
 *        A pointer to the URL cache.
 * @param url
 *        The URL string to cache.
 */
globus_result_t
globus_i_ftp_client_cache_add(
    globus_list_t **				cache,
    const char *				url)
{
    globus_object_t *				err;
    globus_url_t				parsed_url;
    globus_list_t *				list_node;
    globus_i_ftp_client_cache_entry_t *		cache_entry;
    globus_l_ftp_client_target_search_t         searcher;
    static char * myname = "globus_i_ftp_client_cache_add";

    if(url == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Cannot cache NULL URL at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);
	goto error;
    }
    err = globus_l_ftp_client_url_parse(url,
					&parsed_url);
    if(err != GLOBUS_SUCCESS)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    err,
	    "[%s] Cannot cache invalid URL at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);
	goto error;
    }

    searcher.url = &parsed_url;
    searcher.attr = GLOBUS_NULL;
    searcher.want_empty = GLOBUS_TRUE;

    list_node = 
	globus_list_search_pred(*cache,
				globus_l_ftp_client_compare_canonically,
				&searcher);
    if(list_node)
    {
	/* Already in cache */
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    err,
	    "[%s] Cannot cache already-cached URL at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);
	goto free_error;
    }
    cache_entry =
	globus_libc_malloc(sizeof(globus_i_ftp_client_cache_entry_t));

    memcpy(&cache_entry->url,
	   &parsed_url,
	   sizeof(parsed_url));

    cache_entry->target = GLOBUS_NULL;

    globus_list_insert(cache,
		       cache_entry);

    return GLOBUS_SUCCESS;

 free_error:
    globus_url_destroy(&parsed_url);
    
 error:
    return globus_error_put(err);
}
/* globus_i_ftp_client_cache_add() */

/**
 * Remove a URL from a cache list.
 *
 * This function is used to remove a URL from an attribute or handle's URL
 * cache. 
 *
 * @param cache
 *        A pointer to the URL cache.
 * @param url
 *        The URL string to cache.
 */
globus_result_t
globus_i_ftp_client_cache_remove(
    globus_list_t **				cache,
    const char *				url)
{
    globus_object_t *				err;
    globus_url_t				parsed_url;
    globus_list_t *				node;
    globus_i_ftp_client_cache_entry_t *		cache_entry;
    globus_l_ftp_client_target_search_t         searcher;
    static char * myname = "globus_i_ftp_client_cache_remove";

    if(url == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Cannot flush NULL URL from handle at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);

	goto error;
    }
    err = globus_l_ftp_client_url_parse(url,
					&parsed_url);
    if(err != GLOBUS_SUCCESS)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    err,
	    "[%s] Cannot flush invalid URL from handle at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);

	goto error;
    }

    searcher.want_empty = GLOBUS_TRUE;
    searcher.url = &parsed_url;
    searcher.attr = GLOBUS_NULL;
    do
    {

        node =
	    globus_list_search_pred(*cache,
				    globus_l_ftp_client_compare_canonically,
				    &parsed_url);
        if(node)
        {
	    cache_entry = (globus_i_ftp_client_cache_entry_t *) node->datum;
	    globus_list_remove(cache, node);
	    if(cache_entry->target)
	    {
	        globus_l_ftp_client_target_delete(cache_entry->target);
	    }
	    globus_url_destroy(&cache_entry->url);
	    globus_libc_free(cache_entry);
        }
        else
        {
	    searcher.want_empty = !searcher.want_empty;
        }
    } while(node || searcher.want_empty);

    return GLOBUS_SUCCESS;

 error:
    return globus_error_put(err);
}
/* globus_i_ftp_client_cache_remove() */

/**
 * Destroy a URL cache.
 *
 * This function removes all URLs from teh cache, and closes any cached
 * connections associated with those URLs.
 *
 * @param cache
 *        The cache to destroy.
 */
globus_result_t
globus_i_ftp_client_cache_destroy(
    globus_list_t **				cache)
{
    globus_i_ftp_client_cache_entry_t *		cache_entry;

    while(!globus_list_empty(*cache))
    {
	cache_entry = (globus_i_ftp_client_cache_entry_t *)
	    globus_list_remove(cache, *cache);
	if(cache_entry->target)
	{
	    globus_l_ftp_client_target_delete(cache_entry->target);
	}
	globus_url_destroy(&cache_entry->url);
	globus_libc_free(cache_entry);
    }
    return GLOBUS_SUCCESS;
}
/* globus_i_ftp_client_cache_destroy() */
#endif
