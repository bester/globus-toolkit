/**
 * @file globus_ftp_control_client.c
 *
 * Client-side FTP Control API.
 */

#include "globus_ftp_control.h"
#include "globus_i_ftp_control.h"
#include <string.h>
#include <ctype.h>
#include <netinet/in.h>


/* Local variable declarations */

static globus_list_t * globus_l_ftp_cc_handle_list = GLOBUS_NULL;
static globus_mutex_t  globus_l_ftp_cc_handle_list_mutex;

/* Local function declarations */

static void 
globus_l_ftp_control_send_cmd_cb(
    void *                                    callback_arg,
    globus_ftp_control_handle_t *             handle,
    globus_object_t *			      error,
    globus_ftp_control_response_t *	      ftp_response);

static void 
globus_l_ftp_control_data_close_cb(
    void *                                      arg,
    struct globus_ftp_control_handle_s *        handle,
    globus_object_t *				error);

static void 
globus_l_ftp_control_close_cb(
    void *                                    arg, 
    globus_io_handle_t *                      handle,
    globus_result_t                           result);

static void 
globus_l_ftp_control_connect_cb(
    void *                                    arg, 
    globus_io_handle_t *                      handle,
    globus_result_t                           result);

static void 
globus_l_ftp_control_read_cb(
    void *                                    arg, 
    globus_io_handle_t *                      handle,
    globus_result_t                           result,
    globus_byte_t *                           buf, 
    globus_size_t                             nbytes);

static void 
globus_l_ftp_control_write_cb(
    void *                                    arg, 
    globus_io_handle_t *                      handle,
    globus_result_t                           result,
    globus_byte_t *                           buf, 
    globus_size_t                             nbytes);

static globus_result_t 
globus_l_ftp_control_response_init(
    globus_ftp_control_response_t *        response);

static int 
globus_l_ftp_control_end_of_reply(
    globus_ftp_cc_handle_t *               cc_handle);

static void 
globus_l_ftp_control_read_next(
    globus_ftp_control_handle_t *             handle);

static globus_result_t
globus_l_ftp_control_queue_element_init(
    globus_ftp_control_rw_queue_element_t *     element,
    globus_ftp_control_response_callback_t	callback,
    void *					arg,
    globus_byte_t *                             write_buf,
    int                                         write_flags,
    globus_io_write_callback_t                  write_callback,
    globus_io_read_callback_t                   read_callback,
    globus_bool_t                               expect_response,
    globus_bool_t                               use_auth,
    globus_ftp_control_handle_t *               handle);

/**
 * Initialize a globus ftp handle
 *
 * This function will set up (i.e. intialize all mutexes and
 * variables) a globus ftp handle. It will also enter the handle in a
 * list used by the module activation/deactivation functions. 
 *
 * @param handle
 *        The handle to initialize.
 * @return
 *        - GLOBUS_SUCCESS
 *        - error object
 */

globus_result_t 
globus_ftp_control_handle_init(
    globus_ftp_control_handle_t *          handle)
{
    globus_result_t                        rc;

    if(handle == GLOBUS_NULL)
    {
	return globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_ftp_control_handle_init: Handle argument is NULL")
	    );
    }

    /* control_client.c specific init */

    globus_fifo_init(&handle->cc_handle.readers);
    globus_fifo_init(&handle->cc_handle.writers);
    globus_l_ftp_control_response_init(
	&(handle->cc_handle.response));
    handle->cc_handle.use_auth = GLOBUS_FALSE;
    handle->cc_handle.cc_state = GLOBUS_FTP_CONTROL_UNCONNECTED;
    handle->cc_handle.command_cb = GLOBUS_NULL;
    handle->cc_handle.command_cb_arg = GLOBUS_NULL;
    handle->cc_handle.auth_cb = GLOBUS_NULL;
    handle->cc_handle.auth_cb_arg = GLOBUS_NULL;
    handle->cc_handle.cb_count = 0;
    handle->cc_handle.close_cb = GLOBUS_NULL;
    handle->cc_handle.close_cb_arg = GLOBUS_NULL;
    handle->cc_handle.close_result = GLOBUS_NULL;
    handle->cc_handle.quit_response.response_buffer = GLOBUS_NULL;
    handle->cc_handle.signal_deactivate = GLOBUS_FALSE;
    handle->cc_handle.nl_handle_set = GLOBUS_FALSE;
    globus_cond_init(&handle->cc_handle.cond, GLOBUS_NULL);
    
    globus_io_tcpattr_init(&handle->cc_handle.io_attr);

    globus_ftp_control_auth_info_init(&(handle->cc_handle.auth_info),
				      GSS_C_NO_CREDENTIAL,
				      GLOBUS_FALSE,
				      GLOBUS_NULL,
				      GLOBUS_NULL,
				      GLOBUS_NULL,
				      GLOBUS_NULL);
    
    globus_mutex_init(&(handle->cc_handle.mutex), GLOBUS_NULL);
    
    handle->cc_handle.read_buffer = (globus_byte_t *) globus_libc_malloc(
	GLOBUS_FTP_CONTROL_READ_BUFFER_SIZE);

    if(handle->cc_handle.read_buffer == GLOBUS_NULL)
    {
	return globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_ftp_control_handle_init: malloc failed")
	    );
    }

    handle->cc_handle.read_buffer_size = GLOBUS_FTP_CONTROL_READ_BUFFER_SIZE;
    handle->cc_handle.bytes_read = 0;

    globus_mutex_lock(&globus_l_ftp_cc_handle_list_mutex);
    {
	globus_list_insert(&globus_l_ftp_cc_handle_list,
			   &(handle->cc_handle));
	handle->cc_handle.list_elem=globus_l_ftp_cc_handle_list;
    }
    globus_mutex_unlock(&globus_l_ftp_cc_handle_list_mutex);

    /* control_data.c specific init */

    rc = globus_i_ftp_control_data_cc_init(handle);

    if(rc != GLOBUS_SUCCESS)
    {
	globus_libc_free(handle->cc_handle.read_buffer);
	return rc;
    }

    return GLOBUS_SUCCESS;
}

/**
 * Destroy a globus ftp handle
 *
 * This function will free up all dynamicly allocated  memory
 * associated with a given  globus ftp handle. It will also remove the
 * handle from a list used by the module activation/deactivation
 * functions. This function should only be called after a call to
 * either globus_ftp_control_force_close or globus_ftp_control_quit.  
 *
 * @param handle
 *        The handle to destory.
 * @return
 *        - success
 *        - invalid handle
 *        - handle is still in connected state
 */

globus_result_t 
globus_ftp_control_handle_destroy(
    globus_ftp_control_handle_t *         handle)
{
    void *                                result;

    if(handle == GLOBUS_NULL)
    {
	return globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_ftp_control_handle_destroy: Null handle argument")
	    );
    }

    if(handle->cc_handle.cc_state != 
       GLOBUS_FTP_CONTROL_UNCONNECTED)
    {
	return globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_ftp_control_handle_destroy: Handle still connected")
	    );
    }
    
    /* control_client.c specific destroy */

    globus_mutex_lock(&globus_l_ftp_cc_handle_list_mutex);
    {
	result = globus_list_remove(&globus_l_ftp_cc_handle_list,
				    handle->cc_handle.list_elem);
    }
    globus_mutex_unlock(&globus_l_ftp_cc_handle_list_mutex);

    if(result != GLOBUS_NULL)
    {
	globus_i_ftp_control_auth_info_destroy(
	    &(handle->cc_handle.auth_info));
	globus_ftp_control_response_destroy(
	    &handle->cc_handle.response);
	globus_ftp_control_response_destroy(
	    &handle->cc_handle.quit_response);
	globus_mutex_destroy(&(handle->cc_handle.mutex));
	globus_cond_destroy(&(handle->cc_handle.cond));
	globus_libc_free(handle->cc_handle.read_buffer);

        globus_io_tcpattr_destroy(&handle->cc_handle.io_attr);
        if(handle->cc_handle.nl_handle_set)
        {
            globus_netlogger_handle_destroy(&handle->cc_handle.nl_handle);
        }

	if(handle->cc_handle.close_result != GLOBUS_SUCCESS)
	{
	    globus_object_free(handle->cc_handle.close_result);
	}
    
	/* control_data.c specific destroy */

	return globus_i_ftp_control_data_cc_destroy(handle);
    }

    return GLOBUS_SUCCESS;
}

globus_result_t
globus_i_ftp_control_client_set_netlogger(
    globus_ftp_control_handle_t *               handle,
    globus_netlogger_handle_t *                 nl_handle)
{
    if(handle == GLOBUS_NULL)
    {
        return globus_error_put(
            globus_error_construct_string(
                GLOBUS_FTP_CONTROL_MODULE,
                GLOBUS_NULL,
                "globus_ftp_control_handle_destroy: Null handle argument")
            );
    }

    if(nl_handle == GLOBUS_NULL)
    {
        return globus_error_put(
            globus_error_construct_string(
                GLOBUS_FTP_CONTROL_MODULE,
                GLOBUS_NULL,
                "globus_ftp_control_handle_destroy: Null nl_handle argument")
            );
    }

    globus_mutex_lock(&(handle->cc_handle.mutex));
    {
        globus_io_attr_netlogger_copy_handle(nl_handle, 
            &handle->cc_handle.nl_handle);

        globus_netlogger_set_desc(
            &handle->cc_handle.nl_handle,
            "FTP_CONTROL");

        globus_io_attr_netlogger_set_handle(
                &handle->cc_handle.io_attr,
                &handle->cc_handle.nl_handle);
    }
    globus_mutex_unlock(&(handle->cc_handle.mutex));

    return GLOBUS_SUCCESS;
}


/**
 * Create a new control connection to an FTP server.
 *
 * This function is used to initiate an FTP control connection. It
 * creates the socket to the FTP server. When the connection is made 
 * to the server, and the server's identification string is received,
 * the callback function will be invoked.
 * 
 * @param handle
 *        A pointer to a initialized FTP control handle. This handle
 *        will be used for all subsequent FTP control operations. 
 * @param host
 *        The hostname of the FTP server.
 * @param port
 *        The TCP port number of the FTP server.
 * @param callback
 *        A function to be called once the connection to the server is
 *        established, and a response has been read. 
 * @param callback_arg
 *        Parameter to the callback function.
 * @return
 *        - success
 *        - Null handle
 *	  - Null host 
 *        - Illegal port number
 *        - Null callback
 *        - Cannot resolve hostname
 *        - Cannot create socket
 *
 * @par Callback errors:
 *        - success
 *        - connection refused
 *        - protocol error
 *        - eof
 *
 * @par Expected callback response values:
 *        - 120 Service ready in nnn minutes.
 *        - 220 Service ready for new user.
 *        - 421 Service not available, closing control connection.
 *        - 500 Syntax error, command unrecognized.
 *
 * @note The server may send other responses.
 */

globus_result_t
globus_ftp_control_connect(
    globus_ftp_control_handle_t *		handle,
    char *				        host,
    unsigned short				port,
    globus_ftp_control_response_callback_t	callback,
    void *					callback_arg)
{
    globus_result_t                             rc;
    globus_io_attr_t                            attr;
    globus_bool_t                               call_close_cb = GLOBUS_FALSE;
    int                                         errno;
    int                                         result;
    char                                        localhost[MAXHOSTNAMELEN];
    struct hostent *                            hp;
    globus_ftp_control_rw_queue_element_t *     element;

    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_ftp_control_connect() entering\n"));
        
    if(handle == GLOBUS_NULL)
    {
	rc = globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_ftp_control_connect: NULL handle argument")
	    ); 
        goto error_exit;
    }

    if(host == GLOBUS_NULL)
    {
	rc = globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_ftp_control_connect: NULL host argument")
	    ); 
	goto error_exit;
    }

    if(port > 65536)
    {
	rc = globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_ftp_control_connect: Port argument greater than 64k")
	    ); 
	goto error_exit;
    }

    if(callback == GLOBUS_NULL)
    {
	rc = globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_ftp_control_connect: NULL callback argument")
	    ); 
	goto error_exit;
    }


    element = (globus_ftp_control_rw_queue_element_t *)
	globus_libc_malloc(sizeof(globus_ftp_control_rw_queue_element_t));
    
    if(element == GLOBUS_NULL)
    {
	rc = globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_ftp_control_connect: malloc failed")
	    ); 
        goto error_exit;
    }

    element->callback=callback;
    element->arg=callback_arg;

    globus_mutex_lock(&(handle->cc_handle.mutex));
    {
	if(globus_fifo_empty(&handle->cc_handle.readers) && 
	   globus_fifo_empty(&handle->cc_handle.writers) &&
	   handle->cc_handle.cc_state == GLOBUS_FTP_CONTROL_UNCONNECTED &&
	   handle->cc_handle.signal_deactivate == GLOBUS_FALSE)
	{
	    handle->cc_handle.cc_state = GLOBUS_FTP_CONTROL_CONNECTING;
	    globus_fifo_enqueue(&handle->cc_handle.readers,
				element);
	    handle->cc_handle.cb_count++;
	}
	else
	{
	    globus_mutex_unlock(&(handle->cc_handle.mutex));
	    globus_libc_free(element);
	    rc = globus_error_put(
		globus_error_construct_string(
		    GLOBUS_FTP_CONTROL_MODULE,
		    GLOBUS_NULL,
		    "globus_ftp_control_connect: Other operation already in progress")
		); 
            goto error_exit;
	}
    }
    globus_mutex_unlock(&(handle->cc_handle.mutex));

    result=globus_libc_gethostname(localhost,MAXHOSTNAMELEN);

    globus_assert(result == 0);

    hp=globus_libc_gethostbyname_r(
	host,
	&(handle->cc_handle.server),
	handle->cc_handle.server_buffer,
	GLOBUS_FTP_CONTROL_HOSTENT_BUFFER_SIZE,
	&errno);


    if(hp == GLOBUS_NULL)
    {
	rc = globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_ftp_control_connect: globus_libc_gethostbyname_r failed")
	    ); 
	goto error_exit;
    }
 
    hp=globus_libc_gethostbyname_r(
	localhost,
	&(handle->cc_handle.client),
	handle->cc_handle.client_buffer,
	GLOBUS_FTP_CONTROL_HOSTENT_BUFFER_SIZE,
	&errno);

    
    if(hp == GLOBUS_NULL)
    {
	rc = globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_ftp_control_connect: globus_libc_gethostbyname_r failed")
	    ); 
	goto error_exit;
    }

    globus_io_attr_set_tcp_nodelay(&handle->cc_handle.io_attr, 
				   GLOBUS_TRUE);
    rc=globus_io_tcp_register_connect(
	host,
	port,
	&handle->cc_handle.io_attr,
	globus_l_ftp_control_connect_cb,
	(void *) handle,
	&handle->cc_handle.io_handle);
    
    if(rc != GLOBUS_SUCCESS)
    {
	globus_mutex_lock(&(handle->cc_handle.mutex));
	{
	    if(handle->cc_handle.cc_state == GLOBUS_FTP_CONTROL_CONNECTING)
	    {
	        handle->cc_handle.cc_state = GLOBUS_FTP_CONTROL_UNCONNECTED;
	    }
	    globus_fifo_dequeue(&handle->cc_handle.readers);
	    globus_libc_free(element);
	    handle->cc_handle.cb_count--;
	    if(!handle->cc_handle.cb_count && 
	       handle->cc_handle.cc_state == GLOBUS_FTP_CONTROL_CLOSING)
	    {
		call_close_cb = GLOBUS_TRUE;
	    }
	}
	globus_mutex_unlock(&(handle->cc_handle.mutex));

	if(call_close_cb == GLOBUS_TRUE)
	{
	    globus_i_ftp_control_call_close_cb(handle);
	}
	goto error_exit;
    }
    
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_ftp_control_connect() exiting\n"));
        
    return GLOBUS_SUCCESS;
error_exit:

    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_ftp_control_connect() exiting with error\n"));
    
    return rc;
}

#ifdef GLOBUS_INTERNAL_DOC

