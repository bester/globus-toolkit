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

#ifndef GLOBUS_COMMON_RANGE_LIST_H
#define GLOBUS_COMMON_RANGE_LIST_H

#include "globus_common_include.h"
/********************************************************************
 *
 * This file defines the globus_range_list_t type
 *
 *
 ********************************************************************/
EXTERN_C_BEGIN

#define GLOBUS_RANGE_LIST_MAX -1

typedef enum
{
    GLOBUS_RANGE_LIST_ERROR_PARAMETER = -1,
    GLOBUS_RANGE_LIST_ERROR_MEMORY = -2
} globus_range_list_error_type_t;

typedef struct globus_l_range_list_s *  globus_range_list_t;


/* destructive merge of two range lists.  will leave the source range
 * lists empty.  globus_range_list_destroy() will still need to be called
 * on source lists. 
 */
int
globus_range_list_merge_destructive(
    globus_range_list_t *               dest,
    globus_range_list_t                 src1,
    globus_range_list_t                 src2);

int
globus_range_list_merge(
    globus_range_list_t *               dest,
    globus_range_list_t                 src1,
    globus_range_list_t                 src2);

int
globus_range_list_copy(
    globus_range_list_t *               dest,
    globus_range_list_t                 src);

int
globus_range_list_init(
    globus_range_list_t *               range_list);

void
globus_range_list_destroy(
    globus_range_list_t                 range_list);

int
globus_range_list_insert(
    globus_range_list_t                 range_list,
    globus_off_t                        offset,
    globus_off_t                        length);

int
globus_range_list_remove(
    globus_range_list_t                 range_list,
    globus_off_t                        offset,
    globus_off_t                        length);

int
globus_range_list_size(
    globus_range_list_t                 range_list);

int
globus_range_list_at(
    globus_range_list_t                 range_list,
    int                                 ndx,
    globus_off_t *                      offset,
    globus_off_t *                      length);

int
globus_range_list_remove_at(
    globus_range_list_t                 range_list,
    int                                 ndx,
    globus_off_t *                      offset,
    globus_off_t *                      length);


EXTERN_C_END

#endif /* GLOBUS_LIST_H */


