/**********************************************************************

display_status.c

Description:
    GSSAPI routine to display the error messages

CVS Information:

    $Source$
    $Date$
    $Revision$
    $Author$

**********************************************************************/

static char *rcsid = "$Header$";

/**********************************************************************
                             Include header files
**********************************************************************/


#include "gssapi_ssleay.h"
#include "gssutils.h"
#include <string.h>

/**********************************************************************
                               Type definitions
**********************************************************************/

/**********************************************************************
                          Module specific prototypes
**********************************************************************/

/**********************************************************************
                       Define module specific variables
**********************************************************************/

/**********************************************************************
Function:   gss_display_status

Description:
    Calls the SSLeay error print routines to produce a printable
	message. This may need some work, as the SSLeay error messages 
	are more of a trace, and my not be the best for the user. 
	Also don't take advantage of being called in a loop. 

Parameters:

Returns:
**********************************************************************/

OM_uint32 
GSS_CALLCONV gss_display_status
(OM_uint32 *          minor_status,
 OM_uint32            status_value,
 int                  status_type,
 const gss_OID        mech_type,
 OM_uint32 *          message_context,
 gss_buffer_t   	  status_string)
{
	char * reason;
	unsigned long err;
	const char * fs;
	const char * rs;
#if SSLEAY_VERSION_NUMBER  >= 0x00904100L
    const char *file;
#else
    char *file;
#endif
	char * data;
#ifdef DEBUG
        char format[] = "%s %s\n  Function:%s        Source:%s:%d";
#else
        char format[] = "%s %s\n  Function:%s";
#endif
	int line;
	char fbuf[1024];
	char rbuf[1024];

	status_string->length = 0;
	status_string->value = NULL;
	*message_context = 0;
	*minor_status = 0;
 
	if (status_type == GSS_C_GSS_CODE) {
		if (status_value == GSS_S_COMPLETE) {
			reason = "GSS COMPLETE";
		}
		else switch (GSS_ERROR(status_value)) {
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
		status_string->value = strdup(reason);
		status_string->length = strlen(status_string->value);
		return GSS_S_COMPLETE;
	}
		/* WIN32 does not have the ERR_get_error_line_data */ 
		/* exported, so simulate it till it is fixed */
		/* in SSLeay-0.9.0 so simulate it */
	else if (status_type == GSS_C_MECH_CODE) {
        /* Returns last error code from error queue without modifying it */ 
		if ((ERR_peek_error()) != 0)
		{
			int i;
            int flags;

			ERR_STATE *es;
            es = ERR_get_state();
			i=(es->bottom+1)%ERR_NUM_ERRORS;

			if (es->err_data[i] == NULL) {
				data = "";
			} else {
				data = es->err_data[i];
			}
            flags =  es->err_data_flags[i];

            /* removes error from error queue along with file and line info */
            err = ERR_get_error_line(&file,&line);
			fs=ERR_func_error_string(err);
			if (fs == NULL) {
				sprintf(fbuf,"func(%u)",ERR_GET_FUNC(err));
				fs = fbuf;
			}

			rs=ERR_reason_error_string(err);
			if (rs == NULL) {
				sprintf(rbuf,"reason(%u)",ERR_GET_REASON(err));
				rs = rbuf;
			}

			status_string->length = 64 + strlen(format) + 
				strlen(fs) + strlen(rs) + strlen(data) + 
				strlen(file);
			status_string->value = 
					(char *)malloc(status_string->length);
			if (status_string->value == NULL) {
				return GSS_S_FAILURE;
			}
			
#ifdef DEBUG
			sprintf(status_string->value, format, rs, data, fs,
								 file, line);
            
		    if (ERR_peek_error())	
                (*message_context) = 1;
            else (*message_context) = 0;
#else
			sprintf(status_string->value, format, rs, data, fs);
            
            /* Check to make sure there is another error in the queue
             * to display
             *
             * If this error is from the user space we already caught it and
             * presumably produced a good error message, so we stop
             * displaying further errors.  If we did not catch it 
             * it will be from the SSL libraries and we'll let it 
             * print all of its error information until one of our errors
             * is printed.
             */
            if (ERR_peek_error() && 
                    (ERR_GET_LIB(err) <  ERR_LIB_USER || 
                     flags & ERR_DISPLAY_CONTINUE_NEEDED))
            {
                (*message_context) = 1;
            }
            else 
            {
                (*message_context) = 0;
            }
#endif
		} else {
			status_string->value = strdup("");
			*message_context = 0;
		}

		if (status_string->value == NULL) {
			return GSS_S_FAILURE;
		}
		
		status_string->length = strlen(status_string->value);
		return GSS_S_COMPLETE;
		
	} else {
		return GSS_S_BAD_STATUS;
	}
}
