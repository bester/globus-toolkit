#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file globus_ftp_client_restart_plugin.c GridFTP Restart Plugin Implementation
 *
 * $RCSfile$
 * $Revision$
 * $Date$
 */
#endif

#include "globus_i_ftp_client.h"
#include "globus_ftp_client_restart_plugin.h"

#include <stdio.h>
#include <string.h>
#include "version.h"

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
#define GLOBUS_L_FTP_CLIENT_RESTART_PLUGIN_NAME "globus_ftp_client_restart_plugin"

#define GLOBUS_L_FTP_CLIENT_RESTART_PLUGIN_RETURN(plugin) \
    if(plugin == GLOBUS_NULL) \
    {\
	return globus_error_put(globus_error_construct_string(\
		GLOBUS_FTP_CLIENT_MODULE,\
		GLOBUS_NULL,\
		"[%s] NULL plugin at %s\n",\
		GLOBUS_FTP_CLIENT_MODULE->module_name,\
		myname));\
    }
#define GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(d, func) \
    result = globus_ftp_client_plugin_set_##func##_func(d, globus_l_ftp_client_restart_plugin_##func); \
    if(result != GLOBUS_SUCCESS) goto result_exit;

/**
 * Plugin specific data for the restart plugin
 */
typedef struct
{
    /** Maximum num of faults to handle. If -1, then we won't be
     * limiting ourselves.
     */
    int						max_retries;

    /**
     * If true, then we will do an exponential backoff of th
     * interval between retries.
     */
    globus_bool_t				backoff;
    /**
     * Delay time between fault detection and next restart.
     */
    globus_reltime_t				interval;
    
    /**
     * Deadline, after which no further restart attempts will
     * be tried. If zero, then we won't be limiting ourselves
     */
    globus_abstime_t				deadline;

    /**
     * Source used in our operation (if applicable).
     */
    char *					source_url;

    /**
     * Destination used in our operation (if applicable).
     */
    char *					dest_url;

    /**
     * Source attributes
     */
    globus_ftp_client_operationattr_t 		source_attr;

    /**
     * Destination attributes.
     */
    globus_ftp_client_operationattr_t 		dest_attr;

    /**
     * Operation we are processing.
     */
    globus_i_ftp_client_operation_t		operation;

    globus_bool_t                               abort_pending;
}
globus_l_ftp_client_restart_plugin_t;

static
globus_ftp_client_plugin_t *
globus_l_ftp_client_restart_plugin_copy(
    globus_ftp_client_plugin_t *		plugin_template,
    void *					plugin_specific);

static
void
globus_l_ftp_client_restart_plugin_destroy(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific);