/**
 * Internal callback for the globus_io_tcp_register_connect function.    
 * 
 * This is a internal callback used with the
 * globus_io_tcp_register_connect function which in this library is
 * called in the globus_ftp_control_connect function. It checks that
 * the connect completed successfully and registers a read on the
 * connection. 
 *
 * @param arg
 *        The callback argument, in this case the control handle for
 *        the connection. 
 * @param handle
 *        The globus_io handle for the connection. In practice this
 *        represents the socket fd for the connection.
 * @param result
 *        The result of the connect operation
 *
 * @return void
 *
 * @par If a error is detected in this function the user callback is
 *      called with an appropriate error object and the function
 *      returns. 
 */

#endif

static void 
globus_l_ftp_control_connect_cb(
    void *                                    arg, 
    globus_io_handle_t *                      handle,
    globus_result_t                           result)
{
    globus_ftp_cc_handle_t *                  cc_handle;
    globus_ftp_control_handle_t *             c_handle;
    globus_object_t *                         error;
    globus_result_t                           rc;
    globus_ftp_control_rw_queue_element_t *   element;
    globus_bool_t                             call_close_cb = GLOBUS_FALSE;
    globus_bool_t                             closing = GLOBUS_FALSE;
    
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_l_ftp_control_connect_cb() entering\n"));
        
    c_handle=(globus_ftp_control_handle_t *) arg;

    cc_handle= &(c_handle->cc_handle);

    element= (globus_ftp_control_rw_queue_element_t *)
	globus_fifo_peek(&cc_handle->readers);

    if(result != GLOBUS_SUCCESS)
    {
	error=globus_error_get(result);
	goto return_no_connect;
    }

    globus_mutex_lock(&(cc_handle->mutex));
    {
	if(cc_handle->cc_state == GLOBUS_FTP_CONTROL_CONNECTING)
	{
	    cc_handle->cc_state = GLOBUS_FTP_CONTROL_CONNECTED;
	}
	else if(cc_handle->cc_state == GLOBUS_FTP_CONTROL_CLOSING)
	{
	    closing = GLOBUS_TRUE;
	}
    }
    globus_mutex_unlock(&(cc_handle->mutex));
    
    if(closing)
    {
        error = globus_error_construct_string(
            GLOBUS_FTP_CONTROL_MODULE,
            GLOBUS_NULL,
            "globus_l_ftp_control_connect_cb: connection forced closed");
        
        goto return_error;
    }
    
    rc=globus_io_register_read(handle,
                               cc_handle->read_buffer,
			       GLOBUS_FTP_CONTROL_READ_BUFFER_SIZE,
			       1,
                               globus_l_ftp_control_read_cb,
			       arg);
    if(rc != GLOBUS_SUCCESS)
    {
	error=globus_error_get(rc);
	goto return_error;
    }
    
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_l_ftp_control_connect_cb() exiting\n"));
    return;

return_no_connect:
    
    globus_mutex_lock(&(cc_handle->mutex));
    {
        if(cc_handle->cc_state == GLOBUS_FTP_CONTROL_CONNECTING)
        {
            cc_handle->cc_state = GLOBUS_FTP_CONTROL_UNCONNECTED;
        }
    }
    globus_mutex_unlock(&(cc_handle->mutex));

return_error:
    
    (element->callback)((element->arg),
			c_handle,
			error,
			GLOBUS_NULL);

    globus_mutex_lock(&(cc_handle->mutex));
    {
	globus_fifo_dequeue(&cc_handle->readers);
	cc_handle->cb_count--;
	if(!cc_handle->cb_count && 
	   cc_handle->cc_state == GLOBUS_FTP_CONTROL_CLOSING)
	{
	    call_close_cb = GLOBUS_TRUE;
	}
    }
    globus_mutex_unlock(&(cc_handle->mutex));

    if(call_close_cb == GLOBUS_TRUE)
    {
	globus_i_ftp_control_call_close_cb(c_handle);
    }

    globus_libc_free(element);
    globus_object_free(error);
    
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_l_ftp_control_connect_cb() exiting with error\n"));
    return;
}

#ifdef GLOBUS_INTERNAL_DOC

/**
 * Internal callback for the globus_io_tcp_register_read function.    
 * 
 * This is a internal callback used with the
 * globus_io_tcp_register_read function which in this library is
 * used for reading replies to any sent ftp commands. It checks that
 * the read completed successfully, copies the bytes read to a
 * response buffer, checks whether there is a complete response in the
 * response buffer, decodes the reply if encryption/authentication is
 * used. If the reply is not complete a new register_read is
 * called. If the reply is complete, but preliminary the user callback
 * is called with the intermediate reply and a new register read is
 * called. If the reply is complete and is not preliminary the
 * user/abort callback is called and no further action is taken.
 *
 * @param arg
 *        The callback argument, in this case the control handle for
 *        the connection. 
 * @param handle
 *        The globus_io handle for the connection. In practice this
 *        represents the socket fd for the connection.
 * @param result
 *        The result of the read operation
 * @param buf
 *        The buffer in which the result of the read is stored
 * @param nbytes
 *        The number of bytes read
 *
 * @return void
 *
 * @par If a error is detected in this function the user callback is
 *      called with an appropriate error object and the function
 *      returns. 
 */

#endif

static void 
globus_l_ftp_control_read_cb(
    void *                                    arg, 
    globus_io_handle_t *                      handle,
    globus_result_t                           result,
    globus_byte_t *                           buf, 
    globus_size_t                             nbytes)
{
    globus_ftp_control_rw_queue_element_t *   element;
    globus_ftp_cc_handle_t *                  cc_handle;
    globus_ftp_control_handle_t *             c_handle;
    globus_object_t *                         error;
    globus_byte_t *                           new_buf;
    int                                       end_of_reply;
    globus_result_t                           rc;
    globus_size_t                             response_length;
    globus_bool_t                             queue_empty;
    globus_bool_t                             call_close_cb = GLOBUS_FALSE;
    
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_l_ftp_control_read_cb() entering\n"));
        
    c_handle=(globus_ftp_control_handle_t *) arg;
    cc_handle=&(c_handle->cc_handle);

    queue_empty=GLOBUS_FALSE;
    element= (globus_ftp_control_rw_queue_element_t *)
	globus_fifo_peek(&cc_handle->readers);

    /* check result */

    if(result != GLOBUS_SUCCESS)
    {
	error=globus_error_get(result);
	goto return_error;
    }


    /* copy the result to a response_buffer; allocate more memory if
     * needed 
     */
    
    if (nbytes < (cc_handle->response.response_buffer_size -
		  cc_handle->response.response_length))
    {
        response_length = cc_handle->response.response_length;
        memcpy(&cc_handle->response.response_buffer[response_length],
               buf, nbytes);
	cc_handle->response.response_length+=nbytes;
    }
    else
    {
	new_buf = (globus_byte_t *)
	    globus_libc_malloc(sizeof(globus_byte_t)*
			       (cc_handle->response.response_buffer_size + 
				(nbytes/
				 GLOBUS_I_FTP_CONTROL_BUF_INCR+1)*
				GLOBUS_I_FTP_CONTROL_BUF_INCR));
	
	if(new_buf == GLOBUS_NULL)
	{
	    error=globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_l_ftp_control_read_cb: malloc failed");
	    goto return_error;
	}
	
	cc_handle->response.response_buffer_size+= 
	    (nbytes/GLOBUS_I_FTP_CONTROL_BUF_INCR+1)*
	    GLOBUS_I_FTP_CONTROL_BUF_INCR;

        memcpy(new_buf, 
               cc_handle->response.response_buffer, 
               cc_handle->response.response_length);
	
	globus_libc_free(cc_handle->response.response_buffer);

	cc_handle->response.response_buffer=new_buf;

        memcpy(&(cc_handle->response.response_buffer[
	    cc_handle->response.response_length]),
	       buf, 
	       nbytes);

	cc_handle->response.response_length+=nbytes;

    }
    
    /* check whether there is a full reply in the
     * response_buffer. Note that _end_of_reply() will also do any
     * necessary decoding of protected replies.
     */

    end_of_reply=globus_l_ftp_control_end_of_reply(cc_handle);

    if(end_of_reply == -1)
    {
	error=globus_error_construct_string(
	    GLOBUS_FTP_CONTROL_MODULE,
	    GLOBUS_NULL,
	    "globus_l_ftp_control_read_cb: Error while searching for end of reply");
	goto return_error;
    }

    while (end_of_reply) /* got a full reply */
    {
	if(cc_handle->response.response_class ==
	   GLOBUS_FTP_POSITIVE_PRELIMINARY_REPLY) 
	{
	    /* if reply was preliminary, call the user cb with reply 
	     * and continue to read 
	     */
	    
	    (element->callback)(element->arg,
				c_handle,
				GLOBUS_NULL,
				&(cc_handle->response));

	    response_length = cc_handle->response.response_length;

	    memcpy(cc_handle->response.response_buffer,
		   &cc_handle->read_buffer[
		       nbytes-(response_length-end_of_reply)],
		   response_length-end_of_reply);
	    
	    cc_handle->response.response_length=response_length
		-end_of_reply;
	    
	    end_of_reply=globus_l_ftp_control_end_of_reply(cc_handle);

	    if(end_of_reply == -1)
	    {
		error=globus_error_construct_string(
		    GLOBUS_FTP_CONTROL_MODULE,
		    GLOBUS_NULL,
		    "globus_l_ftp_control_read_cb: Error while searching for end of reply");

		goto return_error;
	    }
	}
	else
	{
	    /* reply was not preliminary so call user callback with
	     * reply and check if there are more entries in the read 
	     * queue 
	     */
	    
	    response_length=cc_handle->response.response_length;
	    cc_handle->response.response_length=end_of_reply;
	    
	    (element->callback)(element->arg,
				c_handle,
				GLOBUS_NULL,
				&(cc_handle->response));
	    
	    
	    memcpy(cc_handle->response.response_buffer,
		   &cc_handle->read_buffer[end_of_reply],
		   response_length-end_of_reply);
	    
	    cc_handle->response.response_length = response_length - 
		end_of_reply;
	    
	    globus_mutex_lock(&cc_handle->mutex);
	    {
		globus_fifo_dequeue(&cc_handle->readers);
		cc_handle->cb_count--;
		queue_empty=globus_fifo_empty(&cc_handle->readers);

		if(!cc_handle->cb_count && 
		   cc_handle->cc_state == GLOBUS_FTP_CONTROL_CLOSING)
		{
		    call_close_cb = GLOBUS_TRUE;
		}
	    }
	    globus_mutex_unlock(&cc_handle->mutex);

	    if(call_close_cb == GLOBUS_TRUE)
	    {
		globus_i_ftp_control_call_close_cb(c_handle);
	    }

	    globus_libc_free(element);
	    
	    if(queue_empty == GLOBUS_TRUE)
	    {
		goto do_return;
	    }
	    else
	    {
		element= (globus_ftp_control_rw_queue_element_t *)
		    globus_fifo_peek(&cc_handle->readers);

		end_of_reply=globus_l_ftp_control_end_of_reply(cc_handle);

		if(end_of_reply == -1)
		{
		    error=globus_error_construct_string(
			GLOBUS_FTP_CONTROL_MODULE,
			GLOBUS_NULL,
			"globus_l_ftp_control_read_cb: Error while searching for end of reply");
		    goto return_error;
		}
	    }
	}
    }

    
    /* call another register_read by default 
     */

    rc=globus_io_register_read(handle,
                               cc_handle->read_buffer,
			       GLOBUS_FTP_CONTROL_READ_BUFFER_SIZE,
			       1,
                               globus_l_ftp_control_read_cb,
                               arg);
    if(rc != GLOBUS_SUCCESS)
    {
	error=globus_error_get(rc);
	goto return_error;
    }
do_return:
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_l_ftp_control_read_cb() exiting\n"));
    return;

return_error:

    (element->callback)(element->arg,
			c_handle,
			error,
			GLOBUS_NULL);

    globus_mutex_lock(&cc_handle->mutex);
    {
	globus_fifo_dequeue(&cc_handle->readers);
	cc_handle->cb_count--;
	queue_empty=globus_fifo_empty(&cc_handle->readers);
	if(!cc_handle->cb_count && 
	   cc_handle->cc_state == GLOBUS_FTP_CONTROL_CLOSING)
	{
	    call_close_cb = GLOBUS_TRUE;
	}
    }
    globus_mutex_unlock(&cc_handle->mutex);


    if(call_close_cb == GLOBUS_TRUE)
    {
	globus_i_ftp_control_call_close_cb(c_handle);
    }

    globus_libc_free(element);
    globus_object_free(error);
    
    if(queue_empty == GLOBUS_FALSE)
    {
	globus_l_ftp_control_read_next(c_handle);
    }
    
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_l_ftp_control_read_cb() exiting with error\n"));
    return;
}

#ifdef GLOBUS_INTERNAL_DOC

/**
 * Internal helper function which checks a buffer for a complete ftp
 * reply. 
 * 
 * This is a internal helper function checks whether a given response
 * struct contains a complete ftp reply. If so, it decodes the reply
 * if necessary and fills in the reply_code and reply_class fields in
 * the response struct.
 *
 * @param cc_handle
 *        A control connection handle, which is assumed to contain
 *        a pointer to the response struct to be processed and, if
 *        authentication is used, information about the security
 *        context of the control connection. 
 *
 * @return 
 *        - -1 if an error occured
 *        - 0 if no complete reply was found
 *        - a index into the response_buffer, indicating the end of
 *          the reply
 *
 */

#endif


int 
globus_l_ftp_control_end_of_reply(
    globus_ftp_cc_handle_t *           cc_handle)
{

    int                                       current;
    int                                       last;
    int                                       first;
    int                                       found;
    int                                       length;
    int                                       total_length;
    char *                                    out_buf;
    gss_buffer_desc                           wrapped_token;
    gss_buffer_desc                           unwrapped_token;
    globus_ftp_control_response_t *           response;
    OM_uint32                                 maj_stat;
    OM_uint32                                 min_stat;
    int                                       conf_state;
    gss_qop_t                                 qop_state;
    globus_result_t                           rc;

    /* could improve performance by saving last in between calls,
     * so we only look at the last line.
     */
    
    last=-1;
    current=1;
    found=0;
    first=1;
    response=&cc_handle->response;


    /* find the end of reply */

    while(!found && current < response->response_length)
    {
	if(response->response_buffer[current - 1] == '\r' &&
	   response->response_buffer[current] == '\n')
	{
	    /* check that we actually have a ftp reply */

	    if(first)
	    {
		if((current < 5) ||
		   !(isdigit((response->response_buffer)[last+1]) &&
		     isdigit((response->response_buffer)[last+2]) &&
		     isdigit((response->response_buffer)[last+3]) &&
		     (((response->response_buffer)[last+4]==' ') ||
		      ((response->response_buffer)[last+4]=='-'))))
		{
		    return -1;
		}
		first=0;
	    }
	    if(((response->response_buffer)[last+4]==' ') &&
	       isdigit((response->response_buffer)[last+1]) &&
	       isdigit((response->response_buffer)[last+2]) &&
	       isdigit((response->response_buffer)[last+3]))
	    {
		found=current+1;
	    }
	    else
	    {
		last=current;
	    }
	}
	current++;
    }
    
    if(found)
    {
	/* need to unwrap reply if it is protected */

	if((response->response_buffer)[last+1] == '6')
	{
	    last=-1;
	    current=0;
	    length=0;
	    total_length=0;

	    out_buf = globus_libc_malloc(
		(response->response_length + 3) * 6 / 8);
	    
	    if( out_buf == GLOBUS_NULL)
	    {
		return -1;
	    }

	    while(current<found)
	    {
		if(response->response_buffer[current] == '\n')
		{
		    /* Decode token */
		    
		    response->response_buffer[current-1] = '\0';
		    rc=globus_i_ftp_control_radix_decode(
			&(response->response_buffer[last+5]),
			&(out_buf[total_length]),&length);
		    


		    /* Unwrap token */
		    
		    wrapped_token.value = &(out_buf[total_length]);
		    wrapped_token.length = length;
		    
		    maj_stat = gss_unwrap(&min_stat, 
					  cc_handle->auth_info.
					  auth_gssapi_context,
					  &wrapped_token, 
					  &unwrapped_token,
					  &conf_state, 
					  &qop_state);

		    if(maj_stat != GSS_S_COMPLETE)
		    {
			globus_libc_free(out_buf);
			return -1;
		    }
		    
		    /* get rid of terminating NULL */

		    unwrapped_token.length--; 

		    memcpy(&(out_buf[total_length]),
			   unwrapped_token.value,
			   unwrapped_token.length);
		    length = unwrapped_token.length;
		    total_length += length;
		    
		    gss_release_buffer(&min_stat, &unwrapped_token);
		    last=current;
		}
		current++;
	    }

	    memcpy(&(out_buf[total_length]),
		   &(response->response_buffer[found]),
		   response->response_length-found);
		   
	    globus_libc_free(response->response_buffer);
	    response->response_buffer=out_buf;
	    response->response_buffer_size=
		(response->response_length + 3) * 6 / 8;
	    response->response_length=
		total_length+response->response_length-found;
	    found=total_length;
	    last=total_length-length-1;
	    
	}

	/* get the ftp response code */

	response->response_buffer[found]='\0';

        if(sscanf(&(response->response_buffer[last+1]), 
		  "%d", &response->code) < 1)
        {
            globus_assert(0); 
        }

	/* determine the ftp response class */

	switch(response->response_buffer[last+1])
	{
	case '1':
	    response->response_class=
		GLOBUS_FTP_POSITIVE_PRELIMINARY_REPLY;
            break;
	case '2':
	    response->response_class=
		GLOBUS_FTP_POSITIVE_COMPLETION_REPLY;
            break;
	case '3':
	    response->response_class=
		GLOBUS_FTP_POSITIVE_INTERMEDIATE_REPLY;
            break;
	case '4':
	    response->response_class=
		GLOBUS_FTP_TRANSIENT_NEGATIVE_COMPLETION_REPLY;
            break;
	case '5':
	    response->response_class=
		GLOBUS_FTP_PERMANENT_NEGATIVE_COMPLETION_REPLY;
            break;
	default:
	    response->response_class=
		GLOBUS_FTP_UNKNOWN_REPLY;
            break;
	}
    }
    
    return found;
}


