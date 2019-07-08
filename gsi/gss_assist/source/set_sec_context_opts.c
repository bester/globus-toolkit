
#include "globus_gss_assist.h"
#include <gssapi.h>
#include <malloc.h>


extern
const gss_OID_desc * const gss_restrictions_extension;


OM_uint32
globus_gss_assist_will_handle_restrictions(
    OM_uint32 *                         minor_status,
    gss_ctx_id_t *                      context_handle)
{
    OM_uint32                           maj_stat;
    gss_buffer_desc                     oid_buffer;
    gss_OID_set_desc                    oid_set;

    oid_set.count = 1;
    oid_set.elements = (gss_OID) gss_restrictions_extension;

    oid_buffer.value = (void *) &oid_set;
    oid_buffer.length = 1;
    
    maj_stat = gss_set_sec_context_option(
        minor_status,
        context_handle,
        (gss_OID) GSS_APPLICATION_WILL_HANDLE_EXTENSIONS,
        &oid_buffer);

    return maj_stat;
}