static
void
globus_l_ftp_client_restart_plugin_delete(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_restart_plugin_mkdir(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_restart_plugin_rmdir(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_restart_plugin_list(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_restart_plugin_verbose_list(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_restart_plugin_move(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				source_url,
    const char *				dest_url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_restart_plugin_get(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_restart_plugin_put(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_restart_plugin_third_party_transfer(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				source_url,
    const globus_ftp_client_operationattr_t *	source_attr,
    const char *				dest_url,
    const globus_ftp_client_operationattr_t *	dest_attr,
    globus_bool_t 				restart);

static
void globus_l_ftp_client_restart_plugin_abort(
    globus_ftp_client_plugin_t *                plugin,
    void *                                      plugin_specific,
    globus_ftp_client_handle_t *                handle);

static
void
globus_l_ftp_client_restart_plugin_fault(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_object_t *				error);

static
void
globus_l_ftp_client_restart_plugin_genericify(
    globus_l_ftp_client_restart_plugin_t *	d);

static int globus_l_ftp_client_restart_plugin_activate(void);
static int globus_l_ftp_client_restart_plugin_deactivate(void);

/**
 * Module descriptor static initializer.
 */
globus_module_descriptor_t globus_i_ftp_client_restart_plugin_module =
{
    "globus_ftp_client_restart_plugin",
    globus_l_ftp_client_restart_plugin_activate,
    globus_l_ftp_client_restart_plugin_deactivate,
    GLOBUS_NULL,
    GLOBUS_NULL,
    &local_version
};


static
int
globus_l_ftp_client_restart_plugin_activate(void)
{
    return globus_module_activate(GLOBUS_FTP_CLIENT_MODULE);
}

static
int
globus_l_ftp_client_restart_plugin_deactivate(void)
{
    return globus_module_deactivate(GLOBUS_FTP_CLIENT_MODULE);
}


static
globus_ftp_client_plugin_t *
globus_l_ftp_client_restart_plugin_copy(
    globus_ftp_client_plugin_t *		plugin_template,
    void *					plugin_specific)
{
    globus_ftp_client_plugin_t *		newguy;
    globus_l_ftp_client_restart_plugin_t *	d;
    globus_l_ftp_client_restart_plugin_t *	newd;
    globus_result_t				result;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    newguy = globus_libc_malloc(sizeof(globus_ftp_client_plugin_t));
    if(newguy == GLOBUS_NULL)
    {
	goto error_exit;
    }
    result = globus_ftp_client_restart_plugin_init(newguy,
	    d->max_retries,
	    &d->interval,
	    &d->deadline);

    if(result != GLOBUS_SUCCESS)
    {
	goto free_exit;
    }
    result = globus_ftp_client_plugin_get_plugin_specific(newguy,
	                                                  (void **) &newd);
    if(result != GLOBUS_SUCCESS)
    {
	goto destroy_exit;
    }
    newd->backoff = d->backoff;

    return newguy;

destroy_exit:
    globus_ftp_client_restart_plugin_destroy(newguy);
free_exit:
    globus_libc_free(newguy);
error_exit:
    return GLOBUS_NULL;
}
/* globus_l_ftp_client_restart_plugin_copy() */

static
void
globus_l_ftp_client_restart_plugin_destroy(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific)
{
    globus_ftp_client_restart_plugin_destroy(plugin);
    globus_libc_free(plugin);
}
/* globus_l_ftp_client_restart_plugin_destroy() */

static
void
globus_l_ftp_client_restart_plugin_delete(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    globus_l_ftp_client_restart_plugin_genericify(d);
    d->operation = GLOBUS_FTP_CLIENT_DELETE;
    d->source_url = globus_libc_strdup(url);
    globus_ftp_client_operationattr_copy(&d->source_attr,attr);

}
/* globus_l_ftp_client_restart_plugin_delete() */

static
void
globus_l_ftp_client_restart_plugin_mkdir(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    globus_l_ftp_client_restart_plugin_genericify(d);
    d->operation = GLOBUS_FTP_CLIENT_MKDIR;
    d->source_url = globus_libc_strdup(url);
    globus_ftp_client_operationattr_copy(&d->source_attr,attr);

}
/* globus_l_ftp_client_restart_plugin_mkdir() */

static
void
globus_l_ftp_client_restart_plugin_rmdir(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    globus_l_ftp_client_restart_plugin_genericify(d);
    d->operation = GLOBUS_FTP_CLIENT_RMDIR;
    d->source_url = globus_libc_strdup(url);
    globus_ftp_client_operationattr_copy(&d->source_attr,attr);
}
/* globus_l_ftp_client_restart_plugin_rmdir() */

static
void
globus_l_ftp_client_restart_plugin_list(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    globus_l_ftp_client_restart_plugin_genericify(d);
    d->operation = GLOBUS_FTP_CLIENT_NLST;
    d->source_url = globus_libc_strdup(url);
    globus_ftp_client_operationattr_copy(&d->source_attr,attr);
}
/* globus_l_ftp_client_restart_plugin_list() */

static
void
globus_l_ftp_client_restart_plugin_verbose_list(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    globus_l_ftp_client_restart_plugin_genericify(d);
    d->operation = GLOBUS_FTP_CLIENT_LIST;
    d->source_url = globus_libc_strdup(url);
    globus_ftp_client_operationattr_copy(&d->source_attr,attr);
}
/* globus_l_ftp_client_restart_plugin_vlist() */

static
void
globus_l_ftp_client_restart_plugin_move(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				source_url,
    const char *				dest_url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    globus_l_ftp_client_restart_plugin_genericify(d);
    d->operation = GLOBUS_FTP_CLIENT_MOVE;
    d->source_url = globus_libc_strdup(source_url);
    globus_ftp_client_operationattr_copy(&d->source_attr, attr);
    d->dest_url = globus_libc_strdup(dest_url);
    globus_ftp_client_operationattr_copy(&d->dest_attr, attr);
}
/* globus_l_ftp_client_restart_plugin_move() */

static
void
globus_l_ftp_client_restart_plugin_get(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    globus_l_ftp_client_restart_plugin_genericify(d);
    d->operation = GLOBUS_FTP_CLIENT_GET;
    d->source_url = globus_libc_strdup(url);
    globus_ftp_client_operationattr_copy(&d->source_attr, attr);
}
/* globus_l_ftp_client_restart_plugin_get() */

static
void
globus_l_ftp_client_restart_plugin_put(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    globus_l_ftp_client_restart_plugin_genericify(d);
    d->operation = GLOBUS_FTP_CLIENT_PUT;
    d->dest_url = globus_libc_strdup(url);
    globus_ftp_client_operationattr_copy(&d->dest_attr, attr);
}
/* globus_l_ftp_client_restart_plugin_put() */

static
void
globus_l_ftp_client_restart_plugin_third_party_transfer(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				source_url,
    const globus_ftp_client_operationattr_t *	source_attr,
    const char *				dest_url,
    const globus_ftp_client_operationattr_t *	dest_attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    globus_l_ftp_client_restart_plugin_genericify(d);
    d->operation = GLOBUS_FTP_CLIENT_TRANSFER;
    d->source_url = globus_libc_strdup(source_url);
    globus_ftp_client_operationattr_copy(&d->source_attr, source_attr);
    d->dest_url = globus_libc_strdup(dest_url);
    globus_ftp_client_operationattr_copy(&d->dest_attr, dest_attr);
}
/* globus_l_ftp_client_restart_plugin_third_party_transfer() */

static
void globus_l_ftp_client_restart_plugin_abort(
    globus_ftp_client_plugin_t *                plugin,
    void *                                      plugin_specific,
    globus_ftp_client_handle_t *                handle)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    d->abort_pending = GLOBUS_TRUE;
}

static
void
globus_l_ftp_client_restart_plugin_fault(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_object_t *				error)
{
    globus_l_ftp_client_restart_plugin_t *	d;
    globus_abstime_t				when;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    if(d->abort_pending)
    {
        return;
    }

    if(d->max_retries == 0)
    {
	return;
    }
    else if(d->max_retries > 0)
    {
	d->max_retries--;
    }

    GlobusTimeAbstimeGetCurrent(when);
    if((d->deadline.tv_sec != 0 || d->deadline.tv_nsec != 0) &&
	globus_abstime_cmp(&when, &d->deadline) > 0)
    {
	return;
    }
    GlobusTimeAbstimeSet(when, d->interval.tv_sec, d->interval.tv_usec);

    switch(d->operation)
    {
	case GLOBUS_FTP_CLIENT_DELETE:
	    globus_ftp_client_plugin_restart_delete(
		    handle,
		    d->source_url,
		    &d->source_attr,
		    &when);
	    break;

	case GLOBUS_FTP_CLIENT_MKDIR:
	    globus_ftp_client_plugin_restart_mkdir(
		    handle,
		    d->source_url,
		    &d->source_attr,
		    &when);
	    break;
	case GLOBUS_FTP_CLIENT_RMDIR:
	    globus_ftp_client_plugin_restart_rmdir(
		    handle,
		    d->source_url,
		    &d->source_attr,
		    &when);
	    break;
	case GLOBUS_FTP_CLIENT_MOVE:
	    globus_ftp_client_plugin_restart_move(
		    handle,
		    d->source_url,
		    d->dest_url,
		    &d->source_attr,
		    &when);
	    break;
	case GLOBUS_FTP_CLIENT_LIST:
	    globus_ftp_client_plugin_restart_verbose_list(
		    handle,
		    d->source_url,
		    &d->source_attr,
		    &when);
	    break;
	case GLOBUS_FTP_CLIENT_NLST:
	    globus_ftp_client_plugin_restart_list(
		    handle,
		    d->source_url,
		    &d->source_attr,
		    &when);
	    break;
	case GLOBUS_FTP_CLIENT_GET:
	    globus_ftp_client_plugin_restart_get(
		    handle,
		    d->source_url,
		    &d->source_attr,
		    GLOBUS_NULL,
		    &when);
	    break;
	case GLOBUS_FTP_CLIENT_PUT:
	    globus_ftp_client_plugin_restart_put(
		    handle,
		    d->dest_url,
		    &d->dest_attr,
		    GLOBUS_NULL,
		    &when);
	    break;
	case GLOBUS_FTP_CLIENT_TRANSFER:
	    globus_ftp_client_plugin_restart_third_party_transfer(
		    handle,
		    d->source_url,
		    &d->source_attr,
		    d->dest_url,
		    &d->dest_attr,
		    GLOBUS_NULL,
		    &when);
	    break;
	case GLOBUS_FTP_CLIENT_MDTM:
	    globus_ftp_client_plugin_restart_modification_time(
		    handle,
		    d->source_url,
		    &d->source_attr,
		    &when);
	    break;
	case GLOBUS_FTP_CLIENT_SIZE:
	    globus_ftp_client_plugin_restart_size(
		    handle,
		    d->source_url,
		    &d->source_attr,
		    &when);
	    break;
    default: /* Only state left is FTP_CLIENT_IDLE */
	  globus_assert(0 && "Unexpected state");
    }

    if(d->backoff)
    {
	GlobusTimeReltimeMultiply(d->interval, 2);
    }
}
/* globus_l_ftp_client_restart_plugin_fault() */
#endif

/**
 * Initialize an instance of the GridFTP restart plugin
 * @ingroup globus_ftp_client_restart_plugin
 *
 * This function will initialize the plugin-specific instance data
 * for this plugin, and will make the plugin usable for ftp
 * client handle attribute and handle creation.
 *
 * @param plugin
 *        A pointer to an uninitialized plugin. The plugin will be
 *        configured as a restart plugin.
 * @param max_retries
 *        The maximum number of times to retry the operation before giving
 *        up on the transfer. If this value is less than or equal to 0,
 *        then the restart plugin will keep trying to restart the operation
 *        until it completes or the deadline is reached with an unsuccessful
 *        operation.
 * @param interval
 *        The interval to wait after a failures before retrying the transfer.
 *        If the interval is 0 seconds, then an exponential backoff will be
 *        used.
 * @param deadline
 *        An absolute timeout
 *
 * @return This function returns an error if
 * - plugin is null
 *
 * @see globus_ftp_client_restart_plugin_destroy(),
 *      globus_ftp_client_handleattr_add_plugin(),
 *      globus_ftp_client_handleattr_remove_plugin(),
 *      globus_ftp_client_handle_init()
 */
globus_result_t
globus_ftp_client_restart_plugin_init(
    globus_ftp_client_plugin_t *		plugin,
    int						max_retries,
    globus_reltime_t *				interval,
    globus_abstime_t *				deadline)
{
    globus_l_ftp_client_restart_plugin_t *	d;
    globus_object_t *				err;
    globus_result_t				result;
    static char * myname = "globus_ftp_client_restart_plugin_init";

    if(plugin == GLOBUS_NULL)
    {
	return globus_error_put(globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] NULL plugin at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		myname));
    }

    d =
	globus_libc_malloc(sizeof(globus_l_ftp_client_restart_plugin_t));

    if(! d)
    {
	return globus_error_put(globus_error_construct_string(
		                GLOBUS_FTP_CLIENT_MODULE,
				GLOBUS_NULL,
				"[%s] Out of memory at %s\n",
				 GLOBUS_FTP_CLIENT_MODULE->module_name,
				 myname));
    }

    result = globus_ftp_client_plugin_init(plugin,
				  GLOBUS_L_FTP_CLIENT_RESTART_PLUGIN_NAME,
				  GLOBUS_FTP_CLIENT_CMD_MASK_ALL,
				  d);
    if(result != GLOBUS_SUCCESS)
    {
	globus_libc_free(d);

	return result;
    }

    d->max_retries = max_retries > 0 ? max_retries : -1;

    if(interval)
    {
	GlobusTimeReltimeCopy(d->interval, *interval);
    }
    if((!interval) || (interval->tv_sec == 0 && interval->tv_usec == 0))
    {
	d->backoff = GLOBUS_TRUE;
	d->interval.tv_sec = 1;
	d->interval.tv_usec = 0;
    }

    if(deadline)
    {
	GlobusTimeAbstimeCopy(d->deadline, *deadline);
    }
    else
    {
	GlobusTimeAbstimeCopy(d->deadline, globus_i_abstime_zero);
    }

    d->dest_url = GLOBUS_NULL;
    d->source_url = GLOBUS_NULL;

    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, copy);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, destroy);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, delete);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, mkdir);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, rmdir);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, move);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, verbose_list);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, list);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, get);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, put);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, third_party_transfer);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, fault);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, abort);

    return GLOBUS_SUCCESS;

