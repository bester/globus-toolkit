#ifndef GLOBUS_INCLUDE_FTP_CLIENT_PERF_PLUGIN_H
#define GLOBUS_INCLUDE_FTP_CLIENT_PERF_PLUGIN_H

#include "globus_ftp_client.h"
#include "globus_ftp_client_plugin.h"

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

/** Module descriptor
 */
#define GLOBUS_FTP_CLIENT_PERF_PLUGIN_MODULE (&globus_i_ftp_client_perf_plugin_module)

extern
globus_module_descriptor_t globus_i_ftp_client_perf_plugin_module;

/**
 * Transfer begin callback
 *
 * This callback is called when a get, put, or third party transfer is
 * started.
 *
 * @param handle
 *        this the client handle that this transfer will be occurring on
 *
 * @param user_specific
 *        this is user specific data either created by the copy method,
 *        or, if a copy method was not specified, the value passed to
 *        init
 *
 * @return
 *        - n/a
 */

typedef void (*globus_ftp_client_perf_plugin_begin_cb_t)(
    globus_ftp_client_handle_t *                    handle,
    void *                                          user_specific);

/**
 * Performance marker received callback
 *
 * This callback is called for all types of transfers except a third
 * party in which extended block mode is not used (because 112 perf markers
 * wont be sent in that case). For extended mode 'put' and '3pt', actual 112
 * perf markers will be used and the frequency of this callback is dependent
 * upon the frequency those messages are received. For 'put' in which
 * extended block mode is not enabled and 'get' transfers, the information in
 * this callback will be determined locally and the frequency of this callback
 * will be at a maximum of one per second.
 *
 * @param handle
 *        this the client handle that this transfer is occurring on
 *
 * @param user_specific
 *        this is user specific data either created by the copy method,
 *        or, if a copy method was not specified, the value passed to
 *        init
 *
 * @param time_stamp
 *        the timestamp at which the number of bytes is valid
 *
 * @param stripe_ndx
 *        the stripe index this data refers to
 *
 * @param num_stripes total number of stripes involved in this transfer
 *
 * @param nbytes
 *        the total bytes transfered on this stripe
 *
 * @return
 *        - n/a
 */

typedef void (*globus_ftp_client_perf_plugin_marker_cb_t)(
    globus_ftp_client_handle_t *                    handle,
    void *                                          user_specific,
    time_t                                          time_stamp,
    int                                             stripe_ndx,
    int                                             num_stripes,
    globus_size_t                                   nbytes);

/**
 * Transfer complete callback
 *
 * This callback will be called upon transfer completion (successful or
 * otherwise)
 *
 * @param handle
 *        this the client handle that this transfer was occurring on
 *
 * @param user_specific
 *        this is user specific data either created by the copy method,
 *        or, if a copy method was not specified, the value passed to
 *        init
 *
 * @return
 *        - n/a
 */

typedef void (*globus_ftp_client_perf_plugin_complete_cb_t)(
    globus_ftp_client_handle_t *                    handle,
    void *                                          user_specific);

/**
 * Copy constructor
 *
 * This callback will be called when a copy of this plugin is made,
 * it is intended to allow initialization of a new user_specific data
 *
 * @param user_specific
 *        this is user specific data either created by this copy
 *        method, or the value passed to init
 *
 * @return
 *        - a pointer to a user specific piece of data
 *        - GLOBUS_NULL (does not indicate error)
 */

typedef void * (*globus_ftp_client_perf_plugin_user_copy_cb_t)(
    void *                                          user_specific);

/**
 * Destructor
 *
 * This callback will be called when a copy of this plugin is destroyed,
 * it is intended to allow the user to free up any memory associated with
 * the user specific data
 *
 * @param user_specific
 *        this is user specific data created by the copy method
 *
 * @return
 *        - n/a
 */

typedef void (*globus_ftp_client_perf_plugin_user_destroy_cb_t)(
    void *                                          user_specific);

globus_result_t
globus_ftp_client_perf_plugin_init(
    globus_ftp_client_plugin_t *                    plugin,
    globus_ftp_client_perf_plugin_begin_cb_t        begin_cb,
    globus_ftp_client_perf_plugin_marker_cb_t       marker_cb,
    globus_ftp_client_perf_plugin_complete_cb_t     complete_cb,
    globus_ftp_client_perf_plugin_user_copy_cb_t    copy_cb,
    globus_ftp_client_perf_plugin_user_destroy_cb_t destroy_cb,
    void *                                          user_specific);

globus_result_t
globus_ftp_client_perf_plugin_destroy(
    globus_ftp_client_plugin_t *                    plugin);

globus_result_t
globus_ftp_client_perf_plugin_get_user_specific(
    globus_ftp_client_plugin_t *                    plugin,
    void **                                         user_specific);

EXTERN_C_END

#endif /* GLOBUS_INCLUDE_FTP_CLIENT_PERF_PLUGIN_H */
