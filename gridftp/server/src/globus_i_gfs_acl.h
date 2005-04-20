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

#ifndef GLOBUS_I_GFS_ACL_H
#define GLOBUS_I_GFS_ACL_H

#include <pwd.h>
#include <sys/types.h>
#include <grp.h>

struct globus_i_gfs_acl_handle_s;

typedef enum globus_l_gfs_acl_type_e
{
    GLOBUS_L_GFS_ACL_TYPE_INIT,
    GLOBUS_L_GFS_ACL_TYPE_AUTHORIZE
} globus_i_gfs_acl_type_t;

/*
 *  user functions.  used by control.c or DSI implementation if it choses.
 */
typedef void
(*globus_gfs_acl_cb_t)(
    const char *                        resource_id,
    void *                              user_arg,
    globus_result_t                     result);


int
globus_gfs_acl_authorize(
    struct globus_i_gfs_acl_handle_s *  acl_handle,
    const char *                        action,
    const char *                        object,
    globus_result_t *                   out_res,
    globus_gfs_acl_cb_t                 cb,
    void *                              user_arg);

int
globus_i_gfs_acl_init(
    struct globus_i_gfs_acl_handle_s *  acl_handle,
    const gss_ctx_id_t                  context,
    const struct passwd *               pwent,
    const struct group *                grent,
    const char *                        given_pw,
    const char *                        ipaddr,
    const char *                        resource_id,
    globus_result_t *                   out_res,
    globus_gfs_acl_cb_t                 cb,
    void *                              user_arg);

void
globus_i_gfs_acl_destroy(
    struct globus_i_gfs_acl_handle_s *  acl_handle);

/*
 *  interface implementation functions
 */
enum
{
    GLOBUS_GFS_ACL_COMPLETE,
    GLOBUS_GFS_ACL_WOULD_BLOCK
};

typedef int
(*globus_gfs_acl_init_t)(
    void **                             out_handle,
    const struct passwd *               passwd,
    const char *                        given_pw,
    const char *                        resource_id,
    struct globus_i_gfs_acl_handle_s *  acl_handle,
    globus_result_t *                   out_res);

typedef int
(*globus_gfs_acl_authorize_t)(
    void *                              out_handle,
    const char *                        action,
    const char *                        object,
    struct globus_i_gfs_acl_handle_s *  acl_handle,
    globus_result_t *                   out_res);

void
globus_gfs_acl_authorized_finished(
    struct globus_i_gfs_acl_handle_s *  acl_handle,
    globus_result_t                     result);

typedef void
(*globus_gfs_acl_destroy_t)(
    void *                              out_handle);

typedef struct globus_gfs_acl_module_s
{
    globus_gfs_acl_init_t               init_func;
    globus_gfs_acl_authorize_t          authorize_func;
    globus_gfs_acl_destroy_t            destroy_func;
} globus_gfs_acl_module_t;

typedef struct globus_i_gfs_acl_handle_s
{
    struct passwd                       pwent;
    struct group                        grpent;
    char *                              given_pw;
    char *                              ipaddr;
    globus_i_gfs_acl_type_t             type;
    char *                              user_id;
    char *                              hostname;
    char *                              auth_action;
    char *                              auth_object;
    globus_gfs_acl_cb_t                 cb;
    void *                              user_arg;
    globus_list_t *                     module_list;
    globus_list_t *                     current_list;
    globus_result_t                     cached_res;
    gss_ctx_id_t                        context;
} globus_i_gfs_acl_handle_t;

void
globus_gfs_acl_add_module(
    globus_gfs_acl_module_t *           module);

#endif
