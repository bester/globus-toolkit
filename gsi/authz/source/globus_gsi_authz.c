/**
 * @file globus_gsi_authz.c
 * Globus Authorization API
 *
 *
 */
#include "globus_common.h"
#include "version.h"
#include "globus_i_gsi_authz.h"
#include "globus_callout.h"
#include "globus_gsi_system_config.h"
#include "globus_gsi_authz_callout_error.h"

static int globus_l_gsi_authz_activate(void);
static int globus_l_gsi_authz_deactivate(void);

int      globus_i_gsi_authz_debug_level   = 0;
FILE *   globus_i_gsi_authz_debug_fstream = NULL;

/**
 * Module descriptor static initializer.
 */
globus_module_descriptor_t globus_i_gsi_authz_module =
{
    "globus_gsi_authz",
    globus_l_gsi_authz_activate,
    globus_l_gsi_authz_deactivate,
    GLOBUS_NULL,
    GLOBUS_NULL,
    &local_version
};

/*
  Sam:
  These variables are used to keep state across requests.
*/
static globus_callout_handle_t        callout_handle;
static void *                         authz_system_state;
static char *			      effective_identity = 0;

/**
 * Module activation
 */
static int globus_l_gsi_authz_activate(void)
{
    /* activate any module used by the implementation */
    /* initialize a globus callout handle */
    /* call authz system init callout */
    /* the callout type is "GLOBUS_GSI_AUTHZ_SYSTEM_INIT" */
    /* arguments are: void ** authz_system_state, ie &authz_system_state */
    /* should define some standard errors for this callout */

    int		                        result = (int) GLOBUS_SUCCESS;
    char *                              filename = 0;
    char *                              tmp_string;
    static char *                       _function_name_ =
        "globus_l_gsi_authz_activate";

    tmp_string = globus_module_getenv("GLOBUS_GSI_AUTHZ_DEBUG_LEVEL");
    if(tmp_string != GLOBUS_NULL)
    {
        globus_i_gsi_authz_debug_level = atoi(tmp_string);
        
        if(globus_i_gsi_authz_debug_level < 0)
        {
            globus_i_gsi_authz_debug_level = 0;
        }
    }

    tmp_string = globus_module_getenv("GLOBUS_GSI_AUTHZ_DEBUG_FILE");
    if(tmp_string != GLOBUS_NULL)
    {
        globus_i_gsi_authz_debug_fstream = fopen(tmp_string, "a");
        if(globus_i_gsi_authz_debug_fstream == NULL)
        {
            result = (int)GLOBUS_FAILURE;
            goto exit;
        }
    }
    else
    {
      /* if the env. var. isn't set, use stderr */
        globus_i_gsi_authz_debug_fstream = stderr;
    }

    GLOBUS_I_GSI_AUTHZ_DEBUG_ENTER;

    result = globus_module_activate(GLOBUS_COMMON_MODULE);
    if(result != (int)GLOBUS_SUCCESS)
    {
        goto exit;
    }

    result = globus_module_activate(GLOBUS_CALLOUT_MODULE);
    if(result != (int)GLOBUS_SUCCESS)
    {
        goto exit;
    }

    result = globus_module_activate(GLOBUS_GSI_AUTHZ_CALLOUT_ERROR_MODULE);
    if(result != (int)GLOBUS_SUCCESS)
    {
        goto exit;
    }

    result = (int)GLOBUS_GSI_SYSCONFIG_GET_AUTHZ_LIB_CONF_FILENAME(&filename);

    /* initialize a globus callout handle */
    result = (int)globus_callout_handle_init(&callout_handle);
    if(result != (int)GLOBUS_SUCCESS)
    {
        goto exit;
    }

    result = (int)globus_callout_read_config(callout_handle, filename);
    if(result != (int)GLOBUS_SUCCESS)
    {
        goto exit;
    }

    free(filename);

    /* call authz system init callout */
    /* the callout type is "GLOBUS_GSI_AUTHZ_SYSTEM_INIT" */
    /* arguments are: void ** authz_system_state, ie &authz_system_state */
    result = (int)globus_callout_call_type(callout_handle,
					   "GLOBUS_GSI_AUTHZ_SYSTEM_INIT",
					   &authz_system_state);
    if(result != (int)GLOBUS_SUCCESS)
    {
        goto exit;
    }

    
 exit:

    GLOBUS_I_GSI_AUTHZ_DEBUG_EXIT;

    return(result);
  
}

/**
 * Module deactivation
 ***/