#ifdef GLOBUS_INTERNAL_DOC

/**
 * Internal helper function which creates and initializes a response
 * structure 
 * 
 * This is a internal helper function allocates memory for a reponse
 * structure and a buffer contained within that structure. It also
 * initializes other values in the structure to default values.
 *
 * @param response
 *        This parameter is used to return the new response struct.
 *
 * @return 
 *        - Error object
 *        - GLOBUS_SUCCESS
 *
 */

#endif


static globus_result_t 
globus_l_ftp_control_response_init(
    globus_ftp_control_response_t *               response)
{
    response->code=0;
    response->response_class=GLOBUS_FTP_UNKNOWN_REPLY;
    response->response_length=0;
    response->response_buffer_size=
	GLOBUS_I_FTP_CONTROL_BUF_SIZE;
    response->response_buffer=(globus_byte_t *) 
	globus_libc_malloc(sizeof(globus_byte_t)*
			   GLOBUS_I_FTP_CONTROL_BUF_SIZE);  
    
    if(response->response_buffer== GLOBUS_NULL)
    {
	return globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_l_ftp_control_response_init: malloc failed")
	    );
    }
    return GLOBUS_SUCCESS;
}

/**
 * Helper function which frees the memory associated with a response
 * structure.  
 * 
 * This is a helper function which frees the memory associated with a 
 * response structure. 
 *
 * @param response
 *        This parameter indicates the response structure to destroy
 *
 * @return 
 *        - Error object
 *        - GLOBUS_SUCCESS
 *
 */


globus_result_t 
globus_ftp_control_response_destroy(
    globus_ftp_control_response_t *               response)
{
    
    if(response == GLOBUS_NULL)
    {
	return GLOBUS_SUCCESS;
    }
    
    if(response->response_buffer != GLOBUS_NULL)
    {
	globus_libc_free(response->response_buffer);
    }
    
    return GLOBUS_SUCCESS;
}

/**
 * Helper function which copies one response structure to another
 * 
 * This is a helper function which copies one response structure to
 * another. 
 *
 * @param src
 *        This parameter indicates the response structure to copy
 * @param dest
 *        This parameter specifies the target response structure
 *
 * @return 
 *        - Error object
 *        - GLOBUS_SUCCESS
 *
 */


globus_result_t 
globus_ftp_control_response_copy(
    globus_ftp_control_response_t *       src,
    globus_ftp_control_response_t *       dest)
{
    if(src == GLOBUS_NULL)
    {
	return globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_l_ftp_control_response_copy: Source argument is NULL")
	    );
    }

    if(dest == GLOBUS_NULL)
    {
	return globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_l_ftp_control_response_copy: Destination argument is NULL")
	    ); 
    }


    dest->code=src->code;
    dest->response_class=src->response_class;
    dest->response_length=src->response_length;
    dest->response_buffer_size=src->response_buffer_size;
    dest->response_buffer=(globus_byte_t *) 
	globus_libc_malloc(dest->response_buffer_size);
    
    if(dest->response_buffer== GLOBUS_NULL)
    {
	return globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_l_ftp_control_response_copy: malloc failed")
	    );
    }
    
    memcpy(dest->response_buffer,
	   src->response_buffer,
	   dest->response_length+1);
    
    return GLOBUS_SUCCESS;
}


/**
 * Authenticate the user to the FTP server.
 *
 * This will perform the authentication handshake with the FTP
 * server. depending on which parameters are non-NULL, the
 * authentication may involve GSSAPI credentials, a username, a
 * password, and an account name.
 *
 * @note Do we want to add attribute arguments for:
 *       - specifying type of delegation
 *       - gsswrap control messages for integrity or confidentiality
 *
 * @param handle
 *        A pointer to a unauthenticated GSIFTP control handle. In the 
 *        case of GSS authentication the GSS security context is stored in
 *        this structure.
 * @param auth_info
 *        This structure is used to pass the following information: 
 *        - user
 *          The user's name for login purposes. If this string is
 *          "anonymous", "ftp", GLOBUS_NULL or ":globus-mapping:" then
 *          the password argument is optional. If this string is
 *          GLOBUS_NULL or ":globus-mapping:" and gss_auth is true then
 *          the users login is looked by the FTP server host.
 *        - password
 *          The password for the above user argument. If the user
 *          argument is "anonymous" or "ftp" or if gss_auth is true this
 *          string may be GLOBUS_NULL.  
 *        - account
 *          This parameter is optional. If not used it should be set to
 *          GLOBUS_NULL. It might be needed by firewalls.
 *        - auth_gssapi_subject
 *          The GSSAPI subject name of the server you are connecting
 *          to. If this is GLOBUS_NULL, and the gss_auth parameter is
 *          set to GLOBUS_TRUE, then the name will default to the host
 *          name. 
 * @param use_auth
 *        If set to GLOBUS_TRUE the above argument indicates that GSS
 *        authentication should be used, otherwise cleartext
 *        user/password authentication is used.
 * @param callback
 *        The function to be called once the authentication process is 
 *        complete or when an error occurs.
 * @param callback_arg
 *        User supplied argument to the callback function
 *
 * @return
 *        - success
 *        - Null handle
 *        - Invalid handle
 *        - Handle already authenticated
 *
 * @par Callback errors:
 *        - success
 *        - authentication failed
 *        - protocol error
 *        - eof
 *
 * @par Expected callback response values:
 *        - 230 User logged in, proceed.
 *        - 232 User logged in, authorized by security data exchange.
 *        - 234 Security data exchange complete.
 *        - 331 User name okay, need password.
 *        - 332 Need account for login.
 *        - 336 Username okay, need password. Challenge is "...."
 *        - 431 Need some unavailable resource to process security.
 *        - 500 Syntax error, command unrecognized.
 *        - 530 Not logged in.
 *
 * @note The server may send other responses.
 */

globus_result_t
globus_ftp_control_authenticate(
    globus_ftp_control_handle_t *		handle,
    globus_ftp_control_auth_info_t *            auth_info,
    globus_bool_t				use_auth,
    globus_ftp_control_response_callback_t	callback,
    void *					callback_arg)
{
    globus_result_t                             rc;
    globus_result_t                             result;
    globus_i_ftp_passthru_cb_arg_t *            auth_cb_arg;
    
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_ftp_control_authenticate() entering\n"));
        
    if(handle == GLOBUS_NULL)
    {
	result = globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_ftp_control_authenticate: handle argument is NULL")
	    );
	goto error;
    }

    if(auth_info == GLOBUS_NULL)
    {
	result = globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_ftp_control_authenticate: auth_info argument is NULL")
	    );
	goto error;
    }

    if(handle->cc_handle.auth_info.auth_gssapi_context != 
       GSS_C_NO_CONTEXT &&
       use_auth == GLOBUS_TRUE)
    {
	result = globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_ftp_control_authenticate: Already authenticated")
	    );
	goto error;
    }

    if(use_auth == GLOBUS_FALSE &&
       auth_info->user == GLOBUS_NULL)
    {
	result = globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_ftp_control_authenticate: No user supplied")
	    );
	goto error;
    }


    globus_mutex_lock(&(handle->cc_handle.mutex));
    {
	if(handle->cc_handle.cc_state != GLOBUS_FTP_CONTROL_CONNECTED)
	{
	    globus_mutex_unlock(&(handle->cc_handle.mutex));
	    result = globus_error_put(
		globus_error_construct_string(
		    GLOBUS_FTP_CONTROL_MODULE,
		    GLOBUS_NULL,
		    "globus_ftp_control_authenticate: Handle not connected")
		);
            goto error;
	}
    }
    globus_mutex_unlock(&(handle->cc_handle.mutex));

    /* copy information into the control connection handle */

    rc = globus_i_ftp_control_auth_info_init(
	&(handle->cc_handle.auth_info),auth_info);

    if(rc != GLOBUS_SUCCESS)
    {
	result=rc;
	goto error;
    }

    handle->cc_handle.use_auth=use_auth;
    
    auth_cb_arg = (globus_i_ftp_passthru_cb_arg_t *)
	globus_libc_malloc(sizeof(globus_i_ftp_passthru_cb_arg_t));
    
    if(auth_cb_arg == GLOBUS_NULL)
    {
	result=globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_ftp_control_authenticate: malloc failed")
	    );
	goto error;
    }

    auth_cb_arg->user_cb=callback;
    auth_cb_arg->user_cb_arg=callback_arg;


    if(use_auth == GLOBUS_FALSE)
    {

	auth_cb_arg->cmd=GLOBUS_I_FTP_USER;

	rc = globus_ftp_control_send_command(handle,"USER %s\r\n",
					     globus_l_ftp_control_send_cmd_cb,
					     (void *) auth_cb_arg,
					     auth_info->user);
    }
    else
    {	
	auth_cb_arg->cmd=GLOBUS_I_FTP_AUTH;
	rc = globus_ftp_control_send_command(handle,"AUTH GSSAPI\r\n",
					     globus_l_ftp_control_send_cmd_cb,
					     (void *) auth_cb_arg);
    }
    
    if(rc != GLOBUS_SUCCESS)
    {
	globus_libc_free(auth_cb_arg);
	result=rc;
	goto error;
    }

    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_ftp_control_authenticate() exiting\n"));
        
    return GLOBUS_SUCCESS;
    
error:
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_ftp_control_authenticate() exiting with error\n"));
        
    return result;
}

/**
 * Send an FTP protocol command to the FTP server and register a
 * response handler.
 *
 * This function is used to send an FTP command, and register a handler
 * to receive the FTP reply (or replies, if an intermediate one is sent).
 * When the control channel is gss authenticated, the message and the reply
 * will be automatically gss wrapped/unwrapped.
 *
 * @param handle
 *        A pointer to a GSIFTP control handle. The command described by
 *        the cmdspec is issued to the server over the control channel
 *        associated with this handle.
 * @param cmdspec
 *        A printf-style format string containing the text of the command
 *        to send to the server. The optional parameters to the format string
 *        are passed after the callback_arg in the function invocation.
 * @param callback
 *        The function to be called once the authentication process is 
 *        complete or when an error occurs.
 * @param callback_arg
 *        User supplied argument to the callback function
 * @param ...
 *        Parameters which will be substituted into the % escapes in the
 *        cmdspec string.
 *
 * @return
 *        - Success
 *        - Null handle
 *        - Command already in progress
 *
 * @par Callback errors:
 *        - success
 *        - protocol error
 *        - eof
 *
 * @par Expected callback response values:
 *        Any defined in RFC 959, 2228, 2389, draft-ietf-ftpext-mlst-10,
 *        or the @ref extensions_intro "protocol extensions" document.
 */

globus_result_t
globus_ftp_control_send_command(
    globus_ftp_control_handle_t *		handle,
    const char *				cmdspec,
    globus_ftp_control_response_callback_t	callback,
    void *					callback_arg,
    ...)
{
    globus_ftp_control_rw_queue_element_t *     element;
    globus_result_t                             rc;
    globus_result_t                             result;
    globus_bool_t                               queue_empty;
    globus_bool_t                               authenticated;
    globus_bool_t                               call_close_cb = GLOBUS_FALSE;
    globus_byte_t *                             buf;
    globus_byte_t *                             encode_buf;
    va_list                                     ap;
    int                                         arglength;

    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_ftp_control_send_command() entering\n"));
        
    if(handle == GLOBUS_NULL)
    {
	result = globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_ftp_control_send command: handle argument is NULL")
	    );
	goto error; 
    }

#ifdef HAVE_STDARG_H
    va_start(ap, callback_arg);
#else
    va_start(ap);
#endif
    
    arglength=globus_libc_vfprintf(globus_i_ftp_control_devnull,
				   cmdspec,
				   ap);

    if(arglength < 1)
    {
	result=globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_ftp_control_send_command: Unable to determine total length of command string")
	    );
	goto error;
    }

    buf=(globus_byte_t *) globus_libc_malloc(sizeof(globus_byte_t)*
					     (arglength+1));

    if(buf == GLOBUS_NULL)
    {
	result=globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_ftp_control_send_command: malloc failed")
	    );
	goto error;
    }
    
    if(globus_libc_vsprintf((char *) buf, cmdspec,ap)<arglength)
    {
	globus_libc_free(buf);
	result= globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_ftp_control_send_command: Command string construction failed")
	    );
	goto error;
    }

    globus_mutex_lock(&(handle->cc_handle.mutex));
    {
	authenticated = handle->cc_handle.auth_info.authenticated;
    }
    globus_mutex_unlock(&(handle->cc_handle.mutex));

    if(authenticated == GLOBUS_TRUE)	    
    {
	/* encode the command */

	rc=globus_i_ftp_control_encode_command(&(handle->cc_handle),
					       buf,
					       (char **) &encode_buf);
	
	globus_libc_free(buf);

	if(rc != GLOBUS_SUCCESS)
	{
	    result=rc;
	    goto error;
	}

	buf=encode_buf;

    }
    
    element = (globus_ftp_control_rw_queue_element_t *)
	globus_libc_malloc(sizeof(globus_ftp_control_rw_queue_element_t));
    
    if(element == GLOBUS_NULL)
    {
	result=globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_ftp_control_send_command: malloc failed")
	    );
	globus_libc_free(buf);
	goto error;
    }

    
    element->callback = callback;
    element->arg = callback_arg;
    element->write_flags = 0;
    element->write_buf = buf;
    element->write_callback = globus_l_ftp_control_write_cb;
    element->read_callback = globus_l_ftp_control_read_cb;
    element->expect_response = GLOBUS_TRUE;
    
    globus_mutex_lock(&(handle->cc_handle.mutex));
    {
	if(handle->cc_handle.cc_state == GLOBUS_FTP_CONTROL_CONNECTED)
	{
	    queue_empty=globus_fifo_empty(&handle->cc_handle.writers);
	    globus_fifo_enqueue(&handle->cc_handle.writers,
				element);
	    handle->cc_handle.cb_count++;
	}
	else
	{
	    globus_mutex_unlock(&(handle->cc_handle.mutex));
	    result=globus_error_put(
		globus_error_construct_string(
		    GLOBUS_FTP_CONTROL_MODULE,
		    GLOBUS_NULL,
		    "globus_ftp_control_send_command: Handle not connected")
		);
	    globus_libc_free(buf);
	    globus_libc_free(element);
	    goto error;

	}
    }
    globus_mutex_unlock(&(handle->cc_handle.mutex));
    
    if(queue_empty == GLOBUS_TRUE)
    {
	/* queue was empty, we need to do the write/send */

	rc = globus_io_register_write(&handle->cc_handle.io_handle,
				      buf,
				      (globus_size_t) strlen(buf),
				      element->write_callback,
				      (void *) handle);
    
	if(rc != GLOBUS_SUCCESS)
	{
	    globus_mutex_lock(&(handle->cc_handle.mutex));
	    {
		globus_fifo_dequeue(&handle->cc_handle.writers);
		handle->cc_handle.cb_count--;
		queue_empty=globus_fifo_empty(&handle->cc_handle.writers);
		if(!handle->cc_handle.cb_count &&  
		   handle->cc_handle.cc_state == GLOBUS_FTP_CONTROL_CLOSING) 
		{ 
		    call_close_cb = GLOBUS_TRUE; 
		} 
	    }
	    globus_mutex_unlock(&(handle->cc_handle.mutex));

	    if(call_close_cb == GLOBUS_TRUE) 
	    { 
		globus_i_ftp_control_call_close_cb(handle);
	    } 

	    globus_libc_free(buf);
	    globus_libc_free(element);	

	    if(queue_empty == GLOBUS_FALSE)
	    {
		globus_i_ftp_control_write_next(handle);
	    }
	    
	    result=rc;
	    goto error;
	}
    }

    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_ftp_control_send_command() exiting\n"));
        
    return GLOBUS_SUCCESS;

