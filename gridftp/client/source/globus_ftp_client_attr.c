#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file globus_ftp_client_attr.c Operation and handle attribute manipulation.
 *
 * $RCSfile$
 * $Revision$
 * $Date$
 */
#endif

#include "globus_i_ftp_client.h"
#include "globus_error_string.h"

#include <string.h>

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
static
int
globus_l_ftp_client_plugin_list_search(void * datum,
				       void * arg);
#endif

/**
 * @name Initialize
 */
/*@{*/
/**
 * Initialize an FTP client handle attribute set.
 * @ingroup globus_ftp_client_handleattr
 *
 * This function creates an empty FTP Client handle attribute set. This
 * function must be called on each attribute set before any of the
 * other functions in this section may be called.
 *
 * @param attr
 *        The new handle attribute.
 *
 * @see globus_ftp_client_handleattr_destroy()
 */
globus_result_t
globus_ftp_client_handleattr_init(
    globus_ftp_client_handleattr_t *		attr)
{
    globus_i_ftp_client_handleattr_t *		i_attr;
    static char * myname = "globus_ftp_client_handleattr_init";

    if(attr == GLOBUS_NULL)
    {
	return globus_error_put(globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot initialize NULL attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname));
    }
    i_attr = globus_libc_calloc(1, sizeof(globus_i_ftp_client_handleattr_t));

    if(i_attr == GLOBUS_NULL)
    {
	return globus_error_put(globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Out of memory at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname));
    }
    i_attr->nl_handle = GLOBUS_NULL;
    *attr = i_attr;
    
    return GLOBUS_SUCCESS;
}
/* globus_ftp_client_handleattr_init() */
/*@}*/

/**
 * @name Destroy
 */
/*@{*/
/**
 * Destroy an FTP client handle attribute set.
 * @ingroup globus_ftp_client_handleattr
 *
 * This function destroys an ftp client handle attribute set. All
 * attributes on this set will be lost. The user must call
 * globus_ftp_client_handleattr_init() again on this attribute set
 * before calling any other handle attribute functions on it.
 *
 * @param attr
 *        The attribute set to destroy.
 */
globus_result_t
globus_ftp_client_handleattr_destroy(
    globus_ftp_client_handleattr_t *		attr)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_i_ftp_client_handleattr_t *		i_attr;

    static char * myname = "globus_ftp_client_handleattr_destroy";

    if(attr == GLOBUS_NULL)
    {
	return globus_error_put(globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot destroy NULL attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname));
    }
    i_attr = *(globus_i_ftp_client_handleattr_t **) attr;

    globus_i_ftp_client_cache_destroy(&i_attr->url_cache);

    while(! globus_list_empty(i_attr->plugins))
    {
	plugin = globus_list_first(i_attr->plugins);
	globus_list_remove(&i_attr->plugins, i_attr->plugins);
	plugin->destroy_func(plugin->plugin,
			     plugin->plugin_specific);
    }
    globus_libc_free(i_attr);
    (*attr) = GLOBUS_NULL;

    return GLOBUS_SUCCESS;
}
/* globus_ftp_client_handleattr_destroy() */
/*@}*/

/**
 * @name Copy
 */
/* @{ */
/**
 * Create a duplicate of a handle attribute set.
 * @ingroup globus_ftp_client_handleattr
 *
 * The duplicated attribute set has a deep copy of all data in the
 * attribute set, so the original may be destroyed while the copy is
 * still valid.
 *
 * @param dest
 *        The attribute set to be initialized to the same values as
 *        src. 
 * @param src
 *        The original attribute set to duplicate.
 */
globus_result_t
globus_ftp_client_handleattr_copy(
    globus_ftp_client_handleattr_t *		dest,
    globus_ftp_client_handleattr_t *		src)
{
    globus_result_t				result;
    static char * myname = "globus_i_ftp_client_handleattr_copy";
    
    if(dest == GLOBUS_NULL || src == GLOBUS_NULL)
    {
	return globus_error_put(globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot copy NULL attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname));
    }
    result = globus_ftp_client_handleattr_init(dest);

    if(result != GLOBUS_SUCCESS)
    {
	return globus_error_put(globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		globus_error_get(result),
		"[%s] Cannot initialize attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname));
    }

    return globus_i_ftp_client_handleattr_copy(
	    *(globus_i_ftp_client_handleattr_t **) dest,
	    *(globus_i_ftp_client_handleattr_t **) src);
}
/* globus_ftp_client_handleattr_copy() */
/* @} */


/**
 * @name Connection Caching
 */
/*@{*/
/**
 * Set/Get the cache all connections attribute for an ftp client
 * handle attribute set.
 * @ingroup globus_ftp_client_handleattr
 *
 * This attribute allows the user to cause all control connections to
 * be cached between ftp operations. When this is enabled, the user 
 * skips the authentication handshake and connection establishment
 * overhead for multiple subsequent ftp operations to the same server.
 *
 * Memory and network connections associated with the caching will
 * be used until the handle is destroyed.  If fine grained caching is
 * needed, then the user should disable  this attribute and explicitly
 * cache specific URLs.
 *
 * @param attr
 *        Attribute to query or modify.
 * @param cache_all
 *        Value of the cache_all attribute.
 *
 * @see globus_ftp_client_handleattr_add_cached_url(),
 *      globus_ftp_client_handleattr_remove_cached_url(),
 *      globus_ftp_client_handle_cache_url_state()
 *      globus_ftp_client_handle_flush_url_state()
 */
globus_result_t
globus_ftp_client_handleattr_set_cache_all(
    globus_ftp_client_handleattr_t *		attr,
    globus_bool_t				cache_all)
{
    globus_object_t *				err = GLOBUS_SUCCESS;
    globus_i_ftp_client_handleattr_t *		i_attr;
    static char * myname = "globus_ftp_client_handleattr_set_cache_all";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot modify NULL attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);
	goto error_exit;
    }
    i_attr = *(globus_i_ftp_client_handleattr_t **) attr;

    i_attr->cache_all = cache_all;

    return GLOBUS_SUCCESS;

 error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_handleattr_set_cache_all() */

globus_result_t
globus_ftp_client_handleattr_get_cache_all(
    const globus_ftp_client_handleattr_t *	attr,
    globus_bool_t *				cache_all)
{
    const globus_i_ftp_client_handleattr_t *	i_attr;
    globus_object_t *				err = GLOBUS_SUCCESS;
    static char * myname = "globus_ftp_client_handleattr_get_cache_all";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot modify NULL attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);
	goto error_exit;
    }
    if(cache_all == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] NULL cache_all parameter at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }
    i_attr = *(const globus_i_ftp_client_handleattr_t **) attr;
    (*cache_all) = i_attr->cache_all;

    return GLOBUS_SUCCESS;
 error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_handleattr_get_cache_all() */
/*@}*/

/**
 * @name URL Caching
 */
/*@{*/
/**
 * Enable/Disable caching for a specific URL.
 * @ingroup globus_ftp_client_handleattr
 *
 * This function adds/removes the specified URL into the default cache for
 * a handle attribute. Handles initialized with this attr will
 * keep connections to FTP servers associated with the URLs in its cache
 * open between @link globus_ftp_client_operations operations @endlink.
 *
 * @param attr
 *        Attribute to modify
 * @param url
 *        URL string to cache
 */
globus_result_t
globus_ftp_client_handleattr_add_cached_url(
    globus_ftp_client_handleattr_t *		attr,
    const char *				url)
{
    globus_i_ftp_client_handleattr_t *		i_attr;
    static char * myname = "globus_ftp_client_handleattr_add_cached_url";

    if(attr == GLOBUS_NULL)
    {
	return globus_error_put(
		globus_error_construct_string(
		    GLOBUS_FTP_CLIENT_MODULE,
		    GLOBUS_NULL,
		    "[%s] Cannot modify NULL attribute at %s\n",
		    GLOBUS_FTP_CLIENT_MODULE->module_name,
		    myname));
    }

    i_attr = *(globus_i_ftp_client_handleattr_t **) attr;

    return globus_i_ftp_client_cache_add(&i_attr->url_cache, url);
}
/* globus_ftp_client_handleattr_add_cached_url() */

