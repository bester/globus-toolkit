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
 * @file globus_gridmap_callout_error.c
 * @author Sam Meder
 *
 * $RCSfile$
 * $Revision$
 * $Date$
 */

#include "globus_gridmap_callout_error.h"
#include "version.h"

char * 
globus_i_gridmap_callout_error_strings[GLOBUS_GRIDMAP_CALLOUT_ERROR_LAST] =
{
/* 0 */   "Gridmap lookup failure",
/* 1 */   "A GSSAPI call returned an error",
/* 2 */   "Caller provided insufficient buffer space for local identity"
};

static int globus_l_gridmap_callout_error_activate(void);
static int globus_l_gridmap_callout_error_deactivate(void);


/**
 * Module descriptor static initializer.
 */
globus_module_descriptor_t globus_i_gridmap_callout_error_module =
{
    "globus_gridmap_callout_error",
    globus_l_gridmap_callout_error_activate,
    globus_l_gridmap_callout_error_deactivate,
    GLOBUS_NULL,
    GLOBUS_NULL,
    &local_version
};

/**
 * Module activation
 */
static
int
globus_l_gridmap_callout_error_activate(void)
{
    globus_module_activate(GLOBUS_COMMON_MODULE);
    return 0;
}

/**
 * Module deactivation
 *
 */
static
int
globus_l_gridmap_callout_error_deactivate(void)
{
    globus_module_deactivate(GLOBUS_COMMON_MODULE);
    return 0;
}


#endif