error:
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_ftp_control_send_command() exiting with error\n"));
        
    return result;
}

#ifdef GLOBUS_INTERNAL_DOC

/**
 * Internal callback for the globus_io_tcp_register_write function.    
 * 
 * This is a internal callback used with the
 * globus_io_tcp_register_write function which in this library is
 * used for sending all ftp commands but ABOR. It checks that
 * the write completed successfully and then calls
 * globus_io_register_read to read the reply for the command that was
 * sent. 
 *
 * @param arg
 *        The callback argument.
 * @param handle
 *        The globus_io handle for the connection. In practice this
 *        represents the socket fd for the connection.
 * @param result
 *        The result of the write operation
 * @param buf
 *        The buffer in which the command was stored
 * @param nbytes
 *        The number of bytes written
 *
 * @return void
 *
 * @par If a error is detected in this function the user callback is
 *      called with an appropriate error object and the function
 *      returns. 
 */

#endif

static void 
globus_l_ftp_control_write_cb(
    void *                                    arg, 
    globus_io_handle_t *                      handle,
    globus_result_t                           result,
    globus_byte_t *                           buf, 
    globus_size_t                             nbytes){

    globus_ftp_cc_handle_t *                  cc_handle;
    globus_ftp_control_handle_t *             c_handle;
    globus_object_t *                         error;
    globus_result_t                           rc;
    globus_ftp_control_rw_queue_element_t *   element;
    globus_bool_t                             write_queue_empty;
    globus_bool_t                             read_queue_empty;
    globus_bool_t                             call_close_cb = GLOBUS_FALSE;
    
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_l_ftp_control_write_cb() entering\n"));
        
    c_handle=(globus_ftp_control_handle_t *) arg;
    cc_handle=&(c_handle->cc_handle);

    globus_libc_free(buf);
    
    globus_mutex_lock(&(cc_handle->mutex));
    {
	element = (globus_ftp_control_rw_queue_element_t *)
	    globus_fifo_dequeue(&cc_handle->writers);
	cc_handle->cb_count--;
	write_queue_empty=globus_fifo_empty(&cc_handle->writers);

	if(element->expect_response == GLOBUS_TRUE &&
	    result == GLOBUS_SUCCESS)
	{
	    read_queue_empty=globus_fifo_empty(&cc_handle->readers);
	    globus_fifo_enqueue(&cc_handle->readers,
				element);
	    cc_handle->cb_count++;
	}
	
	if(!cc_handle->cb_count &&  
	   cc_handle->cc_state == GLOBUS_FTP_CONTROL_CLOSING) 
	{ 
	    call_close_cb = GLOBUS_TRUE; 
	} 
    }
    globus_mutex_unlock(&(cc_handle->mutex));

    if(call_close_cb == GLOBUS_TRUE) 
    { 
	globus_i_ftp_control_call_close_cb(c_handle);
    } 

    if(write_queue_empty == GLOBUS_FALSE)
    {
	globus_i_ftp_control_write_next(c_handle);
    }
    
    if(result != GLOBUS_SUCCESS)
    {
	error=globus_error_get(result);
	goto return_error;
    }

    if(element->expect_response == GLOBUS_TRUE)
    {
	if(read_queue_empty == GLOBUS_TRUE)
	{
	    
	    rc=globus_io_register_read(&cc_handle->io_handle,
				       cc_handle->read_buffer,
				       GLOBUS_FTP_CONTROL_READ_BUFFER_SIZE,
				       1,
				       element->read_callback,
				       arg);
	    if(rc != GLOBUS_SUCCESS)
	    {
		globus_mutex_lock(&(cc_handle->mutex));
		{
		    element = (globus_ftp_control_rw_queue_element_t *)
			globus_fifo_dequeue(&cc_handle->readers);
		    cc_handle->cb_count--;
		    read_queue_empty=globus_fifo_empty(&cc_handle->readers);

		    if(!cc_handle->cb_count &&  
		       cc_handle->cc_state == GLOBUS_FTP_CONTROL_CLOSING) 
		    { 
			call_close_cb = GLOBUS_TRUE; 
		    } 
		}
		globus_mutex_unlock(&(cc_handle->mutex));

		if(call_close_cb == GLOBUS_TRUE) 
		{ 
		    globus_i_ftp_control_call_close_cb(c_handle);
		} 
	    
		if(read_queue_empty == GLOBUS_FALSE)
		{
		    globus_l_ftp_control_read_next(c_handle);
		}
		
		error=globus_error_get(rc);
		goto return_error;
	    }
	}
    }
    else
    {
	globus_libc_free(element);
    }
    
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_l_ftp_control_write_cb() exiting\n"));
        
    return;
    
return_error:

    if(element->expect_response == GLOBUS_TRUE)
    {
	(element->callback)((element->arg),
			    c_handle,
			    error,
			    GLOBUS_NULL);
    }

    globus_libc_free(element);
    globus_object_free(error);
    
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_l_ftp_control_write_cb() exiting with error\n"));
        
    return;
}



/**
 * Send an ABORT to the FTP server and register a response handler.
 *
 * This function is used to send the ABORT message to the FTP server.
 * The ABORT message is sent out-of-band, and terminates any current
 * data transfer in progress.
 *
 * As a result of the ABORT, the data channels used by this control channel
 * will be closed. The data command callback will be issued with either 
 * a completion reply, or a transfer aborted reply. The ABORT callback
 * will also be invoked, with the server's response to the abort command.
 *
 * Any attempts to register buffers for read or write after an ABORT
 * has been sent will fail with a "no transfer in progress" error.
 *
 * @param handle
 *        A pointer to a GSIFTP control handle. The ABORT command 
 *        is issued to the server over the control channel
 *        associated with this handle.
 * @param callback
 *        The function to be called once the authentication process is 
 *        complete or when an error occurs.
 * @param callback_arg
 *        User supplied argument to the callback function
 *
 * @return
 *        - Success
 *        - Null handle
 *        - No transfer in progress
 *
 * @par Callback errors:
 *        - success
 *        - protocol error
 *        - eof
 *
 * @par Expected callback response values:
 *        - 226 Abort successful.
 *        - 500 Syntax error, command unrecognized.
 *
 * @note The server may send other responses.
 */

globus_result_t
globus_ftp_control_abort(
    globus_ftp_control_handle_t *		handle,
    globus_ftp_control_response_callback_t	callback,
    void *					callback_arg)
{
    globus_result_t                             rc;
    globus_result_t                             result;
    globus_ftp_control_rw_queue_element_t *     element;
    globus_ftp_control_rw_queue_element_t *     element_ip;
    globus_ftp_control_rw_queue_element_t *     element_synch;
    globus_ftp_control_rw_queue_element_t *     element_abor;
    globus_bool_t                               queue_empty;
    globus_fifo_t                               abort_queue;

    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_ftp_control_abort() entering\n"));

    globus_fifo_init(&abort_queue);
    
    element_ip = (globus_ftp_control_rw_queue_element_t *)
	globus_libc_malloc(sizeof(globus_ftp_control_rw_queue_element_t));
    
    if(element_ip == GLOBUS_NULL)
    {
	result = globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_ftp_control_abort: malloc failed")
	    ); 
	goto return_error;
    }

    result = globus_l_ftp_control_queue_element_init(
	element_ip,
	callback,
	callback_arg,
	GLOBUS_I_TELNET_IP,
	0,
	globus_l_ftp_control_write_cb,
	GLOBUS_NULL,
	GLOBUS_FALSE,
	GLOBUS_FALSE,
	handle);
    
    
    if(result != GLOBUS_SUCCESS)
    {
	globus_libc_free(element_ip);
	goto return_error;
    }
    
    element_synch = (globus_ftp_control_rw_queue_element_t *)
	globus_libc_malloc(sizeof(globus_ftp_control_rw_queue_element_t));
    
    if(element_synch == GLOBUS_NULL)
    {
	globus_libc_free(element_ip->write_buf);
	globus_libc_free(element_ip);
	result = globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_ftp_control_abort: malloc failed")
	    ); 
	goto return_error;
    }

    result = globus_l_ftp_control_queue_element_init(
	element_synch,
	callback,
	callback_arg,
	GLOBUS_I_TELNET_SYNCH,
	MSG_OOB,
	globus_l_ftp_control_write_cb,
	GLOBUS_NULL,
	GLOBUS_FALSE,
	GLOBUS_FALSE,
	handle);

    if(result != GLOBUS_SUCCESS)
    {
	globus_libc_free(element_ip->write_buf);
	globus_libc_free(element_ip);
	globus_libc_free(element_synch);
	goto return_error;
    }   

    element_abor = (globus_ftp_control_rw_queue_element_t *)
	globus_libc_malloc(sizeof(globus_ftp_control_rw_queue_element_t));
    
    if(element_abor == GLOBUS_NULL)
    {
	globus_libc_free(element_ip->write_buf);
	globus_libc_free(element_ip);
	globus_libc_free(element_synch->write_buf);
	globus_libc_free(element_synch);
	result = globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_ftp_control_abort: malloc failed")
	    );
	goto return_error; 
    }

    result = globus_l_ftp_control_queue_element_init(
	element_abor,
	callback,
	callback_arg,
	"ABOR\r\n",
	0,
	globus_l_ftp_control_write_cb,
	globus_l_ftp_control_read_cb,
	GLOBUS_TRUE,
	handle->cc_handle.use_auth,
	handle);
    
    if(result != GLOBUS_SUCCESS)
    {
	globus_libc_free(element_ip->write_buf);
	globus_libc_free(element_ip);
	globus_libc_free(element_synch->write_buf);
	globus_libc_free(element_synch);
	globus_libc_free(element_abor);
	goto return_error;
    }

    globus_mutex_lock(&(handle->cc_handle.mutex));
    {
	if( handle->cc_handle.cc_state != 
	    GLOBUS_FTP_CONTROL_CONNECTED)
	{
	    globus_mutex_unlock(&(handle->cc_handle.mutex));
	    globus_libc_free(element_ip->write_buf);
	    globus_libc_free(element_ip);
	    globus_libc_free(element_synch->write_buf);
	    globus_libc_free(element_synch);
	    globus_libc_free(element_abor->write_buf);
	    globus_libc_free(element_abor);
	    result =  globus_error_put(
		globus_error_construct_string(
		    GLOBUS_FTP_CONTROL_MODULE,
		    GLOBUS_NULL,
		    "globus_ftp_control_abort: Handle not connected")
		);
	    goto return_error;
	}
	else
	{
	    queue_empty=globus_fifo_empty(&handle->cc_handle.writers);
	    if(queue_empty == GLOBUS_FALSE)
	    {
		globus_fifo_move(&abort_queue,
				 &handle->cc_handle.writers);
		globus_fifo_enqueue(&handle->cc_handle.writers,
				    globus_fifo_dequeue(&abort_queue));
	    }
		
	    globus_fifo_enqueue(&handle->cc_handle.writers,
				(void *) element_ip);
	    globus_fifo_enqueue(&handle->cc_handle.writers,
				(void *) element_synch);
	    globus_fifo_enqueue(&handle->cc_handle.writers,
				(void *) element_abor);
	    handle->cc_handle.cb_count -= globus_fifo_size(&abort_queue); 
	    handle->cc_handle.cb_count += 3; 
	}
    }
    globus_mutex_unlock(&(handle->cc_handle.mutex));

    while( (element=globus_fifo_dequeue(&abort_queue)) != GLOBUS_NULL)
    {
	(element->callback)((element->arg),
			    handle,
			    globus_error_construct_string(
				GLOBUS_FTP_CONTROL_MODULE,
				GLOBUS_NULL,
				"Command aborted"),
			    GLOBUS_NULL);
	globus_libc_free(element);
    }

    
    if(queue_empty == GLOBUS_TRUE)
    {
	/* queue was empty, we need to do the write/send */
	
	rc = globus_io_register_write(&handle->cc_handle.io_handle,
				      element_ip->write_buf,
				      (globus_size_t) strlen(
					  element_ip->write_buf),
				      element_ip->write_callback,
				      (void *) handle);
    
	if(rc != GLOBUS_SUCCESS)
	{
	    globus_mutex_lock(&(handle->cc_handle.mutex));
	    {
		globus_fifo_dequeue(&handle->cc_handle.writers);
		globus_fifo_dequeue(&handle->cc_handle.writers);
		globus_fifo_dequeue(&handle->cc_handle.writers);
		handle->cc_handle.cb_count -= 3; 
		queue_empty=globus_fifo_empty(&handle->cc_handle.writers);
	    }
	    globus_mutex_unlock(&(handle->cc_handle.mutex));

	    globus_libc_free(element_ip->write_buf);
	    globus_libc_free(element_ip);
	    globus_libc_free(element_synch->write_buf);
	    globus_libc_free(element_synch);
	    globus_libc_free(element_abor->write_buf);
	    globus_libc_free(element_abor);

	    if(queue_empty == GLOBUS_FALSE)
	    {
		globus_i_ftp_control_write_next(handle);
	    }
	    
	    result=rc;
	    goto return_error;
	}
    }

    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_ftp_control_abort() exiting\n"));

    return GLOBUS_SUCCESS;

return_error:
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_ftp_control_abort() exiting with error\n"));
        
    return result;
}

/**
 * Send a QUIT message to the FTP server and register a response handler.
 *
 * This function is used to close the control channel to the FTP server.
 * There should be no transfer commands in progress when this is called.
 * Once the final response callback passed to this function is invoked,
 * the control handle can no longer be used for any gsiftp control
 * operations.
 *
 * @note Need to further define behavior for when a QUIT happens
 *       during a transfer or command is in progress.
 *
 * @note Since this function waits until all other callbacks are completed
 * before calling it's own callback it may not be called in a blocking
 * fashion from another callback.
 *
 * @param handle
 *        A pointer to a GSIFTP control handle. The quit message is
 *        issued to the server over the control channel
 *        associated with this handle.
 * @param callback
 *        The function to be called once the authentication process is 
 *        complete or when an error occurs.
 * @param callback_arg
 *        User supplied argument to the callback function
 * @return
 *        - Success
 *        - Null handle
 *        - Command in progress
 *
 * @par Callback errors:
 *        - success
 *        - protocol error
 *        - eof
 *
 * @par Expected callback response values:
 *        - 221 Service closing control connection.
 *        - 500 Syntax error, command unrecognized.
 *
 * @note The server may send other responses.
 */