/**
 * @ingroup globus_ftp_client_handleattr
 */
globus_result_t
globus_ftp_client_handleattr_remove_cached_url(
    globus_ftp_client_handleattr_t *		attr,
    const char *				url)
{
    globus_i_ftp_client_handleattr_t *		i_attr;
    static char * myname = "globus_ftp_client_handleattr_remove_cached_url";

    if(attr == GLOBUS_NULL)
    {
	return globus_error_put(
		globus_error_construct_string(
		    GLOBUS_FTP_CLIENT_MODULE,
		    GLOBUS_NULL,
		    "[%s] Cannot modify NULL attribute at %s\n",
		    GLOBUS_FTP_CLIENT_MODULE->module_name,
		    myname));
    }

    i_attr = *(globus_i_ftp_client_handleattr_t **) attr;

    return globus_i_ftp_client_cache_remove(&i_attr->url_cache, url);
}
/* globus_ftp_client_handleattr_remove_cached_url() */
/*@}*/

/**
 * @name Netlogger management
 */
/**
 * Set the netlogger handle used with this transfer.
 * @ingroup globus_ftp_client_handleattr
 *
 * Each handle can have a netlogger handle associated with it
 * for logging its data.
 *
 * Only 1 netlogger handle can be associated with a client handle.
 *
 * @param attr
 *        The attribute set to modify.
 * @param nl_handle
 *        The open netlogger handle to be associated with this
 *        attribute set.
 */
globus_result_t
globus_ftp_client_handleattr_set_netlogger(
    globus_ftp_client_handleattr_t *            attr,
    globus_netlogger_handle_t *                 nl_handle)
{
    globus_object_t *                           err = GLOBUS_SUCCESS;
    globus_i_ftp_client_handleattr_t *          i_attr;
    static char * myname = "globus_ftp_client_handleattr_set_netlogger";

    if(attr == GLOBUS_NULL)
    {
        err = globus_error_construct_string(
                GLOBUS_FTP_CLIENT_MODULE,
                GLOBUS_NULL,
                "[%s] Cannot modify NULL attribute at %s\n",
                GLOBUS_FTP_CLIENT_MODULE->module_name,
                myname);
        return globus_error_put(err);
    }
    if(nl_handle == GLOBUS_NULL)
    {
        err = globus_error_construct_string(
                GLOBUS_FTP_CLIENT_MODULE,
                GLOBUS_NULL,
                "[%s] Cannot set NULL netlogger handle at %s\n",
                GLOBUS_FTP_CLIENT_MODULE->module_name,
                myname);
        return globus_error_put(err);
    }
    i_attr = *(globus_i_ftp_client_handleattr_t **) attr;

    i_attr->nl_handle = nl_handle;

    return GLOBUS_SUCCESS;
}

/**
 * @name Plugin Management
 */
/*@{*/
/**
 * Add/Remove a plugin to a handle attribute set.
 * @ingroup globus_ftp_client_handleattr
 *
 * Each handle attribute set contains a list of plugins associated with
 * it. When a handle is created with a particular attribute set, it
 * will be associated with a copy of those plugins.
 *
 * Only one instance of a specific plugin may be added to an attribute
 * set. Each plugin must have a different name.
 *
 * A copy of the plugin is created via the plugins 'copy' method when
 * it is added to an attribute set. Thus, any changes to a particular
 * plugin must be done before the plugin is added to an attribute set,
 * and before the attribute set is used to create handles.
 *
 * @param attr
 *        The attribute set to modify.
 * @param plugin
 *        The plugin to add or remove from the list.
 */
globus_result_t
globus_ftp_client_handleattr_add_plugin(
    globus_ftp_client_handleattr_t *		attr,
    globus_ftp_client_plugin_t *		plugin)
{
    globus_ftp_client_plugin_t *		tmp;
    globus_list_t *				node;
    globus_object_t *				err = GLOBUS_SUCCESS;
    globus_i_ftp_client_handleattr_t *		i_attr;
    static char * myname = "globus_ftp_client_handleattr_add_plugin";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot modify NULL attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);
	goto error_exit;
    }
    if(plugin == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot add NULL plugin at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);
	goto error_exit;
    }
    if(*plugin == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot add invalid plugin at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);
	goto error_exit;
    }
    if((*plugin)->plugin_name == GLOBUS_NULL ||
       (*plugin)->copy_func == GLOBUS_NULL ||
       (*plugin)->destroy_func == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot add invali plugin at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);
	goto error_exit;
    }
    i_attr = *(globus_i_ftp_client_handleattr_t **) attr;

    node = globus_list_search_pred(i_attr->plugins,
				   globus_l_ftp_client_plugin_list_search,
				   (*plugin)->plugin_name);

    if(node)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Plugin %s already associated with attribute set at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		(*plugin)->plugin_name,
		myname);
	goto error_exit;
    }
    else
    {
	tmp = (*plugin)->copy_func(plugin,
		                   (*plugin)->plugin_specific);

	if(tmp)
	{
	    (*tmp)->plugin = tmp;
	    globus_list_insert(&i_attr->plugins, *tmp);
	}
	else
	{
	    err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Plugin %s already associated with attribute set at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		(*plugin)->plugin_name,
		myname);
	    goto error_exit;
	}
    }
    return GLOBUS_SUCCESS;

 error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_handleattr_add_plugin() */

globus_result_t
globus_ftp_client_handleattr_remove_plugin(
    globus_ftp_client_handleattr_t *		attr,
    globus_ftp_client_plugin_t *		plugin)
{
    globus_list_t *				node;
    globus_i_ftp_client_plugin_t *		tmp;
    globus_object_t *				err = GLOBUS_SUCCESS;
    globus_i_ftp_client_handleattr_t *		i_attr;
    static char * myname = "globus_ftp_client_handleattr_remove_plugin()";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot modify NULL attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }
    else if(plugin == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot add NULL plugin at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);
	goto error_exit;
    }
    else if((*plugin)->plugin_name == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot remove invalid plugin at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);
	goto error_exit;
    }
    i_attr = *(globus_i_ftp_client_handleattr_t **) attr;
    node = globus_list_search_pred(i_attr->plugins,
				   globus_l_ftp_client_plugin_list_search,
				   (*plugin)->plugin_name);

    if(!node)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Plugin %s not associated with attribute set at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    (*plugin)->plugin_name,
	    myname);

	goto error_exit;
    }
    tmp = globus_list_first(node);
	
    globus_list_remove(&i_attr->plugins, node);
    tmp->destroy_func(tmp->plugin,
	              tmp->plugin_specific);

    return GLOBUS_SUCCESS;

 error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_handleattr_remove_plugin() */
/*@}*/

/**
 * Initialize an FTP client attribute set.
 * @ingroup globus_ftp_client_operationattr
 *
 * @param attr
 *        A pointer to the new attribute set.
 */