static int globus_l_gsi_authz_deactivate(void)
{
    /* deactivate any module used by the implementation */
    /* destroy globus callout handle */
    /* call authz system destroy callout */
    /* the callout type is "GLOBUS_GSI_AUTHZ_SYSTEM_DESTROY" */
    /* arguments are: void ** authz_system_state, ie &authz_system_state */
    /* should define some standard errors for this callout */
    
    int                                 result = (int) GLOBUS_SUCCESS;
    static char *                       _function_name_ =
	"globus_l_gsi_authz_deactivate";
    
    GLOBUS_I_GSI_AUTHZ_DEBUG_ENTER;
    
    /* deactivate any module used by the implementation */
    
    result = globus_module_deactivate(GLOBUS_CALLOUT_MODULE);
    if(result != GLOBUS_SUCCESS)
    {
	goto exit;
    }
    
    result = globus_module_deactivate(GLOBUS_COMMON_MODULE);
    if(result != GLOBUS_SUCCESS)
    {
	goto exit;
    }

    result = globus_module_deactivate(GLOBUS_GSI_AUTHZ_CALLOUT_ERROR_MODULE);
    if(result != (int)GLOBUS_SUCCESS)
    {
        goto exit;
    }


    /* destroy globus callout handle here */
    /* call authz system destroy callout */
    /* the callout type is "GLOBUS_GSI_AUTHZ_SYSTEM_DESTROY" */
    /* arguments are: void ** authz_system_state, ie &authz_system_state */
    result = (int) globus_callout_call_type(callout_handle,
					    "GLOBUS_GSI_AUTHZ_SYSTEM_DESTROY",
					    &authz_system_state);
    if(result != GLOBUS_SUCCESS)
    {
	goto exit;
    }
    
    result = (int)globus_callout_handle_destroy(callout_handle);
    if(result != GLOBUS_SUCCESS)
    {
	goto exit;
    }
    
    GLOBUS_I_GSI_AUTHZ_DEBUG_EXIT;
    
    if(globus_i_gsi_authz_debug_fstream != stderr)
    {
	fclose(globus_i_gsi_authz_debug_fstream);
    }
    
 exit:

    return result;
}


/**
 * Initialize Handle
 * @ingroup globus_gsi_authz_handle
 */
/* @{ */
/**
 * Initializes a handle.
 *
 * @param handle
 *        Pointer to the handle that is to be initialized
 * @return
 *        GLOBUS_SUCCESS if successful
 *        A Globus error object on failure:
 */
globus_result_t
globus_gsi_authz_handle_init(
    globus_gsi_authz_handle_t *         handle,
    const char *                        service_name,
    const gss_ctx_id_t                  context,
    globus_gsi_authz_cb_t               callback,
    void *                              callback_arg)
{
    globus_result_t                     result = GLOBUS_SUCCESS;
    static char *                       _function_name_ =
	"globus_gsi_authz_handle_init";
    
    GLOBUS_I_GSI_AUTHZ_DEBUG_ENTER;

    if (handle == 0)
    {
	result = GLOBUS_GSI_AUTHZ_ERROR_NULL_VALUE("handle");
	goto exit;
    }

    if (service_name == 0)
    {
	result = GLOBUS_GSI_AUTHZ_ERROR_NULL_VALUE("service_name");
	goto exit;
    }
    
    /* call authz system per connection init callout */
    /* the callout type is "GLOBUS_GSI_AUTHZ_HANDLE_INIT" */
    /* arguments are: globus_gsi_authz_handle_t * handle,
       const char * service_name,
       const gss_ctx_id_t context,
       globus_gsi_authz_cb_t callback,
       void * callback_arg,
       void * authz_system_state */
    result = globus_callout_call_type(callout_handle,
				      "GLOBUS_GSI_AUTHZ_HANDLE_INIT",
				      handle,
				      service_name,
				      context,
				      callback,
				      callback_arg,
				      authz_system_state);
    if(result != GLOBUS_SUCCESS)
    {
	goto exit;
    }
    
    GLOBUS_I_GSI_AUTHZ_DEBUG_EXIT;

 exit:
    
    return result;
}
/* globus_gsi_authz_handle_init */
/* @} */


/**
 * Authorization decision made here
 * @ingroup globus_gsi_authorize
 */
/**
 * Authorization decision made here
 *
 * @param handle
 *        Pointer to the handle that is to be initialized
 *         action
 *         object,
 *         callback,
 *         callback_arg
 *
 * @return
 *        GLOBUS_SUCCESS if successful
 *        A Globus error object on failure:
 */
globus_result_t
globus_gsi_authorize(
  globus_gsi_authz_handle_t           handle,
  const void *                        action,
  const void *                        object,
  globus_gsi_authz_cb_t               callback,
  void *                              callback_arg)
{
    globus_result_t                  result = GLOBUS_SUCCESS;
    static char *                   _function_name_ =
	"globus_gsi_authorize";
    
    if(callback == GLOBUS_NULL)
    {
	result = GLOBUS_GSI_AUTHZ_ERROR_NULL_VALUE("callback parameter");
	goto exit;
    }
    
    GLOBUS_I_GSI_AUTHZ_DEBUG_ENTER;
    
    /* call main authorization callout */
    /* the callout type is "GLOBUS_GSI_AUTHORIZE_ASYNC" */
    /* arguments are: globus_gsi_authz_handle_t handle,
       const void * action,
       const void * object,                      
       globus_gsi_authz_cb_t callback,
       void * callback_arg,
       void * authz_system_state */
    result = globus_callout_call_type(callout_handle,
				      "GLOBUS_GSI_AUTHORIZE_ASYNC",
				      handle,
				      action,
				      object,
				      callback,
				      callback_arg,
				      authz_system_state);
    
 exit:
    GLOBUS_I_GSI_AUTHZ_DEBUG_EXIT;  
    return result;
}