globus_result_t
globus_ftp_control_quit(
    globus_ftp_control_handle_t *		handle,
    globus_ftp_control_response_callback_t	callback,
    void *					callback_arg)
{
    globus_result_t                             rc;
    globus_result_t                             result;
    globus_i_ftp_passthru_cb_arg_t *            quit_cb_arg;
    
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_ftp_control_quit() entering\n"));
        
    globus_mutex_lock(&(handle->cc_handle.mutex));
    {
	if(handle->cc_handle.cc_state != GLOBUS_FTP_CONTROL_CONNECTED)
	{
	    globus_mutex_unlock(&(handle->cc_handle.mutex));
	    result = globus_error_put(
		globus_error_construct_string(
		    GLOBUS_FTP_CONTROL_MODULE,
		    GLOBUS_NULL,
		    "globus_ftp_control_quit: Handle not connected")
		);
            goto return_error;
	}
	else
	{
	    handle->cc_handle.close_cb = callback; 
            handle->cc_handle.close_cb_arg = callback_arg;
	}
    }
    globus_mutex_unlock(&(handle->cc_handle.mutex));


    quit_cb_arg = (globus_i_ftp_passthru_cb_arg_t *)
	globus_libc_malloc(sizeof(globus_i_ftp_passthru_cb_arg_t));
    
    if(quit_cb_arg == GLOBUS_NULL)
    {
	result=globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_ftp_control_quit: malloc failed")
	    );
	goto return_error;
    }
    
    quit_cb_arg->user_cb = callback;
    quit_cb_arg->user_cb_arg = callback_arg;
    quit_cb_arg->cmd = GLOBUS_I_FTP_QUIT;
    quit_cb_arg->handle = handle;

    rc = globus_ftp_control_send_command(handle,"QUIT\r\n",
					 globus_l_ftp_control_send_cmd_cb,
					 (void *) quit_cb_arg);
    if(rc != GLOBUS_SUCCESS)
    {
	globus_libc_free(quit_cb_arg);
	result=rc;
	goto return_error;
    }
    
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_ftp_control_quit() exiting\n"));
        
    return GLOBUS_SUCCESS;
    
return_error:
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_ftp_control_quit() exiting with error\n"));
        
    return result;
}

#ifdef GLOBUS_INTERNAL_DOC

/**
 * Internal callback for the globus_ftp_control_send_cmd function.    
 * 
 * This is a internal callback used with the
 * globus_ftp_control_send_cmd function which in this library is
 * used in the authentication process and for sending the QUIT
 * command. It checks that the command was successfully sent and based
 * upon the command that was executed and the ftp reply to that
 * command calls other functions.
 *
 * @param arg
 *        The callback argument, which in this case is used to pass
 *        the original user callback and argument.
 * @param handle
 *        The control handle associated with the session
 * @param error
 *        A error object containing information about any errors that
 *        occured. 
 * @param ftp_response
 *        A struct containing information about the ftp reply to the
 *        command that was sent.
 *
 * @return void
 *
 * @par If a error is detected in this function the user callback is
 *      called with an appropriate error object or ftp response and
 *      the function returns. 
 */

#endif

static void 
globus_l_ftp_control_send_cmd_cb(
    void *                                      callback_arg,
    globus_ftp_control_handle_t *               handle,
    globus_object_t *				error,
    globus_ftp_control_response_t *		ftp_response)
{
    globus_result_t                             rc;
    globus_i_ftp_passthru_cb_arg_t *            cb_arg;
    globus_object_t *			        error_obj;
    globus_bool_t                               call_close_cb = GLOBUS_FALSE;
    int                                         i;
    int                                         len;
    char                                        realhostname[128];
    char                                        stbuf[128+5];
    OM_uint32                                   maj_stat;
    OM_uint32                                   min_stat;
    gss_buffer_desc                             send_tok;
    gss_buffer_desc                             recv_tok;
    gss_buffer_desc *                           token_ptr;
    struct gss_channel_bindings_struct *        chan_bindings;
    char *                                      radix_buf;
    char *                                      error_str;
    OM_uint32					max_input_size[2];

    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_l_ftp_control_send_cmd_cb() entering\n"));
        
    cb_arg = (globus_i_ftp_passthru_cb_arg_t *) callback_arg;
    chan_bindings=&(handle->cc_handle.auth_info.chan_bindings);

    if(error != GLOBUS_NULL &&
       cb_arg->cmd != GLOBUS_I_FTP_QUIT)
    {
	error_obj=globus_object_copy(error);
	goto return_error;
    }

    switch(cb_arg->cmd)
    {
	
    case GLOBUS_I_FTP_AUTH:
	switch(ftp_response->response_class)
	{
	case GLOBUS_FTP_POSITIVE_INTERMEDIATE_REPLY:

	    /* follow AUTH with ADAT and set authentication variables
	     * int the control connection handle
	     */

	    cb_arg->cmd=GLOBUS_I_FTP_ADAT;

	    chan_bindings->initiator_addrtype = GSS_C_AF_INET;
	    chan_bindings->initiator_address.length = 4;
	    chan_bindings->initiator_address.value = 
		handle->cc_handle.client.h_addr;
	    chan_bindings->acceptor_addrtype = GSS_C_AF_INET;
	    chan_bindings->acceptor_address.length = 4;
	    chan_bindings->acceptor_address.value = 
		handle->cc_handle.server.h_addr;
	    chan_bindings->application_data.length = 0;
	    chan_bindings->application_data.value = 0;	    
	    
	    /* Do mutual authentication */
	    handle->cc_handle.auth_info.req_flags |= GSS_C_MUTUAL_FLAG;
	    
	    /* Do limited delegation */
	    handle->cc_handle.auth_info.req_flags |= 
		GSS_C_GLOBUS_LIMITED_DELEG_PROXY_FLAG|GSS_C_DELEG_FLAG;

	    /* use a target_name based on either a supplied subject
	     * string or the remote hostname
	     */
	    
	    
	    if(handle->cc_handle.auth_info.auth_gssapi_subject ==
	       GLOBUS_NULL)
	    {
		struct in_addr address;

		memcpy(&address.s_addr,
		       handle->cc_handle.server.h_addr,
		       sizeof(address.s_addr));

		/* 
		 * For connections to localhost, check for certificate
		 * matching our real hostname, not "localhost"
		 */
		if(ntohl(address.s_addr) == INADDR_LOOPBACK)
		{
		    globus_libc_gethostname(realhostname, sizeof(realhostname));
		}
		else
		{
		    strncpy(realhostname, handle->cc_handle.server.h_name, 
			    sizeof(realhostname));
		}

		/*
		 * To work around the GSI GSSAPI library being case sensitive
		 * convert the hostname to lower case as noone seems to
		 * request uppercase name certificates.
		 */
	    
		for (i=0; realhostname[i] && 
			 (i < sizeof(realhostname)) ; i++) {
		    realhostname[i] = tolower(realhostname[i]);
		}
	    
		/* only host@hostname works with our gssapi*/
	    
		globus_libc_sprintf(stbuf, "%s@%s","host", realhostname);
		
		send_tok.value = stbuf;
		send_tok.length = strlen(stbuf) + 1;
		maj_stat = gss_import_name(&min_stat, 
					   &send_tok, 
					   GSS_C_NT_HOSTBASED_SERVICE, 
					   &(handle->cc_handle.auth_info.
					     target_name));

	    }
	    else
	    {
		gss_OID				name_type = GSS_C_NT_USER_NAME;

		if(strstr(handle->cc_handle.auth_info.auth_gssapi_subject,
			  "host@") ||
		   strstr(handle->cc_handle.auth_info.auth_gssapi_subject,
			  "ftp@"))
		{
		    name_type = GSS_C_NT_HOSTBASED_SERVICE;
		}
		send_tok.value=handle->cc_handle.auth_info.
		    auth_gssapi_subject;
		send_tok.length=strlen(handle->cc_handle.auth_info.
				       auth_gssapi_subject) + 1;

		maj_stat = gss_import_name(&min_stat, 
					   &send_tok, 
					   name_type, 
					   &(handle->cc_handle.auth_info.
					     target_name));
		
	    }
	    
	    if(maj_stat != GSS_S_COMPLETE) 
	    {
		globus_gss_assist_display_status_str(&error_str,
						     GLOBUS_NULL,
						     maj_stat,
						     min_stat,
						     0);
						     
		error_obj=globus_error_construct_string(
		    GLOBUS_FTP_CONTROL_MODULE,
		    GLOBUS_NULL,
		    "globus_l_ftp_control_send_cmd_cb: gss_import_name failed\n\n %s",error_str);
		globus_libc_free(error_str);
		goto return_error;
	    }
	    
	    token_ptr=GSS_C_NO_BUFFER;
	    
	    if(handle->cc_handle.auth_info.encrypt)
	    {
		handle->cc_handle.auth_info.req_flags |= GSS_C_CONF_FLAG;
	    }

	    /* initialize security context 
	     */
	    maj_stat = gss_init_sec_context(&min_stat,
					    handle->cc_handle.auth_info.credential_handle,
					    &(handle->cc_handle.auth_info.
					      auth_gssapi_context),
					    handle->cc_handle.auth_info.
					    target_name,
					    GSS_C_NULL_OID,
					    handle->cc_handle.auth_info.
					    req_flags,
					    0,
					    chan_bindings,
					    token_ptr,
					    NULL,
					    &send_tok,
					    NULL,
					    NULL);
	    
	    if(maj_stat != GSS_S_COMPLETE && 
	       maj_stat != GSS_S_CONTINUE_NEEDED) 
	    {
		globus_gss_assist_display_status_str(&error_str,
						     GLOBUS_NULL,
						     maj_stat,
						     min_stat,
						     0);

		error_obj = globus_error_construct_string(
		    GLOBUS_FTP_CONTROL_MODULE,
		    GLOBUS_NULL,
		    "globus_l_ftp_control_send_cmd_cb: gss_init_sec_context failed\n\n %s",
		    error_str);
		globus_libc_free(error_str);
		goto return_error;
	    }
	    
	    len = send_tok.length;

	    /* base64 encode the token that needs to be sent to the
	     * server 
	     */
	    	    
	    radix_buf = globus_libc_malloc(send_tok.length * 8 / 6 + 4);
	    
	    if(radix_buf == GLOBUS_NULL)
	    {
		gss_release_buffer(&min_stat, &send_tok);
		error_obj = globus_error_construct_string(
		    GLOBUS_FTP_CONTROL_MODULE,
		    GLOBUS_NULL,
		    "globus_l_ftp_control_send_cmd_cb: malloc failed");
		goto return_error;
	    }

	    rc = globus_i_ftp_control_radix_encode(send_tok.value, 
						   radix_buf, 
						   &len);
	    
	    if(rc != GLOBUS_SUCCESS) 
	    {
		gss_release_buffer(&min_stat, &send_tok);
		globus_libc_free(radix_buf);
		error_obj = globus_error_get(rc);
		goto return_error;
	    }

	    /* send the initial security token to the server */

	    rc = globus_ftp_control_send_command(
		handle,"ADAT %s\r\n",
		globus_l_ftp_control_send_cmd_cb,
		callback_arg,radix_buf);
	    
	    globus_libc_free(radix_buf);
	    gss_release_buffer(&min_stat, &send_tok);
		
	    if(rc != GLOBUS_SUCCESS)
	    {
		error_obj = globus_error_get(rc);
		goto return_error;
	    }

	    break;

	case GLOBUS_FTP_UNKNOWN_REPLY:
	case GLOBUS_FTP_POSITIVE_COMPLETION_REPLY:
	case GLOBUS_FTP_POSITIVE_PRELIMINARY_REPLY:
	case GLOBUS_FTP_TRANSIENT_NEGATIVE_COMPLETION_REPLY:
	case GLOBUS_FTP_PERMANENT_NEGATIVE_COMPLETION_REPLY:

	    (cb_arg->user_cb)((cb_arg->user_cb_arg),
			      handle,
			      GLOBUS_NULL,
			      ftp_response);
	    
	    globus_libc_free(cb_arg);
	    break;
	}
	break;
    case GLOBUS_I_FTP_ADAT:
	
	switch(ftp_response->response_class)
	{
	case GLOBUS_FTP_POSITIVE_INTERMEDIATE_REPLY:
	    
	    /* base64 decode the reply */

	    ftp_response->response_buffer
		[ftp_response->response_length-2]='\0';
	    
	    len = strlen(ftp_response->response_buffer);
						
	    radix_buf = globus_libc_malloc((len + 1) * 6 / 8 + 1);
	    
	    if(radix_buf == GLOBUS_NULL)
	    {
		error_obj = globus_error_construct_string(
		    GLOBUS_FTP_CONTROL_MODULE,
		    GLOBUS_NULL,
		    "globus_l_ftp_control_send_cmd_cb: malloc failed");
		goto return_error;
	    }

	    rc = globus_i_ftp_control_radix_decode(
		ftp_response->response_buffer + strlen("335 ADAT="), 
		radix_buf, 
		&len);


	    if(rc != GLOBUS_SUCCESS)
	    {
		globus_libc_free(radix_buf);
		error_obj = globus_error_get(rc);
		goto return_error;
	    }
	    
	    recv_tok.value = radix_buf;
	    recv_tok.length = len;
	    token_ptr = &recv_tok;
	    
	    maj_stat = gss_init_sec_context(&min_stat,
					    handle->cc_handle.auth_info.credential_handle,
					    &(handle->cc_handle.auth_info.
					      auth_gssapi_context),
					    handle->cc_handle.auth_info.
					    target_name,
					    GSS_C_NULL_OID,
					    handle->cc_handle.auth_info.
					    req_flags,
					    0,
					    chan_bindings,
					    token_ptr,
					    NULL,
					    &send_tok,
					    NULL,
					    NULL);
	    
	    	    
	    if(maj_stat != GSS_S_COMPLETE && 
	       maj_stat != GSS_S_CONTINUE_NEEDED) 
	    {
		globus_gss_assist_display_status_str(&error_str,
						     GLOBUS_NULL,
						     maj_stat,
						     min_stat,
						     0);

		error_obj = globus_error_construct_string(
		    GLOBUS_FTP_CONTROL_MODULE,
		    GLOBUS_NULL,
		    "globus_l_ftp_control_send_cmd_cb: gss_init_sec_context failed\n\n %s",
		    error_str);
		globus_libc_free(error_str);
		gss_release_buffer(&min_stat, token_ptr);
		
		goto return_error;
	    }
	    
	    gss_release_buffer(&min_stat, token_ptr);

	    len = send_tok.length;
	    	    
	    radix_buf = globus_libc_malloc(send_tok.length * 8 / 6 + 4);
	    
	    if(radix_buf == GLOBUS_NULL)
	    {
		gss_release_buffer(&min_stat, &send_tok);
		error_obj = globus_error_construct_string(
		    GLOBUS_FTP_CONTROL_MODULE,
		    GLOBUS_NULL,
		    "globus_l_ftp_control_send_cmd_cb: malloc failed");
		goto return_error;
	    }

	    rc = globus_i_ftp_control_radix_encode(send_tok.value, 
						   radix_buf, 
						   &len);
	    
	    if(rc != GLOBUS_SUCCESS) 
	    {
		globus_libc_free(radix_buf);
		gss_release_buffer(&min_stat, &send_tok);
		error_obj = globus_error_get(rc);
		goto return_error;
	    }

	    rc = globus_ftp_control_send_command(
		handle,"ADAT %s\r\n",
		globus_l_ftp_control_send_cmd_cb,
		callback_arg,radix_buf);
	    
	    globus_libc_free(radix_buf);
	    gss_release_buffer(&min_stat, &send_tok);
		
	    if(rc != GLOBUS_SUCCESS)
	    {
		error_obj = globus_error_get(rc);
		goto return_error;
	    }

	    break;
	    
	case GLOBUS_FTP_POSITIVE_COMPLETION_REPLY:

	    cb_arg->cmd=GLOBUS_I_FTP_USER;
	    
	    globus_mutex_lock(&(handle->cc_handle.mutex));
	    {
		handle->cc_handle.auth_info.authenticated = GLOBUS_TRUE;
	    }
	    globus_mutex_unlock(&(handle->cc_handle.mutex));

	    gss_wrap_size_limit(
		    &min_stat,
		    &handle->cc_handle.auth_info.auth_gssapi_context,
		    0,
		    GSS_C_QOP_DEFAULT,
		    1<<30,
		    &max_input_size[0]);

	    gss_wrap_size_limit(
		    &min_stat,
		    &handle->cc_handle.auth_info.auth_gssapi_context,
		    1,
		    GSS_C_QOP_DEFAULT,
		    1<<30,
		    &max_input_size[1]);


	    if(max_input_size[0] > max_input_size[1] && max_input_size[1] != 0)
	    {
		globus_ftp_control_local_pbsz(handle,
					      max_input_size[1]);
	    }
	    else
	    {
		globus_ftp_control_local_pbsz(handle,
					      max_input_size[0]);
	    }

	    if(handle->cc_handle.auth_info.user != GLOBUS_NULL)
	    {

		rc = globus_ftp_control_send_command(
		    handle,
		    "USER %s\r\n",
		    globus_l_ftp_control_send_cmd_cb,
		    callback_arg,
		    handle->cc_handle.auth_info.user);
	    }
	    else
	    {

		rc = globus_ftp_control_send_command(
		    handle,
		    "USER :globus-mapping:\r\n",
		    globus_l_ftp_control_send_cmd_cb,
		    callback_arg);
	    }
	    
	    if(rc != GLOBUS_SUCCESS)
	    {
		error_obj = globus_error_get(rc);
		goto return_error;
	    }

	    break;

	case GLOBUS_FTP_UNKNOWN_REPLY:
	case GLOBUS_FTP_POSITIVE_PRELIMINARY_REPLY:
	case GLOBUS_FTP_TRANSIENT_NEGATIVE_COMPLETION_REPLY:
	case GLOBUS_FTP_PERMANENT_NEGATIVE_COMPLETION_REPLY:
	    
	    (cb_arg->user_cb)((cb_arg->user_cb_arg),
			      handle,
			      GLOBUS_NULL,
			      ftp_response);
	    
	    globus_libc_free(cb_arg);
	    break;
	}
	
	break;
	
    case GLOBUS_I_FTP_USER:
	switch(ftp_response->response_class)
	{
	case GLOBUS_FTP_POSITIVE_INTERMEDIATE_REPLY:

	ugly_hack:

	    cb_arg->cmd=GLOBUS_I_FTP_PASS;

	    if(handle->cc_handle.auth_info.password != GLOBUS_NULL)
	    {

		rc = globus_ftp_control_send_command(
		    handle,
		    "PASS %s\r\n",
		    globus_l_ftp_control_send_cmd_cb,
		    callback_arg,
		    handle->cc_handle.auth_info.password);
	    }
	    else
	    {
		rc = globus_ftp_control_send_command(
		    handle,
		    "PASS dummy\r\n",
		    globus_l_ftp_control_send_cmd_cb,
		    callback_arg);
	    }
	    
	    if(rc != GLOBUS_SUCCESS)
	    {
		error_obj = globus_error_get(rc);
		goto return_error;
	    }
	    
	    break;
	case GLOBUS_FTP_POSITIVE_COMPLETION_REPLY:
	    if(handle->cc_handle.auth_info.authenticated == GLOBUS_TRUE)
	    {
		goto ugly_hack;
	    }
	case GLOBUS_FTP_UNKNOWN_REPLY:
	case GLOBUS_FTP_POSITIVE_PRELIMINARY_REPLY:
	case GLOBUS_FTP_TRANSIENT_NEGATIVE_COMPLETION_REPLY:
	case GLOBUS_FTP_PERMANENT_NEGATIVE_COMPLETION_REPLY:
	    
	    (cb_arg->user_cb)((cb_arg->user_cb_arg),
			      handle,
			      GLOBUS_NULL,
			      ftp_response);
	    
	    globus_libc_free(callback_arg);
	    break;	
	}
	
	break;
	
    case GLOBUS_I_FTP_PASS:
	switch(ftp_response->response_class)
	{
	case GLOBUS_FTP_POSITIVE_INTERMEDIATE_REPLY:
	    cb_arg->cmd=GLOBUS_I_FTP_ACCT;
	    
	    if(handle->cc_handle.auth_info.account != GLOBUS_NULL)
	    {
		rc = globus_ftp_control_send_command(
		    handle,"ACCT %s\r\n",
		    globus_l_ftp_control_send_cmd_cb,
		    callback_arg,
		    handle->cc_handle.auth_info.account);
	    }
	    else
	    {
		(cb_arg->user_cb)((cb_arg->user_cb_arg),
				  handle,
				  GLOBUS_NULL,
				  ftp_response);
		
		globus_libc_free(callback_arg);
		break;	
		
	    }

	    if(rc != GLOBUS_SUCCESS)
	    {
		error_obj = globus_error_get(rc);
		goto return_error;
	    }
	    
	    break;
	    
	case GLOBUS_FTP_UNKNOWN_REPLY:
	case GLOBUS_FTP_POSITIVE_PRELIMINARY_REPLY: 
	case GLOBUS_FTP_POSITIVE_COMPLETION_REPLY:
	case GLOBUS_FTP_TRANSIENT_NEGATIVE_COMPLETION_REPLY:
	case GLOBUS_FTP_PERMANENT_NEGATIVE_COMPLETION_REPLY:
	    
	    (cb_arg->user_cb)((cb_arg->user_cb_arg),
			      handle,
			      GLOBUS_NULL,
			      ftp_response);
	    
	    globus_libc_free(callback_arg);
	    break;
	}
	
	break;
	
    case GLOBUS_I_FTP_QUIT:
	
	if(ftp_response != GLOBUS_NULL)
	{
	    rc=globus_ftp_control_response_copy(
		ftp_response,
		&handle->cc_handle.quit_response);

	    if(rc != GLOBUS_SUCCESS){
		error_obj = globus_error_get(rc);
		goto return_error;
	    }
	}

	globus_mutex_lock(&(handle->cc_handle.mutex));
	{
	    if(handle->cc_handle.cc_state == GLOBUS_FTP_CONTROL_CONNECTED)
	    {
		handle->cc_handle.cc_state = GLOBUS_FTP_CONTROL_CLOSING;
		
		globus_i_ftp_control_auth_info_destroy(
		    &(handle->cc_handle.auth_info));

		handle->cc_handle.cb_count++;

		globus_mutex_unlock(&(handle->cc_handle.mutex));
		
		rc=globus_ftp_control_data_force_close(
		    handle,
		    globus_l_ftp_control_data_close_cb,
		    (void *) handle);
		
		if(rc != GLOBUS_SUCCESS)
		{

		    rc=globus_io_register_close(&handle->cc_handle.io_handle,
						globus_l_ftp_control_close_cb,
						(void *) handle);
		    if(rc != GLOBUS_SUCCESS)
		    {
			globus_mutex_lock(&(handle->cc_handle.mutex));
			{
			    handle->cc_handle.cb_count--;
			    handle->cc_handle.close_result = 
				globus_error_get(rc);
			    if(!handle->cc_handle.cb_count)
			    {
				call_close_cb = GLOBUS_TRUE;
			    }
			}
			globus_mutex_unlock(&(handle->cc_handle.mutex));
			
			if(call_close_cb == GLOBUS_TRUE)
			{
			    globus_i_ftp_control_call_close_cb(handle);
			}
		    }
		}
	    }
	    else
	    {
		globus_mutex_unlock(&(handle->cc_handle.mutex));
		
		(cb_arg->user_cb)((cb_arg->user_cb_arg),
				  handle,
				  GLOBUS_NULL,
				  ftp_response);	
	    }
	}
	globus_mutex_unlock(&(handle->cc_handle.mutex));

	globus_libc_free(cb_arg);
	
	break;	
    }
    
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_l_ftp_control_send_cmd_cb() exiting\n"));
        
    return;

return_error:
    
    (cb_arg->user_cb)((cb_arg->user_cb_arg),
		      handle,
		      error_obj,
		      GLOBUS_NULL);

    globus_object_free(error_obj);
    globus_libc_free(callback_arg);
    
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_l_ftp_control_send_cmd_cb() exiting with error\n"));
        
    return;
    
}