globus_result_t
globus_ftp_client_operationattr_init(
    globus_ftp_client_operationattr_t *		attr)
{
    char *					tmp_name;
    char *					tmp_pass;
    globus_object_t *				err;
    globus_result_t				result;
    globus_i_ftp_client_operationattr_t *	i_attr;
    static char * myname = "globus_ftp_client_operationattr_init";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot initialize NULL attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);
	goto error_exit;
    }

    i_attr = globus_libc_calloc(1, sizeof(globus_i_ftp_client_operationattr_t));

    if(i_attr == GLOBUS_NULL)
    {
	return globus_error_put(globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Out of memory at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname));
    }
    *attr = i_attr;

    i_attr->using_default_auth		= GLOBUS_TRUE;
    i_attr->parallelism.mode		= GLOBUS_FTP_CONTROL_PARALLELISM_NONE;
    i_attr->parallelism.fixed.size	= 1;
    i_attr->layout.mode			= GLOBUS_FTP_CONTROL_STRIPING_NONE;
    i_attr->buffer.mode			= GLOBUS_FTP_CONTROL_TCPBUFFER_DEFAULT;
    i_attr->type			= GLOBUS_FTP_CONTROL_TYPE_IMAGE;
    i_attr->mode			= GLOBUS_FTP_CONTROL_MODE_STREAM;
    i_attr->append			= GLOBUS_FALSE;
    i_attr->dcau.mode			= GLOBUS_FTP_CONTROL_DCAU_DEFAULT;
    i_attr->data_prot			= GLOBUS_FTP_CONTROL_PROTECTION_CLEAR;
    i_attr->read_all			= GLOBUS_FALSE;
    i_attr->read_all_intermediate_callback= GLOBUS_NULL;
    i_attr->read_all_intermediate_callback_arg
					= GLOBUS_NULL;
    i_attr->resume_third_party		= GLOBUS_FALSE;
    i_attr->force_striped		= GLOBUS_FALSE;

    tmp_name = globus_libc_strdup("anonymous");
    if(tmp_name == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Could not allocate internal data structure at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);
	
	goto error_exit;
    }
    tmp_pass = globus_libc_strdup("globus@");
    if(tmp_pass == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Could not allocate internal data structure at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);
	
	goto free_name;
    }

    result = globus_ftp_control_auth_info_init(&i_attr->auth_info,
					       GSS_C_NO_CREDENTIAL,
					       GLOBUS_TRUE,
					       tmp_name,
					       tmp_pass,
					       0,
					       0);
    
    if(result != GLOBUS_SUCCESS)
    {
	err = globus_error_get(result);

	goto free_pass;
    }
    return GLOBUS_SUCCESS;
free_pass:
    globus_libc_free(tmp_pass);
free_name:
    globus_libc_free(tmp_name);
error_exit:
    globus_libc_free(i_attr);
    *attr = GLOBUS_NULL;

    return globus_error_put(err);
}
/* globus_ftp_client_operationattr_init() */

/**
 * Destroy an FTP client attribute set.
 * @ingroup globus_ftp_client_operationattr
 *
 * @param attr
 *        A pointer to the attribute to destroy.
 */
globus_result_t
globus_ftp_client_operationattr_destroy(
    globus_ftp_client_operationattr_t *		attr)
{
    globus_object_t *				err;
    globus_i_ftp_client_operationattr_t *	i_attr;
    static char * myname = "globus_ftp_client_operationattr_destroy";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot destoy NULL attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }
    if(*attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot destoy invalid attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }
    i_attr = *(globus_i_ftp_client_operationattr_t **) attr;

    if(i_attr->auth_info.user)
    {
	globus_libc_free(i_attr->auth_info.user);
	i_attr->auth_info.user = GLOBUS_NULL;
    }
    if(i_attr->auth_info.password)
    {
	globus_libc_free(i_attr->auth_info.password);
	i_attr->auth_info.password = GLOBUS_NULL;
    }
    if(i_attr->auth_info.auth_gssapi_subject)
    {
	globus_libc_free(i_attr->auth_info.auth_gssapi_subject);
	i_attr->auth_info.auth_gssapi_subject = GLOBUS_NULL;
    }
    if(i_attr->dcau.mode == GLOBUS_FTP_CONTROL_DCAU_SUBJECT)
    {
	globus_libc_free(i_attr->dcau.subject.subject);
	i_attr->dcau.subject.subject = GLOBUS_NULL;
	i_attr->dcau.mode = GLOBUS_FTP_CONTROL_DCAU_NONE;
    }
    globus_libc_free(i_attr);

    *attr = GLOBUS_NULL;

    return GLOBUS_SUCCESS;
error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_operationattr_destroy() */

/**
 * @name Parallelism
 */
/* @{ */
/**
 * Set/Get the parallelism attribute for an ftp client attribute set.
 * @ingroup globus_ftp_client_operationattr
 *
 * This attribute allows the user to control the level of parallelism
 * to be used on an extended block mode file transfer. 
 * Currently, only a "fixed" parallelism level is supported. This
 * is interpreted by the FTP server as the number of parallel data
 * connections to be allowed for each stripe of data. Currently, only
 * the "fixed" parallelism type is 
 *
 * This attribute is ignored in stream mode.
 *
 * @param attr
 *        The attribute set to query or modify.
 * @param parallelism
 *        The value of parallelism attribute.
 *        
 * @see #globus_gsiftp_control_parallelism_t,
 *      globus_ftp_client_operationattr_set_layout(), 
 *      globus_ftp_client_operationattr_set_mode()
 *
 * @note This is a Grid-FTP extension, and may not be supported on all FTP
 * servers.
 */
globus_result_t
globus_ftp_client_operationattr_set_parallelism(
    globus_ftp_client_operationattr_t *		attr,
    const globus_ftp_control_parallelism_t *	parallelism)
{
    globus_object_t *				err;
    globus_i_ftp_client_operationattr_t *	i_attr;
    static char * myname = "globus_ftp_client_operationattr_set_parallelism";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot set values on a NULL attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }
    if(parallelism == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] NULL parallelism parameter at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }
    i_attr = *(globus_i_ftp_client_operationattr_t **) attr;

    if(parallelism->mode == GLOBUS_FTP_CONTROL_PARALLELISM_FIXED ||
       parallelism->mode == GLOBUS_FTP_CONTROL_PARALLELISM_NONE)
    {
	memcpy(&i_attr->parallelism,
	       parallelism,
	       sizeof(globus_ftp_control_parallelism_t));

	return GLOBUS_SUCCESS;
    }
    else
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Unsupported parallelism mode %d %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    (int) parallelism->mode,
	    myname);

	goto error_exit;
    }

error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_operationattr_set_parallelism() */

globus_result_t
globus_ftp_client_operationattr_get_parallelism(
    const globus_ftp_client_operationattr_t *	attr,
    globus_ftp_control_parallelism_t *		parallelism)
{
    globus_object_t *				err;
    const globus_i_ftp_client_operationattr_t *	i_attr;
    static char * myname = "globus_ftp_client_operationattr_get_parallelism";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot get values from a NULL attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }
    if(parallelism == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] NULL parallelism parameter at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }

    i_attr = *(const globus_i_ftp_client_operationattr_t **) attr;

    memcpy(parallelism,
	   &i_attr->parallelism,
	   sizeof(globus_ftp_control_parallelism_t));

    return GLOBUS_SUCCESS;

error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_operationattr_get_parallelism() */
/* @} */

/* @{ */
/**
 * @name Striped Data Movement
 */

/**
 * Set/Get the striped attribute for an ftp client attribute set.
 * @ingroup globus_ftp_client_operationattr
 *
 * This attribute allows the user to force the client library to used
 * the FTP commands to do a striped data transfer, even when the user
 * has not requested a specific file layout via the layout attribute.
 * This is useful when transferring files between servers which use
 * the server side processing commands ERET or ESTO to transform data
 * and send it to particular stripes on the destination server.
 *
 * The layout attribute is used only when the data is
 * being stored the server (on a put or 3rd party transfer). This
 * attribute is ignored for stream mode data transfers.
 *
 * @param attr
 *        The attribute set to query or modify.
 * @param striped
 *        The value of striped attribute. 
 *
 * @see globus_ftp_client_operationattr_set_parallelism(),
 *      globus_ftp_client_operationattr_set_layout()
 *      globus_ftp_client_operationattr_set_mode()
 *
 * @note This is a Grid-FTP extension, and may not be supported on all FTP
 *       servers.
 */
globus_result_t
globus_ftp_client_operationattr_set_striped(
    globus_ftp_client_operationattr_t *		attr,
    globus_bool_t 				striped)
{
    globus_object_t *				err;
    globus_i_ftp_client_operationattr_t *	i_attr;
    static char * myname = "globus_ftp_client_operationattr_set_striped";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot set values on a NULL attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }
    i_attr = *attr;

    i_attr->force_striped = striped;
    
    return GLOBUS_SUCCESS;

error_exit:
    return globus_error_put(err);

}
/* globus_ftp_client_operationattr_set_striped() */

