 /**********************************************************************

init_sec_context.c:

Description:
    GSSAPI routine to initiate the sending of a security context
	See: <draft-ietf-cat-gssv2-cbind-04.txt>
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
#include "openssl/evp.h"

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
Function: gss_init_sec_context

Description:
	Called by the client in a loop, it will return a token
	to be sent to the accept_sec_context running in the server. 
Parameters:

Returns:
**********************************************************************/

OM_uint32 
GSS_CALLCONV gss_init_sec_context(
    OM_uint32 *                         minor_status,
    const gss_cred_id_t                 initiator_cred_handle,
    gss_ctx_id_t *                      context_handle_P,
    const gss_name_t                    target_name,
    const gss_OID                       mech_type,
    OM_uint32                           req_flags,
    OM_uint32                           time_req,
    const gss_channel_bindings_t        input_chan_bindings,
    const gss_buffer_t                  input_token,
    gss_OID *                           actual_mech_type,
    gss_buffer_t                        output_token,
    OM_uint32 *                         ret_flags,
    OM_uint32 *                         time_rec) 
{

    gss_ctx_id_desc *                   context = NULL;
    OM_uint32 		                major_status = 0;
    OM_uint32 		                inv_minor_status = 0;
    OM_uint32 		                inv_major_status = 0;
    X509_REQ *                          reqp = NULL;
    X509 *                              ncert = NULL;
    int  			        rc;
    char 			        cbuf[1];
    X509_EXTENSION *                    ex = NULL;
    STACK_OF(X509_EXTENSION) *          extensions = NULL;

#ifdef DEBUG
    fprintf(stderr, "init_sec_context:\n") ;
#endif /* DEBUG */

    *minor_status = 0;
    output_token->length = 0;

    context = *context_handle_P;

    if ((context == (gss_ctx_id_t) GSS_C_NO_CONTEXT))
    {
#if defined(DEBUG) || defined(DEBUGX)
        fprintf(stderr, 
                "\n**********\ninit_sec_context: uid=%d pid=%d\n**********\n",
                getuid(), getpid()) ;
#endif /* DEBUG */

        /* 
         * We are going to use the SSL error routines, get them
         * initilized early. They may be called more then once. 
         */

        ERR_load_gsserr_strings(0);  

#ifdef DEBUG
        fprintf(stderr, "Creating context w/%s.\n",
                (initiator_cred_handle == GSS_C_NO_CREDENTIAL) ?
                "GSS_C_NO_CREDENTIAL" :
                "Credentials provided" ) ;
#endif /* DEBUG */

/* DEE - test encryption, simulating client setting the flag */
#ifdef DEBUG
        if (req_flags & GSS_C_DELEG_FLAG)
        {
            if (getenv("DEE_DEBUG_ENC_D"))
            {
                fprintf(stderr,"DEE_FORCING GSS_C_CONF_FLAG\n");
                req_flags |= GSS_C_CONF_FLAG;
            } 
        }
        else
        {
            if (getenv("DEE_DEBUG_ENC"))
            {
                req_flags |= GSS_C_CONF_FLAG;
                fprintf(stderr,"DEE_FORCING GSS_C_CONF_FLAG\n");
            }
        }
#endif

        major_status = gss_create_and_fill_context(minor_status,
                                                   &context,
                                                   initiator_cred_handle,
                                                   GSS_C_INITIATE,
                                                   req_flags) ;
        if (GSS_ERROR(major_status))
        {
            return major_status;
        }

        *context_handle_P = context;

        if (actual_mech_type != NULL)
        {
            *actual_mech_type = (gss_OID) gss_mech_globus_gssapi_ssleay;
        }

        if (ret_flags != NULL)
        {
            *ret_flags = 0 ;
        }

        if (time_rec != NULL)
        {
            *time_rec = GSS_C_INDEFINITE ;
        }
    }
    else
    {
        /*
         * first time there is no input token, but after that
         * there will always be one
         */

    	major_status = gs_put_token(minor_status,context,input_token);
    	if (major_status != GSS_S_COMPLETE)
        {
            return major_status;
    	}
    }


    switch (context->gs_state)
    {
    case(GS_CON_ST_HANDSHAKE):
        /* do the handshake work */

        major_status = gs_handshake(minor_status,
                                    context);


        if (major_status == GSS_S_CONTINUE_NEEDED)
        {
            break;
        }
        /* if failed, may have SSL alert message too */
        if (major_status != GSS_S_COMPLETE)
        {
            context->gs_state = GS_CON_ST_DONE;
            break;
        } 
        /* make sure we are talking to the correct server */
        major_status = gs_retrieve_peer(minor_status, 
                                        context,
                                        GSS_C_INITIATE);
        if (major_status != GSS_S_COMPLETE)
        {
            context->gs_state = GS_CON_ST_DONE;
            break;
        }

        /* 
         * Need to check if the server is using a limited proxy. 
         * And if that is acceptable here. 
         * Caller tells us if it is not acceptable to 
         * use a limited proxy. 
         */
        if ((context->req_flags & GSS_C_GLOBUS_LIMITED_PROXY_FLAG)
            && context->pvd.limited_proxy)
        {
            GSSerr(GSSERR_F_INIT_SEC,GSSERR_R_PROXY_VIOLATION);
            *minor_status = GSSERR_R_PROXY_VIOLATION;
            major_status = GSS_S_UNAUTHORIZED;
            context->gs_state = GS_CON_ST_DONE;
            break;
        }

        /* this is the mutual authentication test */
        if (target_name != NULL)
        {
            inv_major_status = gss_compare_name(&inv_minor_status,
                                                context->target_name,
                                                target_name,
                                                &rc);
            if (inv_major_status != GSS_S_COMPLETE)
            {
                *minor_status = inv_minor_status;
                major_status  = inv_major_status;
                context->gs_state = GS_CON_ST_DONE;
                break;
            }
            else if( rc == 0)
            {
                GSSerr(GSSERR_F_INIT_SEC,GSSERR_R_MUTUAL_AUTH);
                {
                    gss_name_desc* n1 = 
                        (gss_name_desc*) context->target_name; 
                    gss_name_desc* n2 = 
                        (gss_name_desc*) target_name;
                    char * s1;
                    char * s2;
                    
                    s1 = X509_NAME_oneline(n1->x509n, NULL, 0);
                    s2 = X509_NAME_oneline(n2->x509n, NULL, 0);
                    
                    ERR_add_error_data(5,
                                       "\n Expected target subject name=\"",
                                       s2,
                                       "\"\n Target returned subject name=\"",
                                       s1, "\"");
                    
                    free (s1);
                    free (s2);
                }
                *minor_status = GSSERR_R_MUTUAL_AUTH;
                major_status = GSS_S_UNAUTHORIZED;
                context->gs_state = GS_CON_ST_DONE;
                break;
            }
        }
	
        context->ret_flags |= GSS_C_MUTUAL_FLAG;
        context->ret_flags |= GSS_C_PROT_READY_FLAG; 
        context->ret_flags |= GSS_C_INTEG_FLAG
            | GSS_C_REPLAY_FLAG
            | GSS_C_SEQUENCE_FLAG;
        if (context->pvd.limited_proxy)
        {
            context->ret_flags |= GSS_C_GLOBUS_LIMITED_PROXY_FLAG;
        }

        /* 
         * IF we are talking to a real SSL server,
         * we dont want to do delegation, so we are done
         */

        if (context->req_flags & GSS_C_GLOBUS_SSL_COMPATABLE)
        {
            context->gs_state = GS_CON_ST_DONE;
            break;
        }
			
        /*
         * If we have completed the handshake, but dont
         * have any more data to send, we can send the flag
         * now. i.e. fall through without break,
         * Otherwise, we will wait for the null byte
         * to get back in sync which we will ignore
         */

        if (output_token->length != 0)
        {
            context->gs_state=GS_CON_ST_FLAGS;
            break;
        }

    case(GS_CON_ST_FLAGS):
        if (input_token->length > 0)
        {	
            BIO_read(context->gs_sslbio,cbuf,1);
        }

        /* send D if we want delegation, 0 otherwise */
        
        if (context->req_flags & GSS_C_DELEG_FLAG)
        {
            BIO_write(context->gs_sslbio,"D",1); 
            context->gs_state=GS_CON_ST_REQ;
        }
        else
        {
            BIO_write(context->gs_sslbio,"0",1);
            context->gs_state=GS_CON_ST_DONE;
        } 
        break;
			
    case(GS_CON_ST_REQ):
        /* DEE? needs error processing here */
        /* Get the cert req */
        reqp = d2i_X509_REQ_bio(context->gs_sslbio,NULL);

        if (reqp == NULL)
        {
            GSSerr(GSSERR_F_INIT_SEC,GSSERR_R_PROXY_NOT_RECEIVED);
            major_status=GSS_S_FAILURE;
            return major_status;
        }
#ifdef DEBUG
        X509_REQ_print_fp(stderr,reqp);
#endif

        if ((extensions = sk_X509_EXTENSION_new_null()) == NULL)
        {
            GSSerr(GSSERR_F_INIT_SEC,GSSERR_R_CLASS_ADD_EXT);
            major_status = GSS_S_FAILURE;
            return major_status;
        }

#ifdef CLASS_ADD
        /* add channel binding data as class-add extension */

        if (input_chan_bindings && 
            input_chan_bindings->application_data.length > 0)
        {
            if ((ex = proxy_extension_class_add_create(
                     input_chan_bindings->application_data.value,
                     input_chan_bindings->application_data.length)) 
                == NULL)
            {
                GSSerr(GSSERR_F_INIT_SEC,GSSERR_R_CLASS_ADD_EXT);
                major_status = GSS_S_FAILURE;
                return major_status;
            }


            if (!sk_X509_EXTENSION_push(extensions, ex))
            {
                GSSerr(GSSERR_F_INIT_SEC,GSSERR_R_CLASS_ADD_EXT);
                major_status = GSS_S_FAILURE;
                return major_status;
            }
        }
#endif
        proxy_sign_ext(0,
                       context->cred_handle->pcd->ucert,
                       context->cred_handle->pcd->upkey,
                       EVP_md5(),
                       reqp,
                       &ncert,
                       time_req,
                       (context->req_flags &
                        GSS_C_GLOBUS_LIMITED_DELEG_PROXY_FLAG)? 1:0,
                       0,
                       "proxy",
                       extensions);
		
        if (extensions)
        {
            sk_X509_EXTENSION_pop_free(extensions, 
                                       X509_EXTENSION_free);
        }
			
#ifdef DEBUG
        X509_print_fp(stderr,ncert);
#endif
        i2d_X509_bio(context->gs_sslbio,ncert);
        context->gs_state = GS_CON_ST_DONE;
        X509_free(ncert);
        ncert = NULL;
        break;
			
    case(GS_CON_ST_CERT): ;
    case(GS_CON_ST_DONE): ;
    } /* end of switch for gs_con_st */
    if (!GSS_ERROR(major_status))
    {
        gs_get_token(minor_status,context,output_token);
        if (context->gs_state != GS_CON_ST_DONE)
        {
            major_status |=GSS_S_CONTINUE_NEEDED;
        }
        if (ret_flags != NULL)
        {
            *ret_flags = context->ret_flags;
        }
    }
#if defined(DEBUG) || defined(DEBUGX)
    fprintf(stderr,"init_sec_context:major_status:%08x:gs_state:%d req_flags=%08x:ret_flags=%08x\n",
            major_status,context->gs_state,req_flags,context->ret_flags);
    if (GSS_ERROR(major_status))
    {
        ERR_print_errors_fp(stderr);
    }
#endif
    return major_status;

}





