#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file globus_i_error_generic.h
 * Globus Generic Error
 *
 * $RCSfile$
 * $Revision$
 * $Date $
 */

#include "globus_common.h"

#ifndef GLOBUS_I_INCLUDE_GENERIC_ERROR_H
#define GLOBUS_I_INCLUDE_GENERIC_ERROR_H

#ifndef EXTERN_C_BEGIN
#ifdef __cplusplus
#define EXTERN_C_BEGIN extern "C" {
#define EXTERN_C_END }
#else
#define EXTERN_C_BEGIN
#define EXTERN_C_END
#endif
#endif

EXTERN_C_BEGIN

/**
 * Generic Error object instance data definition
 * @ingroup globus_generic_error_object
 * @internal
 *
 * This structure contains all of the data associated with a Globus
 * Generic Error.
 *
 * @see globus_error_construct_error(),
 *      globus_error_initialize_error(),
 *      globus_l_error_free_globus()
 */

typedef struct globus_l_error_data_s
{
    /** the error type */
    int                                 type;
    /** the short error description */
    char *                              short_desc;
    /** the long error description */
    char *                              long_desc;    
}
globus_l_error_data_t;

EXTERN_C_END

#endif /* GLOBUS_I_INCLUDE_GENERIC_ERROR_H */

#endif /* GLOBUS_DONT_DOCUMENT_INTERNAL */