globus_result_t
globus_ftp_client_operationattr_get_striped(
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t *				striped)
{
    globus_object_t *				err;
    const globus_i_ftp_client_operationattr_t * i_attr; 
    static char * myname = "globus_ftp_client_operationattr_get_striped";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot get values from a NULL attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }
    if(striped == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] NULL striped parameter at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }
    i_attr = *attr;

    (*striped) = i_attr->force_striped;
    
    return GLOBUS_SUCCESS;

error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_operationattr_get_striped() */

/* @} */

/**
 * @name Striped File Layout
 */
/* @{ */
/**
 * Set/Get the layout attribute for an ftp client attribute set.
 * @ingroup globus_ftp_client_operationattr
 *
 * This attribute allows the user to control the layout of a file
 * being transfered to a striped Grid-FTP server. The striping layout
 * defines what regions of a file will be stored on each stripe of a
 * multiple-striped ftp server.
 *
 * The layout attribute is used only when the data is
 * being stored on the server (on a put or 3rd party transfer). This
 * attribute is ignored for stream mode data transfers.
 *
 * @param attr
 *        The attribute set to query or modify.
 * @param layout
 *        The value of layout attribute. 
 *
 * @see #globus_ftp_control_layout_t,
 *      globus_ftp_client_operationattr_set_parallelism(),
 *      globus_ftp_client_operationattr_set_mode()
 *
 * @note This is a Grid-FTP extension, and may not be supported on all FTP
 *       servers.
 */
globus_result_t
globus_ftp_client_operationattr_set_layout(
    globus_ftp_client_operationattr_t *		attr,
    const globus_ftp_control_layout_t *		layout)
{
    globus_object_t *				err;
    globus_i_ftp_client_operationattr_t *	i_attr;
    static char * myname = "globus_ftp_client_operationattr_set_layout";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Cannot set values on a NULL attribute at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);
	goto error_exit;
    }
    if(layout == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] NULL layout parameter at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);
	goto error_exit;
    }

    if(layout->mode == GLOBUS_FTP_CONTROL_STRIPING_BLOCKED_ROUND_ROBIN &&
	    layout->round_robin.block_size == 0)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot set round robin layout with 0 byte block size at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }
    i_attr = *(globus_i_ftp_client_operationattr_t **) attr;
    memcpy(&i_attr->layout,
	   layout,
	   sizeof(globus_ftp_control_layout_t));
    
    return GLOBUS_SUCCESS;

error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_operationattr_set_layout() */

globus_result_t
globus_ftp_client_operationattr_get_layout(
    const globus_ftp_client_operationattr_t *	attr,
    globus_ftp_control_layout_t *		layout)
{
    globus_object_t *				err;
    const globus_i_ftp_client_operationattr_t *	i_attr;
    static char * myname = "globus_ftp_client_operationattr_get_layout";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot get values from a NULL attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }
    if(layout == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] NULL layout at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }

    i_attr = *(const globus_i_ftp_client_operationattr_t **) attr;

    memcpy(layout,
	   &i_attr->layout,
	   sizeof(globus_ftp_control_layout_t));
    return GLOBUS_SUCCESS;

error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_operationattr_get_layout() */
/* @} */

/**
 * @name TCP Buffer
 */
/* @{ */
/**
 * Set/Get the TCP buffer attribute for an ftp client attribute set.
 * @ingroup globus_ftp_client_operationattr
 *
 * This attribute allows the user to control the TCP buffer size used
 * for all data channels used in a file transfer. The size of the TCP
 * buffer can make a significant impact on the performance of a file
 * transfer. The user may set the buffer to either a system-dependent
 * default value, or to a fixed value.
 *
 * The actual implementation of this attribute is designed to be as widely
 * interoperable as possible. In addition to supporting the SBUF command
 * described in the GridFTP protocol extensions document, it also supports
 * other commands and site commands which are used by other servers to
 * set TCP buffer sizes. These are 
 * - SITE RETRBUFSIZE
 * - SITE RBUFSZ
 * - SITE RBUFSIZ
 * - SITE STORBUFIZE
 * - SITE SBUFSZ
 * - SITE SBUFSIZ
 * - SITE BUFSIZE
 *
 * This attribute is affects any type of data transfer done with the
 * ftp client library.
 *
 * @param attr
 *        The attribute set to query or modify.
 * @param buffer
 *        The value of tcp_buffer attribute.
 *
 * @see #globus_gsiftp_control_tcpbuffer_t
 */
globus_result_t
globus_ftp_client_operationattr_set_tcp_buffer(
    globus_ftp_client_operationattr_t *		attr,
    const globus_ftp_control_tcpbuffer_t *	tcp_buffer)
{
    globus_object_t *				err;
    globus_i_ftp_client_operationattr_t *	i_attr;
    static char * myname = "globus_ftp_client_operationattr_set_tcp_buffer";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Cannot set values on a NULL attribute at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);
	goto error_exit;
    }
    if(tcp_buffer == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] NULL tcp_buffer parameter at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);
	goto error_exit;
    }
    i_attr = *(globus_i_ftp_client_operationattr_t **) attr;

    memcpy(&i_attr->buffer,
	   tcp_buffer,
	   sizeof(globus_ftp_control_tcpbuffer_t));

    return GLOBUS_SUCCESS;

error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_operationattr_set_tcp_buffer() */

globus_result_t
globus_ftp_client_operationattr_get_tcp_buffer(
    const globus_ftp_client_operationattr_t *	attr,
    globus_ftp_control_tcpbuffer_t *		tcp_buffer)
{
    globus_object_t *				err;
    const globus_i_ftp_client_operationattr_t *	i_attr;
    static char * myname = "globus_ftp_client_operationattr_get_tcp_buffer";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot get values from a NULL attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }
    if(tcp_buffer == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] NULL tcp_buffer parameter at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }

    i_attr = *(const globus_i_ftp_client_operationattr_t **) attr;

    memcpy(tcp_buffer,
	   &i_attr->buffer,
	   sizeof(globus_ftp_control_tcpbuffer_t));

    return GLOBUS_SUCCESS;
error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_operationattr_get_tcp_buffer() */
/* @} */

/**
 * @name File Type
 */
/* @{ */
/**
 * Set/Get the file representation type attribute for an ftp client
 * attribute set.
 * @ingroup globus_ftp_client_operationattr
 *
 * This attribute allows the user to choose the file type used for an
 * FTP file transfer. The file may be transferred as either ASCII or
 * a binary image.
 *
 * When transferring an ASCII file, the data will be transformed
 * in the following way
 * - the high-order bit will be set to zero
 * - end-of line characters will be converted to a CRLF pair for the 
 *   data transfer, and then converted to native format before being
 *   returned to the user's data callbacks.
 *
 * The default type for the ftp client libary is binary.
 *
 * @param attr
 *        The attribute set to query or modify.
 * @param buffer
 *        The value of type attribute.
 *
 * @see #globus_ftp_control_type_t
 */
globus_result_t
globus_ftp_client_operationattr_set_type(
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_control_type_t			type)
{
    globus_object_t *				err;
    globus_i_ftp_client_operationattr_t *	i_attr;
    static char * myname = "globus_ftp_client_operationattr_set_type";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot set values on a NULL attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }
    if(type == GLOBUS_FTP_CONTROL_TYPE_NONE ||
       type == GLOBUS_FTP_CONTROL_TYPE_EBCDIC ||
       type == GLOBUS_FTP_CONTROL_TYPE_LOCAL)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Invalid type parameter %d at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    type,
	    myname);	    
    }
    i_attr = *(globus_i_ftp_client_operationattr_t **) attr;
    i_attr->type = type;

    return GLOBUS_SUCCESS;

error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_operationattr_set_type() */

globus_result_t
globus_ftp_client_operationattr_get_type(
    const globus_ftp_client_operationattr_t *	attr,
    globus_ftp_control_type_t *			type)
{
    globus_object_t *				err;
    const globus_i_ftp_client_operationattr_t *	i_attr;
    static char * myname = "globus_ftp_client_operationattr_get_type";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot get values from a NULL attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }
    if(type == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] NULL type parameter at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }
    i_attr = *(const globus_i_ftp_client_operationattr_t **) attr;

    *type = i_attr->type;

    return GLOBUS_SUCCESS;
