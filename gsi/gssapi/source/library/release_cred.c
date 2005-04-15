/*
 * Portions of this file Copyright 1999-2005 University of Chicago
 * Portions of this file Copyright 1999-2005 The University of Southern California.
 *
 * This file or a portion of this file is licensed under the
 * terms of the Globus Toolkit Public License, found at
 * http://www.globus.org/toolkit/download/license.html.
 * If you redistribute this file, with or without
 * modifications, you must include this notice in the file.
 */

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file release_cred.c
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
 * @name GSS Release Cred
 * @ingroup globus_gsi_gssapi
 */
/* @{ */
/**
 * Release the GSS cred handle
 *
 * @param minor_status
 *        The minor status result - this is a globus_result_t
 *        cast to a OM_uint32.  To access the globus error object
 *        use:  globus_error_get((globus_result_t) *minor_status)  
 * @param cred_handle_P
 *        The gss cred handle to be released 
 * @return
 *        The major status - GSS_S_COMPLETE or GSS_S_FAILURE
 */
OM_uint32 
GSS_CALLCONV gss_release_cred(
    OM_uint32 *                         minor_status,
    gss_cred_id_t *                     cred_handle_P)
{
    OM_uint32                           major_status = GSS_S_COMPLETE;
    gss_cred_id_desc**                  cred_handle =
        (gss_cred_id_desc**) cred_handle_P;
    OM_uint32                           local_minor_status = GSS_S_COMPLETE;
    OM_uint32                           local_major_status = GSS_S_COMPLETE;

    static char *                       _function_name_ =
        "gss_release_cred";
    GLOBUS_I_GSI_GSSAPI_DEBUG_ENTER;

    *minor_status = (OM_uint32) GLOBUS_SUCCESS;

    if (*cred_handle == NULL || *cred_handle == GSS_C_NO_CREDENTIAL )
    {
        goto exit;
    }

    if ((*cred_handle)->globusid != NULL)
    {
        local_major_status = gss_release_name(
            &local_minor_status,
            (void*) &((*cred_handle)->globusid));
    }

    globus_gsi_cred_handle_destroy((*cred_handle)->cred_handle);

    if((*cred_handle)->ssl_context)
    {
        X509_STORE_free((*cred_handle)->ssl_context->cert_store);
        (*cred_handle)->ssl_context->cert_store = NULL;
        SSL_CTX_free((*cred_handle)->ssl_context);
    }

    free(*cred_handle);
    *cred_handle = GSS_C_NO_CREDENTIAL;

 exit:
    GLOBUS_I_GSI_GSSAPI_DEBUG_EXIT;
    return major_status;
} 
/* gss_release_cred */
/* @} */