result_exit:
    err = globus_error_get(result);
    globus_ftp_client_plugin_destroy(plugin);
    return globus_error_put(result);
}
/* globus_ftp_client_restart_plugin_init() */

/**
 * Destroy an instance of the GridFTP restart plugin
 * @ingroup globus_ftp_client_restart_plugin
 *
 * This function will free all restart plugin-specific instance data
 * from this plugin, and will make the plugin unusable for further ftp
 * handle creation.
 *
 * Existing FTP client handles and handle attributes will not be affected by
 * destroying a plugin associated with them, as a local copy of the plugin
 * is made upon handle initialization.
 *
 * @param plugin
 *        A pointer to a GridFTP restart plugin, previously initialized by
 *        calling globus_ftp_client_restart_plugin_init()
 *
 * @return This function returns an error if
 * - plugin is null
 * - plugin is not a restart plugin
 *
 * @see globus_ftp_client_restart_plugin_init(),
 *      globus_ftp_client_handleattr_add_plugin(),
 *      globus_ftp_client_handleattr_remove_plugin(),
 *      globus_ftp_client_handle_init()
 */
globus_result_t
globus_ftp_client_restart_plugin_destroy(
    globus_ftp_client_plugin_t *		plugin)
{
    globus_l_ftp_client_restart_plugin_t * d;
    globus_result_t result;
    static char * myname = "globus_ftp_client_restart_plugin_destroy";

    GLOBUS_L_FTP_CLIENT_RESTART_PLUGIN_RETURN(plugin);

    result = globus_ftp_client_plugin_get_plugin_specific(plugin,
	                                                  (void **) &d);
    if(result != GLOBUS_SUCCESS)
    {
	return result;
    }
    globus_l_ftp_client_restart_plugin_genericify(d);

    return globus_ftp_client_plugin_destroy(plugin);
}
/* globus_ftp_client_restart_plugin_destroy() */

static
void
globus_l_ftp_client_restart_plugin_genericify(
    globus_l_ftp_client_restart_plugin_t *	d)
{
    if(d->source_url)
    {
	globus_libc_free(d->source_url);
	globus_ftp_client_operationattr_destroy(&d->source_attr);
    }
    if(d->dest_url)
    {
	globus_libc_free(d->dest_url);
	globus_ftp_client_operationattr_destroy(&d->dest_attr);
    }

    d->operation = GLOBUS_FTP_CLIENT_IDLE;
    d->abort_pending = GLOBUS_FALSE;
}
/* globus_l_ftp_client_restart_plugin_genericify() */