error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_operationattr_get_type() */
/* @} */

/**
 * @name Transfer Mode
 */
/* @{ */
/**
 * Set/Get the file transfer mode attribute for an ftp client
 * attribute set.
 * @ingroup globus_ftp_client_operationattr
 *
 * This attribute allows the user to choose the data channel protocol
 * used to transfer a file. There are two modes supported by this
 * library: stream mode and extended block mode.
 *
 * Stream mode is a file transfer mode where all data is sent over a
 * single TCP socket, without any data framing. In stream mode, data
 * will arrive in sequential order. This mode is supported
 * by nearly all FTP servers.
 *
 * Extended block mode is a file transfer mode where data can be sent
 * over multiple parallel connections and to multiple data storage
 * nodes to provide a high-performance data transfer. In extended
 * block mode, data may arrive out-of-order. ASCII type files are not
 * supported in extended block mode.
 *
 * @param attr
 *        The attribute set to query or modify.
 * @param layout
 *        The value of mode attribute
 *
 * @see #globus_ftp_control_mode_t,
 *      globus_ftp_client_operationattr_set_parallelism(),
 *      globus_ftp_client_operationattr_set_layout()
 *
 * @note Extended block mode is a Grid-FTP extension, and may not be
 *       supported on all FTP servers.
 */
globus_result_t
globus_ftp_client_operationattr_set_mode(
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_control_mode_t			mode)
{
    globus_object_t *				err;
    globus_i_ftp_client_operationattr_t *	i_attr;
    static char * myname = "globus_ftp_client_operationattr_set_mode";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Cannot set values on a NULL attribute at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);
	goto error_exit;
    }
    if(mode == GLOBUS_FTP_CONTROL_MODE_NONE ||
       mode == GLOBUS_FTP_CONTROL_MODE_BLOCK ||
       mode == GLOBUS_FTP_CONTROL_MODE_COMPRESSED)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Invalid mode parameter %d at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		mode,
		myname);
	goto error_exit;
    }

    i_attr = *(globus_i_ftp_client_operationattr_t **) attr;

    if(i_attr->append == GLOBUS_TRUE &&
       mode == GLOBUS_FTP_CONTROL_MODE_EXTENDED_BLOCK)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Cannot set extended block mode on append at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);

	goto error_exit;
    }
    if(i_attr->type == GLOBUS_FTP_CONTROL_TYPE_ASCII && 
       mode == GLOBUS_FTP_CONTROL_MODE_EXTENDED_BLOCK)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Cannot set extended block mode for an ASCII file at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);

	goto error_exit;
    }

    i_attr->mode = mode;

    return GLOBUS_SUCCESS;
error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_operationattr_set_mode() */

globus_result_t
globus_ftp_client_operationattr_get_mode(
    const globus_ftp_client_operationattr_t *	attr,
    globus_ftp_control_mode_t *			mode)
{
    globus_object_t *				err;
    const globus_i_ftp_client_operationattr_t * i_attr;
    static char * myname = "globus_ftp_client_operationattr_get_mode";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot get values from a NULL attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }
    if(mode == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] NULL mode parameter at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }
    i_attr = *(globus_i_ftp_client_operationattr_t **) attr;

    *mode = i_attr->mode;

    return GLOBUS_SUCCESS;
error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_operationattr_get_mode() */
/* @} */

/**
 * @name Resume 3rd Party Transfers
 */
/*@{*/

/**
 * Set/Get the resume 3rd party transfer attribute for an ftp client
 * attribute set.
 *
 * This attribute allows the user to automatically resume a previously
 * started stream mode third party transfer, without knowing how much
 * data has already been transferred.
 *
 * If this attribute is set to GLOBUS_FALSE (the default), all third
 * party transfers will be complete file transfers, unless a restart
 * marker is passed to the globus_ftp_client_third_party_transfer()
 * function. 
 *
 * If this attribute is set to GLOBUS_TRUE, a stream restart marker is
 * computed based on the size of the file on the destination server.
 *
 * This attribute influences the behavior of the library only when
 * a third-party transfer is done in stream mode.
 */
globus_result_t
globus_ftp_client_operationattr_set_resume_third_party_transfer(
    globus_ftp_client_operationattr_t *         attr,
    globus_bool_t				resume)
{
    globus_object_t *				err;
    globus_i_ftp_client_operationattr_t *	i_attr;
    static char * myname = "globus_ftp_client_operationattr_set_resume_thid_party_transfer";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot set values on a NULL attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }
    i_attr = *attr;

    i_attr->resume_third_party = resume;
    
    return GLOBUS_SUCCESS;

error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_operationattr_set_resume_third_party_transfer() */

globus_result_t
globus_ftp_client_operationattr_get_resume_third_party_transfer(
    const globus_ftp_client_operationattr_t *   attr,
    globus_bool_t *				resume)
{
    globus_object_t *				err;
    const globus_i_ftp_client_operationattr_t * i_attr; 
    static char * myname = "globus_ftp_client_operationattr_get_resume_third_party_transfer";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot get values from a NULL attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }
    if(resume == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] NULL resume parameter at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }
    i_attr = *attr;

    (*resume) = i_attr->resume_third_party;
    
    return GLOBUS_SUCCESS;

error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_operationattr_get_resume_third_party_transfer() */
/*@}*/

/**
 * @name Authorization
 */
/* @{ */
/**
 * Set/Get the authorization attribute for an ftp client attribute set.
 * @ingroup globus_ftp_client_operationattr
 *
 * This attribute allows the user to pass authentication information to
 * the ftp client library. This information is used to authenticate
 * with the ftp server. 
 *
 * The Globus FTP client library supports authentication using either
 * the GSSAPI, or standard plaintext username and passwords. The type
 * of authentication is determined by the URL scheme which is used for
 * the individual get, put, or 3rd party transfer calls.
 *
 * @param attr
 *        The attribute set to query or modify.
 * @param credential
 *        The credential to use for authenticating with a GSIFTP server.
 *        This may be GSS_C_NO_CREDENTIAL to use the default credential.
 * @param user
 *        The user name to send to the FTP server. When doing a gsiftp
 *	  transfer, this may be set to NULL, and the default globusmap
 *        entry for the user's GSI identity will be usd.
 * @param password
 *        The password to send to the FTP server. When doing a gsiftp
 *	  transfer, this may be set to NULL.
 * @param account
 *        The account to use for the data transfer. Most FTP servers
 *        do not require this.
 * @param subject
 *        The subject name of the FTP server. This is only used when
 *        doing a gsiftp transfer, and then only when the security
 *        subject name does not match the hostname of the server (ie,
 *        when the server is being run by a user).
 */
globus_result_t
globus_ftp_client_operationattr_set_authorization(
    globus_ftp_client_operationattr_t *		attr,
    gss_cred_id_t				credential,
    const char *				user,
    const char *				password,
    const char *				account,
    const char *				subject)
{
    globus_object_t *				err;
    char *					tmp_user;
    char *					tmp_pass;
    char *					tmp_acct;
    char *					tmp_gss_sub;
    globus_i_ftp_client_operationattr_t * 	i_attr;

    static char * myname = "globus_ftp_client_operationattr_set_authorization";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot set values on a NULL attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }

    i_attr = *attr;

    tmp_user = i_attr->auth_info.user;
    tmp_pass = i_attr->auth_info.password;
    tmp_acct = i_attr->auth_info.account;
    tmp_gss_sub = i_attr->auth_info.auth_gssapi_subject;

    if(i_attr->auth_info.user)
    {
	i_attr->auth_info.user = GLOBUS_NULL;
    }
    if(i_attr->auth_info.password)
    {
	i_attr->auth_info.password = GLOBUS_NULL;
    }
    if(i_attr->auth_info.auth_gssapi_subject)
    {
	i_attr->auth_info.auth_gssapi_subject = GLOBUS_NULL;
    }

    if(user)
    {
	i_attr->auth_info.user = globus_libc_strdup(user);
	if(!i_attr->auth_info.user)
	{
	    goto reset_user;
	}
    }
    if(password)
    {
	i_attr->auth_info.password = globus_libc_strdup(password);
	if(!i_attr->auth_info.password)
	{
	    goto reset_pass;
	}
    }
    if(account)
    {
	i_attr->auth_info.account = globus_libc_strdup(account);
	if(!i_attr->auth_info.account)
	{
	    goto reset_acct;
	}
    }
    if(subject)
    {
	i_attr->auth_info.auth_gssapi_subject = globus_libc_strdup(subject);

	if(!i_attr->auth_info.auth_gssapi_subject)
	{
	    goto reset_subject;
	}
    }

    i_attr->using_default_auth = GLOBUS_FALSE;
    i_attr->auth_info.credential_handle = credential;

    if(tmp_user)
    {
	globus_libc_free(tmp_user);
    }
    if(tmp_pass)
    {
	globus_libc_free(tmp_pass);
    }
    if(tmp_acct)
    {
	globus_libc_free(tmp_acct);
    }
    if(tmp_gss_sub)
    {
	globus_libc_free(tmp_gss_sub);
    }
    return GLOBUS_SUCCESS;