#ifdef GLOBUS_INTERNAL_DOC

/**
 * Internal callback for the globus_ftp_control_data[_force]_close
 * function.      
 * 
 * This is an internal callback used as part of the
 * globus_ftp_control[_force]_close function. It checks the result of
 * the close and calls either globus_io_register_close on the control
 * connection or the user callback if the connection is already closed.
 *
 * @param arg
 *        The callback argument, which in this case is used to pass
 *        the original user callback and argument.
 * @param handle
 *        The handle for the ftp connection. 
 * @param error
 *        The result of the close operation 
 *
 * @return void
 *
 * @par If a error is detected in this function the user callback is
 *      called with an appropriate error object or ftp response and
 *      the function returns. 
 */

#endif

static void 
globus_l_ftp_control_data_close_cb(
    void *                                      arg,
    globus_ftp_control_handle_t *               handle,
    globus_object_t *				error)
{
    globus_result_t                             rc;
    globus_bool_t                               call_close_cb = GLOBUS_FALSE;
    
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_l_ftp_control_data_close_cb() entering\n"));
        
    rc=globus_io_register_close(&handle->cc_handle.io_handle,
				globus_l_ftp_control_close_cb,
				arg);
    if(rc != GLOBUS_SUCCESS)
    {
	globus_mutex_lock(&(handle->cc_handle.mutex)); 
	{ 
	    handle->cc_handle.cb_count--; 
	    handle->cc_handle.close_result =  
		globus_error_get(rc); 
	    if(!handle->cc_handle.cb_count) 
	    { 
		call_close_cb = GLOBUS_TRUE;
	    } 
	} 
	globus_mutex_unlock(&(handle->cc_handle.mutex)); 
	
	if(call_close_cb == GLOBUS_TRUE) 
	{ 
	    globus_i_ftp_control_call_close_cb(handle); 
	} 
    }
    
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_l_ftp_control_data_close_cb() exiting\n"));
    return;
}


#ifdef GLOBUS_INTERNAL_DOC

/**
 * Internal callback for the globus_io_register_close function.    
 * 
 * This is an internal callback used as part of the
 * globus_ftp_control_quit function. It checks the result of the close
 * and calls the user callback with the response of to the quit.
 *
 * @param arg
 *        The callback argument, which in this case is used to pass
 *        the original user callback and argument.
 * @param handle
 *        The globus_io handle for the connection. In practice this
 *        represents the socket fd for the connection.
 * @param result
 *        The result of the close operation 
 *
 * @return void
 *
 * @par If a error is detected in this function the user callback is
 *      called with an appropriate error object or ftp response and
 *      the function returns. 
 */

#endif

static void 
globus_l_ftp_control_close_cb(
    void *                                    arg, 
    globus_io_handle_t *                      handle,
    globus_result_t                           result)
{
    globus_ftp_cc_handle_t *                  cc_handle;
    globus_ftp_control_handle_t *             c_handle;
    globus_bool_t                             call_close_cb = GLOBUS_FALSE;

    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_l_ftp_control_close_cb() entering\n"));
        
    c_handle=(globus_ftp_control_handle_t *) arg;
    cc_handle=&(c_handle->cc_handle);

    globus_mutex_lock(&cc_handle->mutex);
    {
	cc_handle->cb_count--;
	if(!cc_handle->cb_count)
	{
	    call_close_cb = GLOBUS_TRUE;
	}
	cc_handle->close_result = globus_error_get(result);
    }
    globus_mutex_unlock(&cc_handle->mutex);

    if(call_close_cb == GLOBUS_TRUE)
    {
	globus_i_ftp_control_call_close_cb(c_handle);
    }

    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_l_ftp_control_close_cb() exiting\n"));
        
    return;
}

/**
 * Force a close of the control connection without waiting for
 * outstanding commands to complete and without sending QUIT.
 *
 * This function is used to close the control channel to the FTP server.
 * Once the final response callback passed to this function is invoked,
 * the control handle can no longer be used for any gsiftp control
 * operations.
 *
 * @note Since this function waits until all other callbacks are completed
 * before calling it's own callback it may not be called in a blocking
 * fashion from another callback.
 *
 * @param handle
 *        A pointer to a GSIFTP control handle. The quit message is
 *        issued to the server over the control channel
 *        associated with this handle.
 * @param callback
 *        The function to be called once the authentication process is 
 *        complete or when an error occurs.
 * @param callback_arg
 *        User supplied argument to the callback function
 * @return
 *        - Success
 *        - Null handle
 *
 * @par Callback errors:
 *        - success
 *        - failure
 *
 * @par Expected callback response values:
 *        - GLOBUS_NULL
 *
 */

globus_result_t
globus_ftp_control_force_close(
    globus_ftp_control_handle_t *		handle,
    globus_ftp_control_response_callback_t	callback,
    void *					callback_arg)
{
    globus_result_t                             rc;
    globus_bool_t                               connected;
    
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_ftp_control_force_close() entering\n"));
        
    globus_mutex_lock(&(handle->cc_handle.mutex));
    {
	if(handle->cc_handle.cc_state != GLOBUS_FTP_CONTROL_CONNECTED &&
	    handle->cc_handle.cc_state != GLOBUS_FTP_CONTROL_CONNECTING)
	{
	    globus_mutex_unlock(&(handle->cc_handle.mutex));
	    rc = globus_error_put(
		globus_error_construct_string(
		    GLOBUS_FTP_CONTROL_MODULE,
		    GLOBUS_NULL,
		    "globus_ftp_control_force_close: Handle is not connected")
		);
            goto return_error;
	}
	else
	{
	    if(handle->cc_handle.cc_state == GLOBUS_FTP_CONTROL_CONNECTED)
	    {
	        connected = GLOBUS_TRUE;
	    }
	    else
	    {
	        connected = GLOBUS_FALSE;
	    }
	    globus_i_ftp_control_auth_info_destroy(
		&(handle->cc_handle.auth_info));
	    handle->cc_handle.close_cb = callback;
	    handle->cc_handle.close_cb_arg = callback_arg;
	    handle->cc_handle.cc_state = GLOBUS_FTP_CONTROL_CLOSING;
	    handle->cc_handle.cb_count++;
	}
    }
    globus_mutex_unlock(&(handle->cc_handle.mutex));
    
    if(connected)
    {
    rc=globus_ftp_control_data_force_close(
	handle,
	globus_l_ftp_control_data_close_cb,
	(void *) handle);
    }
    
    
    if(!connected || rc != GLOBUS_SUCCESS)
    {
	rc=globus_io_register_close(&handle->cc_handle.io_handle,
				    globus_l_ftp_control_close_cb,
				    (void *) handle);
	if(rc != GLOBUS_SUCCESS)
	{
	    globus_mutex_lock(&(handle->cc_handle.mutex));
	    {
		handle->cc_handle.cb_count--;
		handle->cc_handle.cc_state = GLOBUS_FTP_CONTROL_UNCONNECTED;
		
		if(!handle->cc_handle.cb_count && 
		   handle->cc_handle.signal_deactivate == GLOBUS_TRUE)
		{
		    handle->cc_handle.signal_deactivate = GLOBUS_FALSE;
		    globus_cond_signal(&handle->cc_handle.cond);
		}
	    }
	    globus_mutex_unlock(&(handle->cc_handle.mutex));
	    
	    goto return_error;
	}
    }
    
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_ftp_control_force_close() exiting\n"));
        
    return GLOBUS_SUCCESS;

return_error:
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_ftp_control_force_close() exiting with error\n"));
        
    return rc;
}


