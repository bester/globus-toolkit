#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file display_name.c
 * @author Sam Lang, Sam Meder
 *
 * $RCSfile$
 * $Revision$
 * $Date$
 */
#endif

static char *rcsid = "$Id$";

#include "gssapi_openssl.h"
#include "globus_i_gsi_gss_utils.h"
#include <string.h>

/**
 * Calls the SSLeay error print routines to produce a printable
 * message. This may need some work, as the SSLeay error messages 
 * are more of a trace, and my not be the best for the user. 
 * Also don't take advantage of being called in a loop. 
 *
 * @param minor_status
 * @param status_value
 * @param status_type
 * @param mech_type
 * @param message_context
 * @param status_string
 *
 * @return
 */
OM_uint32 
GSS_CALLCONV gss_display_status(
    OM_uint32 *                         minor_status,
    OM_uint32                           status_value,
    int                                 status_type,
    const gss_OID                       mech_type,
    OM_uint32 *                         message_context,
    gss_buffer_t   	                status_string)
{
    globus_object_t *                   error_obj = NULL;
    char *                              error_chain_string = NULL;
    OM_uint32                           major_status = GSS_S_COMPLETE;
    char *                              reason;
    static char *                       _function_name_ =
        "gss_display_status";
    GLOBUS_I_GSI_GSSAPI_DEBUG_ENTER;

    status_string->length = 0;
    status_string->value = NULL;

    *minor_status = (OM_uint32) GLOBUS_SUCCESS;
 
    if (status_type == GSS_C_GSS_CODE ||
        status_type == GSS_C_MECH_CODE) 
    {
        if (!GSS_ERROR(status_value)) 
        {
            reason = "GSS COMPLETE";
        }
        else switch (GSS_ERROR(status_value)) 
        {
        
        case GSS_S_FAILURE:
            reason = "General failure";
            break;
        
        case GSS_S_DEFECTIVE_TOKEN:
            reason = "Communications Error";
            break;
        
        case GSS_S_DEFECTIVE_CREDENTIAL:
            reason = "Authentication Failed";
            break;
        
        case GSS_S_CREDENTIALS_EXPIRED:
            reason = "Credentials Expired";
            break;
        
        case GSS_S_BAD_NAME:
            reason = "Service or hostname could "
                "not be understood";
            break;
        
        case GSS_S_UNAUTHORIZED:
            reason = "Unexpected Gatekeeper or Service Name";
            break;
        
        case GSS_S_NO_CRED:
            reason = "Problem with local credentials";			
            break;
        
        case GSS_S_BAD_SIG:
            reason = "Invalid signature on message";
            break;
        
        default:
            reason = "Some Other GSS failure";
            break;
        } 

        error_obj = globus_error_get((globus_result_t) *message_context);
        error_chain_string = globus_error_print_chain(error_obj);

        status_string->value = globus_gsi_cert_utils_create_string(
            "GSS Major Status: %s\nGSS Minor Status Error Chain:\n%s",
            reason, 
            error_chain_string);

        globus_libc_free(error_chain_string);
        globus_object_free(error_obj);

        status_string->length = strlen(status_string->value);
        major_status = GSS_S_COMPLETE;
        goto exit;
    }
    else 
    {
        major_status = GSS_S_BAD_STATUS;
        goto exit;
    }

 exit:
    GLOBUS_I_GSI_GSSAPI_DEBUG_EXIT;
    return major_status;
}
/* @} */