reset_subject:
    i_attr->auth_info.auth_gssapi_subject = tmp_gss_sub;
    if(i_attr->auth_info.account)
    {
	globus_libc_free(i_attr->auth_info.account);
    }
reset_acct:
    i_attr->auth_info.account = tmp_acct;
    if(i_attr->auth_info.password)
    {
	globus_libc_free(i_attr->auth_info.password);
    }
reset_pass:
    i_attr->auth_info.password = tmp_pass;
    if(i_attr->auth_info.user)
    {
	globus_libc_free(i_attr->auth_info.user);
    }
reset_user:
    i_attr->auth_info.user = tmp_user;

    err = globus_error_construct_string(
	GLOBUS_FTP_CLIENT_MODULE,
	GLOBUS_NULL,
	"[%s] Unable to allocate internal data at %s\n",
	GLOBUS_FTP_CLIENT_MODULE->module_name,
	myname);
error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_operationattr_set_authorization() */

globus_result_t
globus_ftp_client_operationattr_get_authorization(
    const globus_ftp_client_operationattr_t *	attr,
    gss_cred_id_t *				credential,
    char **					user,
    char **					password,
    char **					account,
    char **					subject)
{
    globus_object_t *				err = GLOBUS_SUCCESS;
    char *					tmp_user = GLOBUS_NULL;
    char *					tmp_pass = GLOBUS_NULL;
    char *					tmp_acct = GLOBUS_NULL;
    char *					tmp_gss_sub  = GLOBUS_NULL;
    const globus_i_ftp_client_operationattr_t *	i_attr;

    static char * myname = "globus_ftp_client_operationattr_get_authorization";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot get values from a NULL attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }
    i_attr = *attr;
    if(i_attr->auth_info.user)
    {
	tmp_user = globus_libc_strdup(i_attr->auth_info.user);
	if(!tmp_user)
	{
	    goto memory_error_exit;
	}
    }
    if(i_attr->auth_info.password)
    {
	tmp_pass = globus_libc_strdup(i_attr->auth_info.password);
	if(!tmp_pass)
	{
	    goto free_user;
	}
    }
    if(i_attr->auth_info.account)
    {
	tmp_acct = globus_libc_strdup(i_attr->auth_info.account);
	if(!tmp_acct)
	{
	    goto free_pass;
	}

    }
    if(i_attr->auth_info.auth_gssapi_subject)
    {
	tmp_gss_sub =
	    globus_libc_strdup(i_attr->auth_info.auth_gssapi_subject);
	if(!tmp_gss_sub)
	{
	    goto free_acct;
	}
    }

    (*user) = tmp_user;
    (*password) = tmp_pass;
    (*account) = tmp_acct;
    (*subject) = tmp_gss_sub;
    (*credential) = i_attr->auth_info.credential_handle;

    return GLOBUS_SUCCESS;

free_acct:
    globus_libc_free(tmp_acct);
free_pass:
    globus_libc_free(tmp_pass);
free_user:
    globus_libc_free(tmp_user);
memory_error_exit:
    err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		err,
		"[%s] Unable to allocate internal data at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);
error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_operationattr_get_authorization() */
/* @} */

/**
 * @name Data Channel Authentication
 */
/* @{ */
/**
 * Set/Get the data channel authentication attribute for an ftp client
 * attribute set.
 * @ingroup globus_ftp_client_operationattr
 *
 * Data channel authentication is a GridFTP extension, and may not be
 * supported by all servers. If a server supports it, then the default
 * is to delegate a credential to the server, and authenticate all
 * data channels with that delegated credential.
 *
 * @param attr
 *        The attribute set to query or modify.
 * @param dcau
 *        The value of data channel authentication attribute.
 */
globus_result_t
globus_ftp_client_operationattr_set_dcau(
    globus_ftp_client_operationattr_t *		attr,
    const globus_ftp_control_dcau_t *		dcau)
{
    globus_i_ftp_client_operationattr_t *	i_attr;
    globus_object_t *				err;

    static char * myname = "globus_ftp_client_operationattr_set_dcau";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Cannot set values on a NULL attribute at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);
	
	goto error_exit;
    }
    if(dcau == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] NULL dcau parameter at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);
	goto error_exit;
    }
    if(dcau->mode == GLOBUS_FTP_CONTROL_DCAU_SUBJECT &&
       dcau->subject.subject == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Invalid dcau subject at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);
	goto error_exit;
    }
    i_attr = *attr;
    if(dcau->mode == GLOBUS_FTP_CONTROL_DCAU_SUBJECT)
    {
	char * tmp_subject;
	tmp_subject = globus_libc_strdup(dcau->subject.subject);
	if(! tmp_subject)
	{
	    err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Unable to allocate internal data at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	    goto error_exit;
	}
	i_attr->dcau.subject.subject = tmp_subject;
    }
    i_attr->dcau.mode = dcau->mode;
    
    return GLOBUS_SUCCESS;

error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_operationattr_set_dcau() */

globus_result_t
globus_ftp_client_operationattr_get_dcau(
    const globus_ftp_client_operationattr_t *	attr,
    globus_ftp_control_dcau_t *			dcau)
{
    globus_object_t *				err;
    const globus_i_ftp_client_operationattr_t *	i_attr;
    static char * myname = "globus_ftp_client_operationattr_get_dcau";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot get values from a NULL attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }
    if(dcau == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] NULL dcau parameter at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);
	goto error_exit;
    }
    i_attr = *attr;
    if(i_attr->dcau.mode == GLOBUS_FTP_CONTROL_DCAU_SUBJECT)
    {
	dcau->subject.subject = globus_libc_strdup(i_attr->dcau.subject.subject);
	if(dcau->subject.subject == GLOBUS_NULL)
	{
	    err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] NULL dcau parameter at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	    goto error_exit;
	}
    }
    dcau->mode = i_attr->dcau.mode;

    return GLOBUS_SUCCESS;

error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_operationattr_get_dcau() */
/* @} */

/**
 * @name Data Channel Protection
 */
/* @{ */
/**
 * Set/Get the data channel protection attribute for an ftp client
 * attribute set.
 * @ingroup globus_ftp_client_operationattr
 *
 * @param attr
 *        The attribute set to query or modify.
 * @param protection
 *        The value of data channel protection attribute. 
 *
 * @bug Only safe and private protection levels are supported by gsiftp.
 */
globus_result_t
globus_ftp_client_operationattr_set_data_protection(
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_control_protection_t 		protection)
{
    globus_object_t *				err;
    globus_i_ftp_client_operationattr_t *	i_attr;
    static char * myname = "globus_ftp_client_operationattr_set_data_protection";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Cannot set values on a NULL attribute at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);
	
	goto error_exit;
    }
    i_attr = *attr;

    i_attr->data_prot = protection;
    
    return GLOBUS_SUCCESS;

error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_operationattr_set_data_protection() */