globus_result_t
globus_gsi_cancel_authz(
    globus_gsi_authz_handle_t           handle)
{
    /* call cancel callout */
    /* the callout type is "GLOBUS_GSI_AUTHZ_CANCEL" */
    /* arguments are: globus_gsi_authz_handle_t * handle,
       void * authz_system_state */
    /* should define some standard errors for this callout */    
    globus_result_t                     result = GLOBUS_SUCCESS;
    static char *                       _function_name_ =
	"globus_gsi_cancel_authz";
    
    
    GLOBUS_I_GSI_AUTHZ_DEBUG_ENTER; 

    result = globus_callout_call_type(callout_handle,
				      "GLOBUS_GSI_AUTHZ_CANCEL",
				      handle,
				      &authz_system_state);
    if(result != GLOBUS_SUCCESS)
    {
	goto exit;
    }
    
 exit:
    
    GLOBUS_I_GSI_AUTHZ_DEBUG_EXIT; 
    return result;
}


/**
 * @name Destroy Handle
 */
/*@{*/
/**
 * Destroy a Globus GSI authz  Handle
 * @ingroup globus_gsi_authz_handle
 *
 * @param handle
 *        The handle that is to be destroyed
 * @return
 *        GLOBUS_SUCCESS
 */
globus_result_t
globus_gsi_authz_handle_destroy(
    globus_gsi_authz_handle_t           handle,
    globus_gsi_authz_cb_t               callback,
    void *                              callback_arg)
{
    /* call authz system callout the frees per connection state */
    /* the callout type is "GLOBUS_GSI_AUTHZ_HANDLE_DESTROY" */
    /* arguments are: globus_gsi_authz_handle_t * handle,
                      globus_gsi_authz_cb_t callback,
                      void * callback_arg,
                      void * authz_system_state */
    /* should define some standard errors for this callout */    

    globus_result_t                     result = GLOBUS_SUCCESS;
    static char *                       _function_name_ =
	"globus_gsi_authz_handle_destroy";

    GLOBUS_I_GSI_AUTHZ_DEBUG_ENTER;

    result = globus_callout_call_type(callout_handle,
				      "GLOBUS_GSI_AUTHZ_HANDLE_DESTROY",
				      handle,
				      callback,
				      callback_arg,
				      &authz_system_state);
    if(result != GLOBUS_SUCCESS)
    {
	goto exit;
    }
    
   
 exit:
    
    GLOBUS_I_GSI_AUTHZ_DEBUG_EXIT;    
    return result;
}
/*globus_gsi_authz_handle_destroy*/
/*@}*/

/**
 * @name Query for authorization identity
 */
/*@{*/
/**
 *  @param (identity_ptr)
 *        output: the authorization identity.  This is malloc'd and should
 *        be freed by the caller.  If the value is 0 (and this function returned
 *	  GLOBUS_SUCCESS), the caller should use the authenticated identity.
 *
 *  @return 
 *        GLOBUS_SUCCESS
 */
globus_result_t
globus_gsi_authz_get_authorization_identity(
    globus_gsi_authz_handle_t           handle,
    char **				identity_ptr,
    globus_gsi_authz_cb_t               callback,
    void *                              callback_arg)
{

    /* call authz system callout to get the authorization identity */
    /* the callout type is "GLOBUS_GSI_AUTHZ_GET_AUTHORIZATION_IDENTITY" */
    /* arguments are: globus_gsi_authz_handle_t * handle,
       		      char **identity_ptr,
                      globus_gsi_authz_cb_t callback,
                      void * callback_arg,
                      void * authz_system_state */
    /* should define some standard errors for this callout */    

    globus_result_t                     result = GLOBUS_SUCCESS;
    static char *                       _function_name_ =
	"globus_gsi_authz_get_authorization_identity";

    GLOBUS_I_GSI_AUTHZ_DEBUG_ENTER;

    if(callback == GLOBUS_NULL)
    {
	result = GLOBUS_GSI_AUTHZ_ERROR_NULL_VALUE("callback parameter");
	goto exit;
    }
    
    if(callback == GLOBUS_NULL)
    {
	result = GLOBUS_GSI_AUTHZ_ERROR_NULL_VALUE("identity_ptr parameter");
	goto exit;
    }
    
    result = globus_callout_call_type(callout_handle,
				      "GLOBUS_GSI_GET_AUTHORIZATION_IDENTITY",
				      handle,
				      identity_ptr,
				      callback,
				      callback_arg,
				      &authz_system_state);
 exit:
    
    GLOBUS_I_GSI_AUTHZ_DEBUG_EXIT;    
    return result;
}

/*globus_gsi_authz_handle_destroy*/
/*@}*/