#ifdef GLOBUS_INTERNAL_DOC

/**
 * Internal helper function which creates and initializes a
 * authentication information structure 
 * 
 * This is internal helper function allocates memory for a
 * auth_info_t structure and initializes it with the values contained
 * in the second argument
 *
 * @param dest
 *        This parameter is used to return the new response struct.
 * @param src
 *        A auth_info_t containing the values to initialize dest
 *        with. 
 * @return 
 *        - Error object
 *        - GLOBUS_SUCCESS
 *
 */

#endif

globus_result_t 
globus_i_ftp_control_auth_info_init(
    globus_ftp_control_auth_info_t *        dest,
    globus_ftp_control_auth_info_t *        src)
{

    if(dest == GLOBUS_NULL)
    {
	return globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_i_ftp_control_auth_info_init: Destination argument is NULL")
	    );
    }


    if(src == GLOBUS_NULL)
    {
	bzero((void *) dest,
	      sizeof(globus_ftp_control_auth_info_t));
    }
    else
    {
	dest->credential_handle = src->credential_handle;
	dest->locally_acquired_credential = GLOBUS_FALSE;
	if(src->auth_gssapi_subject != GLOBUS_NULL)
	{
	    dest->auth_gssapi_subject =
		globus_libc_strdup(src->auth_gssapi_subject);
	}
	else
	{
	    dest->auth_gssapi_subject = GLOBUS_NULL;
	}
	
	if(src->user != GLOBUS_NULL)
	{
	    dest->user = globus_libc_strdup(src->user);
	}
	else
	{
	    dest->user = GLOBUS_NULL;
	}
	
	if(src->password != GLOBUS_NULL)
	{
	    dest->password = globus_libc_strdup(src->password);
	}
	else
	{
	    dest->password = GLOBUS_NULL;
	}
	
	if(src->account != GLOBUS_NULL)
	{
	    dest->account = globus_libc_strdup(src->account);
	}
	else
	{
	    dest->account = GLOBUS_NULL;
	}
	dest->delegated_credential_handle = GSS_C_NO_CREDENTIAL;

        dest->encrypt = src->encrypt;
    }

    dest->prev_cmd=GLOBUS_FTP_CONTROL_COMMAND_UNKNOWN;    
    dest->auth_gssapi_context = GSS_C_NO_CONTEXT;
    dest->req_flags = 0;
    dest->target_name = GSS_C_NO_NAME;
    dest->authenticated = GLOBUS_FALSE;
    
    return GLOBUS_SUCCESS;
}

/**
 * Helper function which initializes a authentication information
 * structure.  
 * 
 * This is helper function initializes a authentication information
 * structure with the values contained in the second to fifth arguments,
 * which may be GLOBUS_NULL. No memory is allocated in this function. 
 *
 * @param auth_info
 *        The authentication structure to initialize.
 * @param credential_handle
 *        The credential to use for authentication. This may be
 *        GSS_C_NO_CREDENTIAL to use the user's default credential.
 * @param encrypt
 *        Boolean whether or not to encrypt the control channel for this
 *        handle.
 * @param user
 *        The user name
 * @param password
 *        The password for the user name
 * @param account
 *        The account for the user name/password
 * @param subject
 *        The gss api subject name
 * @return 
 *        - Error object 
 *        - GLOBUS_SUCCESS
 *
 */


globus_result_t 
globus_ftp_control_auth_info_init(
    globus_ftp_control_auth_info_t *       auth_info,
    gss_cred_id_t		           credential_handle,
    globus_bool_t			   encrypt,
    char *                                 user,
    char *                                 password,
    char *                                 account,
    char *                                 subject)
{
    
    if(auth_info == GLOBUS_NULL)
    {
	return globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_ftp_control_auth_info_init: auth_info argument is NULL")
	    );
    }

    if(credential_handle != GLOBUS_NULL)
    {
	auth_info->credential_handle = credential_handle;
    }
    else
    {
	auth_info->credential_handle = GSS_C_NO_CREDENTIAL;
    }
    
    auth_info->auth_gssapi_subject = subject;
    auth_info->user = user;
    auth_info->password = password;
    auth_info->account = account;

    auth_info->prev_cmd=GLOBUS_FTP_CONTROL_COMMAND_UNKNOWN;
    auth_info->auth_gssapi_context = GSS_C_NO_CONTEXT;
    auth_info->req_flags = 0;
    auth_info->target_name = GSS_C_NO_NAME;
    auth_info->authenticated = GLOBUS_FALSE;
    auth_info->locally_acquired_credential = GLOBUS_FALSE;
    auth_info->delegated_credential_handle = GSS_C_NO_CREDENTIAL;
    auth_info->encrypt = encrypt;

    return GLOBUS_SUCCESS;
}

/**
 * Helper function which compares two authentication information
 * structures.  
 * 
 * This is helper function compares two authentication information
 * structures and return zero if the two structures are deemed equal
 * and a non-zero value otherwise.
 *
 * @param auth_info_1
 *        The first authentication structure 
 * @param auth_info_2
 *        The second authentication structure 
 * @return 
 *        - 0 if the structures are equal
 *        - !0 if the structures differ or an error occured
 *
 */


int
globus_ftp_control_auth_info_compare(
    globus_ftp_control_auth_info_t *       auth_info_1,
    globus_ftp_control_auth_info_t *       auth_info_2)
{
    if(auth_info_1 == GLOBUS_NULL)
    {
	return -1;
    }

    if(auth_info_2 == GLOBUS_NULL)
    {
	return -1;
    }
    
    if(auth_info_1->auth_gssapi_subject != GLOBUS_NULL &&
       auth_info_2->auth_gssapi_subject != GLOBUS_NULL )
    {
	if(strcmp(auth_info_1->auth_gssapi_subject,
		  auth_info_2->auth_gssapi_subject))
	{
	    return -1;
	}
    }
    else if(auth_info_1->auth_gssapi_subject != GLOBUS_NULL ||
	    auth_info_2->auth_gssapi_subject != GLOBUS_NULL)
    {
	return -1;
    }

    if(auth_info_1->credential_handle != GSS_C_NO_CREDENTIAL &&
       auth_info_2->credential_handle != GSS_C_NO_CREDENTIAL)
    {
	if(auth_info_1->credential_handle !=
	   auth_info_2->credential_handle)
	{
	    return -1;
	}
    }
    else if((auth_info_1->locally_acquired_credential != GLOBUS_TRUE &&
             auth_info_1->credential_handle != GSS_C_NO_CREDENTIAL &&
	     auth_info_2->credential_handle == GSS_C_NO_CREDENTIAL) ||
	    (auth_info_1->credential_handle == GSS_C_NO_CREDENTIAL &&
	     auth_info_2->locally_acquired_credential != GLOBUS_TRUE &&
             auth_info_2->credential_handle != GSS_C_NO_CREDENTIAL))
									    
    {
	return -1;
    }
    
    if(auth_info_1->user != GLOBUS_NULL &&
       auth_info_2->user != GLOBUS_NULL )
    {
	if(strcmp(auth_info_1->user,
		  auth_info_2->user))
	{
	    return -1;
	}
    }
    else if(auth_info_1->user != GLOBUS_NULL ||
	    auth_info_2->user != GLOBUS_NULL)
    {
	return -1;
    }

    if(auth_info_1->password != GLOBUS_NULL &&
       auth_info_2->password != GLOBUS_NULL )
    {
	if(strcmp(auth_info_1->password,
		  auth_info_2->password))
	{
	    return -1;
	}
    }
    else if(auth_info_1->password != GLOBUS_NULL ||
	    auth_info_2->password != GLOBUS_NULL)
    {
	return -1;
    }

    if(auth_info_1->account != GLOBUS_NULL &&
       auth_info_2->account != GLOBUS_NULL )
    {
	if(strcmp(auth_info_1->account,
		  auth_info_2->account))
	{
	    return -1;
	}
    }
    else if(auth_info_1->account != GLOBUS_NULL ||
	    auth_info_2->account != GLOBUS_NULL)
    {
	return -1;
    }

    return 0;
}

#ifdef GLOBUS_INTERNAL_DOC

/**
 * Internal helper function which frees the memory associated with a
 * auth_info_t structure.
 * 
 * This is a internal helper function frees the memory associated with
 * a auth_info_t structure.
 *
 * @param auth_info
 *        This parameter indicates the auth_info structure to destroy
 *        and is used to set the pointer to the auth_info structure to
 *        GLOBUS_NULL  
 *
 * @return 
 *        - Error object 
 *        - GLOBUS_SUCCESS
 *
 */

#endif

globus_result_t 
globus_i_ftp_control_auth_info_destroy(
    globus_ftp_control_auth_info_t *            auth_info)
{
    OM_uint32                                   major_status;
    OM_uint32                                   minor_status;

    if( auth_info != GLOBUS_NULL)
    {
	if(auth_info->auth_gssapi_subject != GLOBUS_NULL)
	{
	    globus_libc_free(auth_info->auth_gssapi_subject);
	    auth_info->auth_gssapi_subject=GLOBUS_NULL;
	}
	
	if(auth_info->user != GLOBUS_NULL)
	{
	    globus_libc_free(auth_info->user);
	    auth_info->user=GLOBUS_NULL;
	}
	
	if(auth_info->password != GLOBUS_NULL)
	{
	    globus_libc_free(auth_info->password);
	    auth_info->password=GLOBUS_NULL;
	}
	
	if(auth_info->account != GLOBUS_NULL)
	{
	    globus_libc_free(auth_info->account);
	    auth_info->account=GLOBUS_NULL;
	}

	if(auth_info->target_name != GSS_C_NO_NAME)
	{

	    major_status=gss_release_name(&minor_status, 
					  &(auth_info->target_name));
	    
	    auth_info->target_name = GSS_C_NO_NAME;

	    if(major_status == GSS_S_FAILURE)
	    {
		return globus_error_put(
		    globus_error_construct_string(
			GLOBUS_FTP_CONTROL_MODULE,
			GLOBUS_NULL,
			"globus_i_ftp_control_auth_info_destroy: gss_release_name failed")
		    );
	    }
	    
	}

	if(auth_info->auth_gssapi_context != GSS_C_NO_CONTEXT)
	{
	    major_status=gss_delete_sec_context(&minor_status,
						&(auth_info->
						  auth_gssapi_context),
						GLOBUS_NULL);
	    
	    auth_info->auth_gssapi_context = GSS_C_NO_CONTEXT;
	    
	    if(major_status == GSS_S_FAILURE)
	    {
		return globus_error_put(
		    globus_error_construct_string(
			GLOBUS_FTP_CONTROL_MODULE,
			GLOBUS_NULL,
			"globus_i_ftp_control_auth_info_destroy: Failed to delete security context")
		    );
	    }

	}

	if(auth_info->credential_handle != GSS_C_NO_CREDENTIAL &&
	   auth_info->locally_acquired_credential)
	{
	    major_status=gss_release_cred(&minor_status, 
					  &(auth_info->credential_handle));
	    
	    auth_info->credential_handle = GSS_C_NO_CREDENTIAL;
	    auth_info->locally_acquired_credential = GLOBUS_FALSE;
	    
	    if(major_status == GSS_S_FAILURE)
	    {
		return globus_error_put(
		    globus_error_construct_string(
			GLOBUS_FTP_CONTROL_MODULE,
			GLOBUS_NULL,
			"globus_i_ftp_control_auth_info_destroy: gss_release_cred failed")
		    );
	    }
	}
	if(auth_info->delegated_credential_handle != GSS_C_NO_CREDENTIAL)
	{
	    major_status=gss_release_cred(&minor_status, 
					  &(auth_info->delegated_credential_handle));
	    
	    auth_info->delegated_credential_handle = GSS_C_NO_CREDENTIAL;
	}
    }

    auth_info->authenticated = GLOBUS_FALSE;

    return GLOBUS_SUCCESS;
}


static char *radixN =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char pad = '=';

#ifdef GLOBUS_INTERNAL_DOC

/**
 * Internal helper function which base64 encodes a given input
 * 
 * This is a internal helper function base64 encodes the first
 * "length" bytes of the given in-buffer and writes the result to the
 * out-buffer . This function assumes that the out-buffer is large
 * enough to contain the encoded in-buffer. The length of the encoded
 * inbuffer is returned through the length parameter.
 *
 * @param inbuf
 *        The input buffer to encode
 * @param outbuf
 *        The buffer in which the encoded input buffer is stored
 * @param length
 *        Initially the length of the input. Used to return the lenght
 *        of the output.
 *
 * @return 
 *        - Error object 
 *        - GLOBUS_SUCCESS
 *
 */

#endif

globus_result_t
globus_i_ftp_control_radix_encode(
    unsigned char *                        inbuf,
    unsigned char *                        outbuf,
    int *                                  length)
{
    int                                    i;
    int                                    j;
    unsigned char                          c;
    
    for (i=0,j=0; i < *length; i++)
    {
	switch (i%3) 
	{
	case 0:
	    outbuf[j++] = radixN[inbuf[i]>>2];
	    c = (inbuf[i]&3)<<4;
	    break;
	case 1:
	    outbuf[j++] = radixN[c|inbuf[i]>>4];
	    c = (inbuf[i]&15)<<2;
	    break;
	case 2:
	    outbuf[j++] = radixN[c|inbuf[i]>>6];
	    outbuf[j++] = radixN[inbuf[i]&63];
	    c = 0;
	}
    }
    
    if (i%3) 
    {
	outbuf[j++] = radixN[c];
    }
    
    switch (i%3) 
    {
    case 1: 
	outbuf[j++] = pad;
    case 2: 
	outbuf[j++] = pad;
    }
    
    outbuf[*length = j] = '\0';
    
    return GLOBUS_SUCCESS;
}

#ifdef GLOBUS_INTERNAL_DOC

/**
 * Internal helper function which base64 decodes a given input
 * 
 * This is a internal helper function base64 decodes the given
 * in-buffer and writes the result to the out-buffer . This function
 * assumes that the out-buffer is large enough to contain the decoded
 * in-buffer. The length of the decoded inbuffer is returned through
 * the length parameter. 
 *
 * @param inbuf
 *        The input buffer to decode
 * @param outbuf
 *        The buffer in which the decoded input buffer is stored
 * @param length
 *        Used to return the lenght of the output.
 *
 * @return 
 *        - Error Object
 *        - GLOBUS_SUCCESS
 *
 */

#endif

globus_result_t
globus_i_ftp_control_radix_decode(
    unsigned char *                        inbuf,
    unsigned char *                        outbuf,
    int *                                  length)
{
    int                                    i;
    int                                    j;
    int                                    D;
    char *                                 p;

    for (i=0,j=0; inbuf[i] && inbuf[i] != pad; i++) 
    {

	if ((p = strchr(radixN, inbuf[i])) == NULL) 
	{
	    return globus_error_put(
		globus_error_construct_string(
		    GLOBUS_FTP_CONTROL_MODULE,
		    GLOBUS_NULL,
		    "globus_i_ftp_control_radix_decode: Character not in charset")
		);
	}

	D = p - radixN;
	switch (i&3) 
	{
	case 0:
	    outbuf[j] = D<<2;
	    break;
	case 1:
	    outbuf[j++] |= D>>4;
	    outbuf[j] = (D&15)<<4;
	    break;
	case 2:
	    outbuf[j++] |= D>>2;
	    outbuf[j] = (D&3)<<6;
	    break;
	case 3:
	    outbuf[j++] |= D;
	}
    }
    switch (i&3) 
    {
    case 1: 
	return globus_error_put(
		globus_error_construct_string(
		    GLOBUS_FTP_CONTROL_MODULE,
		    GLOBUS_NULL,
		    "globus_i_ftp_control_radix_decode: Padding error")
		);
    case 2: 
	if (D&15)
	{
	    return globus_error_put(
		globus_error_construct_string(
		    GLOBUS_FTP_CONTROL_MODULE,
		    GLOBUS_NULL,
		    "globus_i_ftp_control_radix_decode: Padding error")
		);
	}
	if (strcmp((char *)&inbuf[i], "=="))
	{
	    return globus_error_put(
		globus_error_construct_string(
		    GLOBUS_FTP_CONTROL_MODULE,
		    GLOBUS_NULL,
		    "globus_i_ftp_control_radix_decode: Padding error")
		);
	}
	break;
    case 3: 
	if (D&3) 
	{
	    return  globus_error_put(
		globus_error_construct_string(
		    GLOBUS_FTP_CONTROL_MODULE,
		    GLOBUS_NULL,
		    "globus_i_ftp_control_radix_decode: Padding error")
		);
	}
	if (strcmp((char *)&inbuf[i], "=")) 
	{
	    return  globus_error_put(
		globus_error_construct_string(
		    GLOBUS_FTP_CONTROL_MODULE,
		    GLOBUS_NULL,
		    "globus_i_ftp_control_radix_decode: Padding error")
		);
	}
    }
    *length = j;

    return GLOBUS_SUCCESS;
}