globus_result_t
globus_ftp_client_operationattr_get_data_protection(
    const globus_ftp_client_operationattr_t *	attr,
    globus_ftp_control_protection_t * 		protection)
{
    globus_object_t *				err;
    const globus_i_ftp_client_operationattr_t *	i_attr;
    static char * myname = "globus_ftp_client_operationattr_get_data_protection";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Cannot set values on a NULL attribute at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);
	
	goto error_exit;
    }
    if(protection == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] NULL protection parameter at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);
	
	goto error_exit;
    }
    i_attr = *attr;
    *protection = i_attr->data_prot;
    
    return GLOBUS_SUCCESS;

error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_operationattr_get_data_protection() */

/* @} */

/**
 * @name Control Channel Protection
 */
/* @{ */
/**
 * Set/Get the control channel protection attribute for an ftp client
 * attribute set.
 * @ingroup globus_ftp_client_operationattr
 *
 * The control channel protection attribute allows the user to decide whether
 * to encrypt or integrity check the command session between the client
 * and the FTP server. This attribute is only relevant if used with a gsiftp
 * URL.
 *
 * @param attr
 *        The attribute set to query or modify.
 * @param protection
 *        The value of control channel protection attribute. 
 *
 * @bug The clear and safe protection levels are treated identically, with
 *      the client integrity checking all commands. The
 *      confidential and private protection levels are treated identically,
 *      with the client encrypting all commands.
 */
globus_result_t
globus_ftp_client_operationattr_set_control_protection(
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_control_protection_t 		protection)
{
    globus_object_t *				err;
    globus_i_ftp_client_operationattr_t *	i_attr;
    static char * myname = "globus_ftp_client_operationattr_set_control_protection";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Cannot set values on a NULL attribute at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);
	
	goto error_exit;
    }
    i_attr = *attr;

    switch(protection)
    {
	case GLOBUS_FTP_CONTROL_PROTECTION_CLEAR:
	case GLOBUS_FTP_CONTROL_PROTECTION_SAFE:
	    i_attr->auth_info.encrypt = GLOBUS_FALSE;
	    break;
	case GLOBUS_FTP_CONTROL_PROTECTION_CONFIDENTIAL:
	case GLOBUS_FTP_CONTROL_PROTECTION_PRIVATE:
	    i_attr->auth_info.encrypt = GLOBUS_TRUE;
	    break;
    }
    
    return GLOBUS_SUCCESS;

error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_operationattr_set_control_protection() */

globus_result_t
globus_ftp_client_operationattr_get_control_protection(
    const globus_ftp_client_operationattr_t *	attr,
    globus_ftp_control_protection_t * 		protection)
{
    globus_object_t *				err;
    const globus_i_ftp_client_operationattr_t *	i_attr;
    static char * myname = "globus_ftp_client_operationattr_get_control_protection";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Cannot set values on a NULL attribute at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);
	
	goto error_exit;
    }
    if(protection == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] NULL protection parameter at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);
	
	goto error_exit;
    }
    i_attr = *attr;

    if(i_attr->auth_info.encrypt)
    {
	*protection = GLOBUS_FTP_CONTROL_PROTECTION_SAFE;
    }
    else
    {
	*protection = GLOBUS_FTP_CONTROL_PROTECTION_PRIVATE;
    }
    
    return GLOBUS_SUCCESS;

error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_operationattr_get_control_protection() */

/* @} */
/**
 * @name Append
 */
/* @{ */
/**
 * Set/Get the append attribute for an ftp client attribute set.
 * @ingroup globus_ftp_client_operationattr
 *
 * This attribute allows the user to append to a file on an FTP
 * server, instead of replacing the existing file when doing a
 * globus_ftp_client_put() or globus_ftp_client_transfer().
 *
 * This attribute is ignored on the retrieving side
 * of a transfer, or a globus_ftp_client_get().
 *
 * @param attr
 *        The attribute set to query or modify.
 * @param append
 *        The value of append attribute. 
 */
globus_result_t
globus_ftp_client_operationattr_set_append(
    globus_ftp_client_operationattr_t *		attr,
    globus_bool_t				append)
{
    globus_object_t *				err;
    globus_i_ftp_client_operationattr_t *	i_attr;
    static char * myname = "globus_ftp_client_operationattr_set_append";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Cannot set values on a NULL attribute at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);
	
	goto error_exit;
    }

    i_attr = *attr;

    if(append && i_attr->mode == GLOBUS_FTP_CONTROL_MODE_EXTENDED_BLOCK)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Cannot set append mode in extended block mode\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);
	
	goto error_exit;	
    }
    i_attr->append = append;
    
    return GLOBUS_SUCCESS;

error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_operationattr_set_append() */

globus_result_t
globus_ftp_client_operationattr_get_append(
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t *				append)
{
    globus_object_t *				err;
    const globus_i_ftp_client_operationattr_t *	i_attr;
    static char * myname = "globus_ftp_client_operationattr_get_append";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot get values from a NULL attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }
    if(append == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] NULL append parameter at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }

    i_attr = *attr;
    (*append) = i_attr->append;

    return GLOBUS_SUCCESS;

error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_operationattr_get_append() */
/* @} */

/**
 * @name Read into a Single Buffer
 */
/* @{ */
/**
 * Set/Get the read_all attribute for an ftp client attribute set.
 * @ingroup globus_ftp_client_operationattr
 *
 * This attribute allows the user to pass in a single buffer to
 * receive all of the data for the current transfer. This buffer must
 * be large enough to hold all of the data for the transfer. Only one
 * buffer may be registered with globus_ftp_client_register_read()
 * when this attribute is used for a get.
 *
 * In extended block mode, this attribute will cause data to be
 * stored directly into the buffer from multiple streams without any
 * extra data copies.
 *
 * If the user sets the intermediate callback to a non-null value,
 * this function will  be called whenever an intermediate sub-section
 * of the data is received into the buffer.
 *
 * This attribute is ignored for globus_ftp_client_put() or
 * globus_ftp_client_third_party_transfer() operations.
 *
 * @param attr
 *        The attribute set to query or modify.
 * @param read_all
 *        The value of read_all attribute. 
 * @param intermediate_callback
 *        Callback to be invoked when a subsection of the data has
 *        been retreived. This callback may be GLOBUS_NULL, if the
 *        user only wants to be notified when the data transfer is
 *        completed. 
 * @param intermediate_callback_arg
 *        User data to be passed to the intermediate callback function.
 */
globus_result_t
globus_ftp_client_operationattr_set_read_all(
    globus_ftp_client_operationattr_t *		attr,
    globus_bool_t				read_all,
    globus_ftp_client_data_callback_t		intermediate_callback,
    void *					intermediate_callback_arg)
{
    globus_object_t *				err;
    globus_i_ftp_client_operationattr_t *	i_attr;
    static char * myname = "globus_ftp_client_operationattr_set_read_all";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
	    GLOBUS_FTP_CLIENT_MODULE,
	    GLOBUS_NULL,
	    "[%s] Cannot set values on a NULL attribute at %s\n",
	    GLOBUS_FTP_CLIENT_MODULE->module_name,
	    myname);
	
	goto error_exit;
    }
    i_attr = *attr;

    i_attr->read_all = read_all;
    i_attr->read_all_intermediate_callback = intermediate_callback;
    i_attr->read_all_intermediate_callback_arg = intermediate_callback_arg; 
   
    return GLOBUS_SUCCESS;

error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_operationattr_set_read_all() */

