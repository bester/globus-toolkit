#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file release_name.c
 * @author Sam Meder, Sam Lang
 * 
 * $RCSfile$
 * $Revision$
 * $Date$
 */
#endif

static char *rcsid = "$Id$";

#include "gssapi_openssl.h"
#include "globus_i_gsi_gss_utils.h"

/**
 * @name GSS Release Name
 * @ingroup globus_gsi_gssapi
 */
/* @{ */
/**
 * Release the GSS Name
 *
 * @param minor_status
 *        The minor status result - this is a globus_result_t
 *        cast to a (OM_uint32 *).
 * @param name_P
 *        The gss name to be released
 * @return
 *        The major status - GSS_S_COMPLETE or GSS_S_FAILURE
 */
OM_uint32 
GSS_CALLCONV gss_release_name(
    OM_uint32 *                         minor_status,
    gss_name_t *                        name_P)
{
    OM_uint32                           major_status = GSS_S_COMPLETE;
    gss_name_desc** name = (gss_name_desc**) name_P ;

    static char *                       _function_name_ =
        "gss_release_name";

    GLOBUS_I_GSI_GSSAPI_DEBUG_ENTER;

    minor_status = (OM_uint32 *) GLOBUS_SUCCESS;

    if (name == NULL || *name == NULL || *name == GSS_C_NO_NAME)
    {
        goto exit;
    } 
    
    if ((*name)->x509n)
    {
        X509_NAME_free((*name)->x509n);
    }

    if((*name)->group)
    {
        sk_pop_free((*name)->group, free);
    }

    if((*name)->group_types)
    {
        ASN1_BIT_STRING_free((*name)->group_types); 
    }
    
    free(*name);
    *name = GSS_C_NO_NAME;
    
 exit:
    GLOBUS_I_GSI_GSSAPI_DEBUG_EXIT;
    return major_status;
    
} 
/* gss_release_name */
/* @} */