#ifdef GLOBUS_INTERNAL_DOC

/**
 * Internal helper function which gss wraps, base 64 encodes and puts
 * a MIC in front of the encoded string
 * 
 * Internal helper function which gss wraps, base 64 encodes and puts
 * a MIC in front of the encoded string
 *
 * @param cc_handle
 *        A control connection handle.
 * @param cmd
 *        A string representing the command to encode.
 * @param encoded_cmd
 *        Used to return the encoded command. Memory for the encoded
 *        command is allocated in this function.
 *
 * @return 
 *        - Error object
 *        - GLOBUS_SUCCESS
 *
 */

#endif


globus_result_t
globus_i_ftp_control_encode_command(
    globus_ftp_cc_handle_t *               cc_handle,
    char *                                 cmd,
    char **                                encoded_cmd)
{
    gss_buffer_desc                        in_buf;
    gss_buffer_desc                        out_buf;
    OM_uint32                              maj_stat;
    OM_uint32                              min_stat;
    int                                    conf_state;


    if(cc_handle == GLOBUS_NULL ||
       cmd == GLOBUS_NULL ||
       encoded_cmd == GLOBUS_NULL)
    {
	return globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_i_ftp_control_encode_command: NULL argument detected")
	    );
    }
    
    in_buf.value = cmd;
    in_buf.length = strlen(cmd)+1;

    maj_stat = gss_wrap(&min_stat,
			cc_handle->auth_info.auth_gssapi_context,
			0,
			GSS_C_QOP_DEFAULT,
			&in_buf, 
			&conf_state,
			&out_buf);
    
    if(maj_stat != GSS_S_COMPLETE) 
    {
	return globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_i_ftp_control_encode_command: gss_wrap failed")
	    );
    }

    *encoded_cmd = (char *) globus_libc_malloc((out_buf.length + 3) * 8 / 6 + 9);

    if(*encoded_cmd == GLOBUS_NULL)
    {
	gss_release_buffer(&min_stat, &out_buf);
	return globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_i_ftp_control_encode_command: malloc failed")
	    );
    }

    if(conf_state == 0)
    {
	(*encoded_cmd)[0]='M';
	(*encoded_cmd)[1]='I';
	(*encoded_cmd)[2]='C';
	(*encoded_cmd)[3]=' ';
    }
    else
    {
	(*encoded_cmd)[0]='E';
	(*encoded_cmd)[1]='N';
	(*encoded_cmd)[2]='C';
	(*encoded_cmd)[3]=' ';
    }

    globus_i_ftp_control_radix_encode(out_buf.value,
				      &((*encoded_cmd)[4]), 
				      &out_buf.length);

    (*encoded_cmd)[out_buf.length+4]='\r';
    (*encoded_cmd)[out_buf.length+5]='\n';
    (*encoded_cmd)[out_buf.length+6]='\0';

    gss_release_buffer(&min_stat, &out_buf);
    
    return GLOBUS_SUCCESS;
}

#ifdef GLOBUS_INTERNAL_DOC

/**
 * Internal helper function which sets up a list for keeping track of
 * control connection handles and which opens /dev/null which is used
 * for checking the length of commands and responses.
 * 
 * Internal helper function which sets up a list for keeping track of
 * control connection handles and which opens /dev/null which is used
 * for checking the length of commands and responses.
 *
 *
 * @return 
 *        - Error object 
 *        - GLOBUS_SUCCESS
 *
 */

#endif


globus_result_t
globus_i_ftp_control_client_activate(void)
{
    globus_result_t                     result;
    
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_i_ftp_control_client_activate() entering\n"));
        
    globus_mutex_init(
	&(globus_l_ftp_cc_handle_list_mutex), GLOBUS_NULL);

    globus_i_ftp_control_devnull=fopen("/dev/null","w"); 

    if (globus_i_ftp_control_devnull == NULL)
    {
	result = globus_error_put(
	    globus_error_construct_string(
		GLOBUS_FTP_CONTROL_MODULE,
		GLOBUS_NULL,
		"globus_i_ftp_control_client_activate: Failed to open /dev/null")
	    );
	goto return_error;
    }
    
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_i_ftp_control_client_activate() exiting\n"));
        
    return GLOBUS_SUCCESS;

return_error:
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_i_ftp_control_client_activate() exiting with error\n"));
    
    return result;
}

#ifdef GLOBUS_INTERNAL_DOC

/**
 *
 * Internal helper function which deactivates any control connections
 *
 * Internal helper function which goes through a list of control
 * connection handles, closes any open connections associated with the
 * handles and deallocates any memory allocated to these handles. It
 * also closes /dev/null
 * 
 * @return 
 *        - Error Object
 *        - GLOBUS_SUCCESS
 *
 */

#endif

globus_result_t
globus_i_ftp_control_client_deactivate(void)
{
    globus_ftp_cc_handle_t *            cc_handle;
    
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_i_ftp_control_client_deactivate() entering\n"));
        
    globus_mutex_lock(&globus_l_ftp_cc_handle_list_mutex);
    {
	while(!globus_list_empty(globus_l_ftp_cc_handle_list))
	{
	    cc_handle=(globus_ftp_cc_handle_t *)
		globus_list_first(globus_l_ftp_cc_handle_list);

	    globus_mutex_lock(&(cc_handle->mutex));

	    cc_handle->signal_deactivate = GLOBUS_TRUE;
	    
	    if(cc_handle->cc_state != GLOBUS_FTP_CONTROL_UNCONNECTED)
	    {
		if(cc_handle->cc_state == GLOBUS_FTP_CONTROL_CONNECTED ||
		    cc_handle->cc_state == GLOBUS_FTP_CONTROL_CONNECTING)
		{
		    if(cc_handle->cb_count)
		    {
			cc_handle->cc_state = GLOBUS_FTP_CONTROL_CLOSING;
			while(cc_handle->signal_deactivate == GLOBUS_TRUE)
			{
			    globus_cond_wait(&cc_handle->cond, 
					     &cc_handle->mutex);
			}
		    }
		    else
		    {
			cc_handle->cc_state = GLOBUS_FTP_CONTROL_UNCONNECTED;
		    }
		    globus_mutex_unlock(&(cc_handle->mutex));
		    globus_io_close(&(cc_handle->io_handle));
		}
		else 
		    /* in the closing state */
		{
		    while(cc_handle->signal_deactivate == GLOBUS_TRUE)
		    {
			globus_cond_wait(&cc_handle->cond,
					 &cc_handle->mutex);
		    }
		    globus_mutex_unlock(&(cc_handle->mutex));
		}
	    }
	    else
	    {
		globus_mutex_unlock(&(cc_handle->mutex));
	    }
	    
	    
	    globus_libc_free(cc_handle->response.response_buffer);
	    globus_i_ftp_control_auth_info_destroy(&(cc_handle->auth_info));
	    globus_mutex_destroy(&(cc_handle->mutex));
	    globus_cond_destroy(&(cc_handle->cond));
	    globus_libc_free(cc_handle->read_buffer);
	    globus_ftp_control_response_destroy(&cc_handle->quit_response);

	    if(!cc_handle->close_result)
	    {
		globus_object_free(cc_handle->close_result);
	    }

	    globus_list_remove(&globus_l_ftp_cc_handle_list,
			       globus_l_ftp_cc_handle_list);
	}
    }
    globus_mutex_unlock(&globus_l_ftp_cc_handle_list_mutex);

    globus_mutex_destroy(&globus_l_ftp_cc_handle_list_mutex);

    fclose(globus_i_ftp_control_devnull);
    
    globus_i_ftp_control_debug_printf(1,
        (stderr, "globus_i_ftp_control_client_deactivate() exiting\n"));
        
    return GLOBUS_SUCCESS;
}


globus_result_t
globus_i_ftp_control_client_get_connection_info(
    globus_ftp_control_handle_t *         handle,
    int                                   localhost[4],
    unsigned short *                      localport,
    int                                   remotehost[4],
    unsigned short *                      remoteport)
{
    globus_result_t                       result = 
	globus_error_put(GLOBUS_ERROR_NO_INFO);

    globus_mutex_lock(&(handle->cc_handle.mutex));
    {
        if(handle->cc_handle.cc_state == GLOBUS_FTP_CONTROL_CONNECTED)
        {
            if(localhost != GLOBUS_NULL &&
               localport != GLOBUS_NULL)
            {
                result = globus_io_tcp_get_local_address(
                             &handle->cc_handle.io_handle,
                             localhost,
                             localport);
            }
            
            if(remotehost != GLOBUS_NULL &&
               remoteport != GLOBUS_NULL)
            {
                result = globus_io_tcp_get_local_address(
                             &handle->cc_handle.io_handle,
                             remotehost,
                             remoteport);
            }
        }
    }
    globus_mutex_unlock(&(handle->cc_handle.mutex));
 
    return result;
}


void 
globus_i_ftp_control_write_next(
    globus_ftp_control_handle_t *             handle)
{
    globus_object_t *                         error;
    globus_result_t                           rc;
    globus_ftp_control_rw_queue_element_t *   element;
    globus_bool_t                             queue_empty = GLOBUS_FALSE;
    globus_bool_t                             call_close_cb = GLOBUS_FALSE;

    rc = (globus_result_t) 1;

    while(queue_empty == GLOBUS_FALSE &&
	  rc != GLOBUS_SUCCESS)
    {
	/* queue was not empty, we need to do the next write/send */
	
	element = globus_fifo_peek(&handle->cc_handle.writers);
	
	rc = globus_io_register_send(&handle->cc_handle.io_handle,
				     element->write_buf,
				     (globus_size_t) strlen(
					 element->write_buf),
				     element->write_flags,
				     element->write_callback,
				     (void *) handle);
	
	if(rc != GLOBUS_SUCCESS)
	{
	    error=globus_error_get(rc);;

	    if(element->expect_response == GLOBUS_TRUE)
	    {
		if(element->callback)
		{
		    (element->callback)((element->arg),
					handle,
					error,
					GLOBUS_NULL);
		}
		else
		{
		    (element->send_response_cb)((element->arg),
						handle,
						error);
		}
	    }

	    globus_mutex_lock(&(handle->cc_handle.mutex));
	    {
		globus_fifo_dequeue(&handle->cc_handle.writers);
		handle->cc_handle.cb_count--;
		queue_empty=globus_fifo_empty(&handle->cc_handle.writers);

		if(!handle->cc_handle.cb_count &&  
		   handle->cc_handle.cc_state == GLOBUS_FTP_CONTROL_CLOSING) 
		{ 
		    call_close_cb = GLOBUS_TRUE; 
		} 
	    }
	    globus_mutex_unlock(&(handle->cc_handle.mutex));
	
	    if(call_close_cb == GLOBUS_TRUE) 
	    { 
		globus_i_ftp_control_call_close_cb(handle);
	    } 

	    globus_libc_free(element->write_buf);
	    globus_object_free(error);
	    globus_libc_free(element);
	    
	}
    }
    return;
}

static void 
globus_l_ftp_control_read_next(
    globus_ftp_control_handle_t *             handle)
{
    globus_object_t *                         error;
    globus_result_t                           rc;
    globus_ftp_control_rw_queue_element_t *   element;
    globus_bool_t                             queue_empty = GLOBUS_FALSE;
    globus_bool_t                             call_close_cb = GLOBUS_FALSE;

    do
    {
	/* queue was not empty, we need to do the next read */
	
	element = globus_fifo_peek(&handle->cc_handle.readers);

	rc=globus_io_register_read(&handle->cc_handle.io_handle,
				   handle->cc_handle.read_buffer,
				   GLOBUS_FTP_CONTROL_READ_BUFFER_SIZE,
				   1, /* 0 or 1 here ? */
				   element->read_callback,
				   (void *) handle);
	
	if(rc != GLOBUS_SUCCESS)
	{
	    error=globus_error_get(rc);
	    
	
	    (element->callback)((element->arg),
				handle,
				error,
				GLOBUS_NULL);

	    globus_mutex_lock(&(handle->cc_handle.mutex));
	    {
		globus_fifo_dequeue(&handle->cc_handle.readers);
		handle->cc_handle.cb_count--;
		queue_empty=globus_fifo_empty(&handle->cc_handle.readers);
		if(!handle->cc_handle.cb_count &&  
		   handle->cc_handle.cc_state == GLOBUS_FTP_CONTROL_CLOSING) 
		{ 
		    call_close_cb = GLOBUS_TRUE; 
		} 
	    }
	    globus_mutex_unlock(&(handle->cc_handle.mutex));

	    if(call_close_cb == GLOBUS_TRUE) 
	    { 
		globus_i_ftp_control_call_close_cb(handle);
	    } 

	    globus_libc_free(element);
	    globus_object_free(error);
	}
    } 
    while(queue_empty == GLOBUS_FALSE &&
	  rc != GLOBUS_SUCCESS);
    return;

}

static globus_result_t
globus_l_ftp_control_queue_element_init(
    globus_ftp_control_rw_queue_element_t *     element,
    globus_ftp_control_response_callback_t	callback,
    void *					arg,
    globus_byte_t *                             write_buf,
    int                                         write_flags,
    globus_io_write_callback_t                  write_callback,
    globus_io_read_callback_t                   read_callback,
    globus_bool_t                               expect_response,
    globus_bool_t                               use_auth,
    globus_ftp_control_handle_t *               handle)
{
    globus_result_t                             result = GLOBUS_SUCCESS;


    element->callback=callback;
    element->arg=arg;
    element->write_flags = write_flags;

    if(use_auth == GLOBUS_TRUE)
    {
	result=globus_i_ftp_control_encode_command(
	    &handle->cc_handle,
	    write_buf,
	    (char **) &element->write_buf);	
    }
    else
    {

	element->write_buf = globus_libc_strdup(write_buf);
	
	if(element->write_buf == GLOBUS_NULL)
	{
	    result = globus_error_put(
		globus_error_construct_string(
		    GLOBUS_FTP_CONTROL_MODULE,
		    GLOBUS_NULL,
		    "globus_l_ftp_control_queue_element_init: strdup failed")
		); 
	}
    }

    element->write_callback = write_callback;
    element->read_callback = read_callback;
    element->expect_response = expect_response; 
    
    return result;
}

void
globus_i_ftp_control_call_close_cb(
    globus_ftp_control_handle_t *             handle)
{

    if(handle->cc_handle.close_cb != GLOBUS_NULL)
    {
	(handle->cc_handle.close_cb)(
	    handle->cc_handle.close_cb_arg,
	    handle,
	    handle->cc_handle.close_result,
	    &handle->cc_handle.quit_response);
    }
    
    globus_mutex_lock(&handle->cc_handle.mutex);
    {
	handle->cc_handle.cc_state = GLOBUS_FTP_CONTROL_UNCONNECTED;
	
	if(handle->cc_handle.signal_deactivate == GLOBUS_TRUE)
	{
	    handle->cc_handle.signal_deactivate = GLOBUS_FALSE;
	    globus_cond_signal(&handle->cc_handle.cond);
	}
    }
    globus_mutex_unlock(&handle->cc_handle.mutex);
}