globus_result_t
globus_ftp_client_operationattr_get_read_all(
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t *				read_all,
    globus_ftp_client_data_callback_t *		intermediate_callback,
    void **					intermediate_callback_arg)
{
    globus_object_t *				err;
    const globus_i_ftp_client_operationattr_t *	i_attr;
    static char * myname = "globus_ftp_client_operationattr_get_read_all";

    if(attr == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot get values from a NULL attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }
    if(read_all == GLOBUS_NULL)
    {
	err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] NULL read_all parameter at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);

	goto error_exit;
    }
    i_attr = *attr;
    if(i_attr->read_all_intermediate_callback)
    {
	if(intermediate_callback == GLOBUS_NULL)
	{
	    err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] NULL intermediate_callback parameter at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);
	    
	    goto error_exit;
	}
	else if(intermediate_callback_arg == GLOBUS_NULL)
	{
	    err = globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] NULL intermediate_callback_arg parameter at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname);
	    
	    goto error_exit;
	}
	else
	{
	    *intermediate_callback = i_attr->read_all_intermediate_callback;
	    *intermediate_callback_arg =
		i_attr->read_all_intermediate_callback_arg; 
	}
    }
    (*read_all) = i_attr->read_all;

    return GLOBUS_SUCCESS;

error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_operationattr_get_read_all() */
/* @} */

/**
 * Create a duplicate of an attribute set.
 * @ingroup globus_ftp_client_operationattr
 *
 * The duplicated attribute set has a deep copy of all data in the
 * attribute set, so the original may be destroyed, while the copy is
 * still valid.
 *
 * @param dst
 *        The attribute set to be initialized to the same values as
 *        src. 
 * @param src
 *        The original attribute set to duplicate.
 */
globus_result_t
globus_ftp_client_operationattr_copy(
    globus_ftp_client_operationattr_t *		dst,
    const globus_ftp_client_operationattr_t *	src)
{
    globus_result_t				result;
    const globus_i_ftp_client_operationattr_t *	i_src;

    result = globus_ftp_client_operationattr_init(dst);

    i_src = *(const globus_i_ftp_client_operationattr_t **) src;

    if(result)
    {
	goto error_exit;
    }

    result =
	globus_ftp_client_operationattr_set_parallelism(dst,
		                                        &i_src->parallelism);
    if(result)
    {
	goto destroy_exit;
    }

    result =
	globus_ftp_client_operationattr_set_layout(dst,
					           &i_src->layout);
    if(result)
    {
	goto destroy_exit;
    }

    result = 
	globus_ftp_client_operationattr_set_striped(dst,
						    i_src->force_striped);
    if(result)
    {
	goto destroy_exit;
    }

    result =
	globus_ftp_client_operationattr_set_tcp_buffer(dst,
						       &i_src->buffer);
    if(result)
    {
	goto destroy_exit;
    }

    result =
	globus_ftp_client_operationattr_set_mode(dst,
						 i_src->mode);
    if(result)
    {
	goto destroy_exit;
    }

    result = globus_ftp_client_operationattr_set_type(dst,
						      i_src->type);
    if(result)
    {
	goto destroy_exit;
    }

    result = globus_ftp_client_operationattr_set_dcau(dst,
					              &i_src->dcau);
    if(result)
    {
	goto destroy_exit;
    }

    result = globus_ftp_client_operationattr_set_data_protection(
	    dst,
	    i_src->data_prot);

    if(result)
    {
	goto destroy_exit;
    }

    result = globus_ftp_client_operationattr_set_control_protection(
	    dst,
	    i_src->auth_info.encrypt ? GLOBUS_FTP_CONTROL_PROTECTION_PRIVATE
	                             : GLOBUS_FTP_CONTROL_PROTECTION_SAFE);
    if(result)
    {
	goto destroy_exit;
    }

    result =
	globus_ftp_client_operationattr_set_append(dst,
						   i_src->append);
    if(result)
    {
	goto destroy_exit;
    }

    result =
	globus_ftp_client_operationattr_set_resume_third_party_transfer(
	    dst,
	    i_src->resume_third_party);
    
    if(result)
    {
	goto destroy_exit;
    }

    result = 
	globus_ftp_client_operationattr_set_read_all(
		dst,
		i_src->read_all,
		i_src->read_all_intermediate_callback,
		i_src->read_all_intermediate_callback_arg);
    if(result)
    {
	goto destroy_exit;
    }

    if(!i_src->using_default_auth)
    {
	result = 
	    globus_ftp_client_operationattr_set_authorization(
		    dst,
		    i_src->auth_info.credential_handle,
		    i_src->auth_info.user,
		    i_src->auth_info.password,
		    i_src->auth_info.account,
		    i_src->auth_info.auth_gssapi_subject);
	if(result)
	{
	    goto destroy_exit;
	}
    }
    return GLOBUS_SUCCESS;

destroy_exit:
    globus_ftp_client_operationattr_destroy(dst);

error_exit:
    return result;
}
/* globus_ftp_client_operationattr_copy() */

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
globus_result_t
globus_i_ftp_client_handleattr_copy(
    globus_i_ftp_client_handleattr_t *		dest,
    globus_i_ftp_client_handleattr_t *		src)
{
    globus_list_t *				tmp;
    globus_i_ftp_client_cache_entry_t *		tmpurl;
    globus_i_ftp_client_cache_entry_t *		newurl;
    globus_list_t *				reverser;
    globus_i_ftp_client_plugin_t *		plugin;
    globus_ftp_client_plugin_t *		new_plugin;
    static char * myname = "globus_i_ftp_client_handleattr_copy";
    
    if(dest == GLOBUS_NULL || src == GLOBUS_NULL)
    {
	return globus_error_put(globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] Cannot copy NULL attribute at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname));
    }
    
    dest->cache_all = src->cache_all;
    dest->nl_handle = src->nl_handle;
    dest->url_cache = GLOBUS_NULL;
    dest->plugins = GLOBUS_NULL;
    
    tmp = src->url_cache;

    while(!globus_list_empty(tmp))
    {
	tmpurl = (globus_i_ftp_client_cache_entry_t *) globus_list_first(tmp);
	newurl = globus_libc_calloc(1,
                                    sizeof(globus_i_ftp_client_cache_entry_t));
	if(!newurl)
	{
	    goto error_exit;
	}
	if(! globus_url_copy(&newurl->url,
			     &tmpurl->url))
	{
	    globus_libc_free(newurl);
	    goto error_exit;
	}
	globus_list_insert(&dest->url_cache, newurl);
	
	tmp = globus_list_rest(tmp);
    }
    tmp = src->plugins;
    reverser = GLOBUS_NULL;
    while(!globus_list_empty(tmp))
    {
	plugin = globus_list_first(tmp);
	tmp = globus_list_rest(tmp);

	if(plugin->copy_func)
	{
	    new_plugin = plugin->copy_func(plugin->plugin,
		                           plugin->plugin_specific);
	    if(new_plugin)
	    {
		(*new_plugin)->plugin = new_plugin;
		globus_list_insert(&reverser, *new_plugin);
	    }
	    else
	    {
		goto free_reverser_exit;
	    }
	}
    }
    tmp = reverser;

    while(!globus_list_empty(tmp))
    {
	globus_list_insert(&dest->plugins,
			   globus_list_first(tmp));
	tmp = globus_list_rest(tmp);
    }
    return GLOBUS_SUCCESS;

 free_reverser_exit:
    tmp = reverser;
    
    while(!globus_list_empty(tmp))
    {
	plugin = globus_list_first(tmp);
	tmp = globus_list_rest(tmp);

	plugin->destroy_func(plugin->plugin,
		             plugin->plugin_specific);
    }

 error_exit:

    while(!globus_list_empty(dest->url_cache))
    {
	tmpurl = (globus_i_ftp_client_cache_entry_t *)
	    globus_list_remove(&dest->url_cache,
                               dest->url_cache);
	globus_url_destroy(&tmpurl->url);
	globus_libc_free(tmpurl);
    }
    return globus_error_put(globus_error_construct_string(
	GLOBUS_FTP_CLIENT_MODULE,
	GLOBUS_NULL,
	"[%s] Unable to allocate internal data at %s\n",
	GLOBUS_FTP_CLIENT_MODULE->module_name,
	myname));
}
/* globus_i_ftp_client_handleattr_copy() */

static
int
globus_l_ftp_client_plugin_list_search(void * datum,
				       void * arg)
{
    globus_i_ftp_client_plugin_t *	plugin;
    char *				name;

    plugin = (globus_i_ftp_client_plugin_t *) datum;
    name = (char *) arg;
    
    return !strcmp(plugin->plugin_name, name);
}
#endif
