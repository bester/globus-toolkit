#if !defined(GLOBUS_I_GRIDFTP_SERVER_CONTROL_H)
#define GLOBUS_I_GRIDFTP_SERVER_CONTROL_H 1

#include "globus_gridftp_server_control.h"
#include "globus_xio.h"

#define GLOBUS_GRIDFTP_SERVER_HASHTABLE_SIZE    256
#define GLOBUS_GRIDFTP_VERSION_CTL              1

GlobusDebugDeclare(GLOBUS_GRIDFTP_SERVER_CONTROL);


#define GlobusGSDebugPrintf(level, message)                                \
    GlobusDebugPrintf(GLOBUS_GRIDFTP_SERVER_CONTROL, level, message)


#define GlobusGridFTPServerDebugEnter()                                     \
    GlobusGSDebugPrintf(                                                    \
        GLOBUS_GRIDFTP_SERVER_CONTROL_DEBUG_TRACE,                          \
        ("[%s] Entering\n", _gridftp_server_name))

#define GlobusGridFTPServerDebugExit()                                      \
    GlobusGSDebugPrintf(                                                    \
        GLOBUS_GRIDFTP_SERVER_CONTROL_DEBUG_TRACE,                          \
        ("[%s] Exiting\n", _gridftp_server_name))
    
#define GlobusGridFTPServerDebugExitWithError()                             \
    GlobusGSDebugPrintf(                                                    \
        GLOBUS_GRIDFTP_SERVER_CONTROL_DEBUG_TRACE,                          \
        ("[%s] Exiting with error\n", _gridftp_server_name))
    
#define GlobusGridFTPServerDebugInternalEnter()                             \
    GlobusGSDebugPrintf(                                                    \
        GLOBUS_GRIDFTP_SERVER_CONTROL_DEBUG_INTERNAL_TRACE,                 \
        ("[%s] I Entering\n", _gridftp_server_name))

#define GlobusGridFTPServerDebugInternalExit()                              \
    GlobusGSDebugPrintf(                                                    \
        GLOBUS_GRIDFTP_SERVER_CONTROL_DEBUG_INTERNAL_TRACE,                 \
        ("[%s] I Exiting\n", _gridftp_server_name))
    
#define GlobusGridFTPServerDebugInternalExitWithError()                     \
    GlobusGSDebugPrintf(                                                    \
        GLOBUS_GRIDFTP_SERVER_CONTROL_DEBUG_INTERNAL_TRACE,                 \
        ("[%s] I Exiting with error\n", _gridftp_server_name))


#define GlobusGridFTPServerErrorParameter(param_name)                       \
    globus_error_put(                                                       \
        globus_error_construct_error(                                       \
            GLOBUS_GRIDFTP_SERVER_CONTROL_MODULE,                           \
            GLOBUS_NULL,                                                    \
            GLOBUS_GRIDFTP_SERVER_CONTROL_ERROR_PARAMETER,                  \
            __FILE__,                                                       \
            _gridftp_server_name,                                           \
            __LINE__,                                                       \
            "Bad parameter, %s",                                            \
            (param_name)))

#define GlobusGridFTPServerErrorMemory(mem_name)                            \
    globus_error_put(                                                       \
        globus_error_construct_error(                                       \
            GLOBUS_GRIDFTP_SERVER_CONTROL_MODULE,                           \
            GLOBUS_NULL,                                                    \
            GLOBUS_GRIDFTP_SERVER_CONTROL_ERROR_MEMORY,                     \
            __FILE__,                                                       \
            _gridftp_server_name,                                           \
            __LINE__,                                                       \
            "Memory allocation failed on %s",                               \
            (mem_name)))

#define GlobusGridFTPServerErrorState(state)                                \
    globus_error_put(                                                       \
        globus_error_construct_error(                                       \
            GLOBUS_GRIDFTP_SERVER_CONTROL_MODULE,                           \
            GLOBUS_NULL,                                                    \
            GLOBUS_GRIDFTP_SERVER_CONTROL_ERROR_STATE,                      \
            __FILE__,                                                       \
            _gridftp_server_name,                                           \
            __LINE__,                                                       \
            "Invalid state: %d",                                            \
            (state)))

#define GlobusGridFTPServerNotAuthenticated()                               \
    globus_error_put(                                                       \
        globus_error_construct_error(                                       \
            GLOBUS_GRIDFTP_SERVER_CONTROL_MODULE,                           \
            GLOBUS_NULL,                                                    \
            GLOBUS_GRIDFTP_SERVER_CONTROL_NO_AUTH,                          \
            __FILE__,                                                       \
            _gridftp_server_name,                                           \
            __LINE__,                                                       \
            "Not yet authenticated."))

#define GlobusGridFTPServerPostAuthenticated()                              \
    globus_error_put(                                                       \
        globus_error_construct_error(                                       \
            GLOBUS_GRIDFTP_SERVER_CONTROL_MODULE,                           \
            GLOBUS_NULL,                                                    \
            GLOBUS_GRIDFTP_SERVER_CONTROL_POST_AUTH,                        \
            __FILE__,                                                       \
            _gridftp_server_name,                                           \
            __LINE__,                                                       \
            "Not yet authenticated."))

#define GlobusGridFTPServerNotACommand()                                    \
    globus_error_put(                                                       \
        globus_error_construct_error(                                       \
            GLOBUS_GRIDFTP_SERVER_CONTROL_MODULE,                           \
            GLOBUS_NULL,                                                    \
            GLOBUS_GRIDFTP_SERVER_CONTROL_NO_COMMAND,                       \
            __FILE__,                                                       \
            _gridftp_server_name,                                           \
            __LINE__,                                                       \
            "Command not implemented."))

#define GlobusGridFTPServerOpSetUserArg(_in_op, _in_arg)                    \
    (_in_op)->user_arg = (_in_arg);                                         \

#define GlobusGridFTPServerOpGetUserArg(_in_op)                             \
    ((_in_op)->user_arg)

#define GlobusGridFTPServerOpGetServer(_in_op)                              \
    ((_in_op)->server)

#define GlobusGridFTPServerOpGetPModArg(_in_op)                             \
    ((_in_op)->pmod_arg)

#define GlobusGridFTPServerOpSetPModArg(_in_op, _in_arg)                    \
    (_in_op)->pmod_arg = (_in_arg);                                         \



struct globus_i_gs_attr_s;

typedef enum globus_gridftp_server_debug_levels_e
{ 
    GLOBUS_GRIDFTP_SERVER_CONTROL_DEBUG_ERROR = 1,
    GLOBUS_GRIDFTP_SERVER_CONTROL_DEBUG_WARNING = 2,
    GLOBUS_GRIDFTP_SERVER_CONTROL_DEBUG_TRACE = 4,
    GLOBUS_GRIDFTP_SERVER_CONTROL_DEBUG_INTERNAL_TRACE = 8,
    GLOBUS_GRIDFTP_SERVER_CONTROL_DEBUG_INFO = 16,
    GLOBUS_GRIDFTP_SERVER_CONTROL_DEBUG_INFO_VERBOSE = 32
} globus_gridftp_server_debug_levels_t;

typedef enum globus_gridftp_server_error_type_e
{
    GLOBUS_GRIDFTP_SERVER_CONTROL_ERROR_PARAMETER,
    GLOBUS_GRIDFTP_SERVER_CONTROL_ERROR_STATE,
    GLOBUS_GRIDFTP_SERVER_CONTROL_ERROR_MEMORY,
    GLOBUS_GRIDFTP_SERVER_CONTROL_NO_AUTH,
    GLOBUS_GRIDFTP_SERVER_CONTROL_POST_AUTH,
    GLOBUS_GRIDFTP_SERVER_CONTROL_NO_COMMAND,
    GLOBUS_GRIDFTP_SERVER_CONTROL_MALFORMED_COMMAND
} globus_gridftp_server_error_type_t;

typedef enum globus_i_gsc_conn_dir_e
{
    GLOBUS_I_GSC_CONN_DIR_PASV,
    GLOBUS_I_GSC_CONN_DIR_PORT
} globus_i_gsc_conn_dir_t;

typedef struct globus_i_gsc_data_s
{
    void *                                          user_handle;
    globus_gridftp_server_control_data_dir_t        data_dir;
    globus_i_gsc_conn_dir_t                         conn_dir;
} globus_i_gsc_data_t;

typedef void
(*globus_gsc_command_func_t)(
    struct globus_i_gsc_op_s *              op,
    const char *                            full_command,
    char **                                 cmd_array,
    int                                     argc,
    void *                                  user_arg);

typedef void
(*globus_i_gsc_auth_callback_t)(
    struct globus_i_gsc_op_s *              op,
    globus_result_t                         result,
    void *                                  user_arg);

typedef void
(*globus_i_gsc_resource_callback_t)(
    struct globus_i_gsc_op_s *              op,
    globus_result_t                         result,
    char *                                  path,
    globus_gridftp_server_control_stat_t *  stat_info,
    int                                     stat_count,
    void *                                  user_arg);

typedef void
(*globus_i_gsc_passive_callback_t)(
    struct globus_i_gsc_op_s *              op,
    globus_result_t                         result,
    const char **                           cs,
    int                                     addr_count,
    void *                                  user_arg);

typedef void
(*globus_i_gsc_port_callback_t)(
    struct globus_i_gsc_op_s *              op,
    globus_result_t                         result,
    void *                                  user_arg);

typedef enum globus_i_gsc_op_type_e
{
    GLOBUS_L_GSC_OP_TYPE_AUTH,
    GLOBUS_L_GSC_OP_TYPE_RESOURCE,
    GLOBUS_L_GSC_OP_TYPE_CREATE_PASV,
    GLOBUS_L_GSC_OP_TYPE_CREATE_PORT,
    GLOBUS_L_GSC_OP_TYPE_DATA,
    GLOBUS_L_GSC_OP_TYPE_DESTROY,
    GLOBUS_L_GSC_OP_TYPE_MOVE,
    GLOBUS_L_GSC_OP_TYPE_DELETE,
    GLOBUS_L_GSC_OP_TYPE_MKDIR,
    GLOBUS_L_GSC_OP_TYPE_RMDIR
} globus_i_gsc_op_type_t;

typedef struct globus_i_gsc_op_s
{
    globus_i_gsc_op_type_t                  type;

    struct globus_i_gsc_server_handle_s *   server_handle;
    globus_result_t                         res;

    globus_list_t *                         cmd_list;

    /* stuff for auth */
    globus_bool_t                           authenticated;
    char *                                  username;
    char *                                  password;
    gss_cred_id_t                           cred;
    gss_cred_id_t                           del_cred;
    globus_i_gsc_auth_callback_t            auth_cb;
    globus_i_gsc_resource_callback_t        stat_cb;

    uid_t                                   uid;

    /* stuff for resource */
    char *                                  path;
    globus_gridftp_server_control_resource_mask_t   mask;

    /* stuff for port/pasv */
    char **                                 cs;
    int                                     max_cs;
    int                                     net_prt;
    globus_i_gsc_passive_callback_t         passive_cb;
    globus_i_gsc_port_callback_t            port_cb;

    char *                                  command;

    /* stuff for transfer */
    char *                                          mod_name;
    char *                                          mod_parms;
    globus_gridftp_server_control_transfer_func_t   user_data_cb;
    globus_bool_t                                   transfer_started;

    void *                                          user_arg;
} globus_i_gsc_op_t;

typedef struct globus_i_gsc_attr_s
{
    int                                     version_ctl;
    globus_hashtable_t                      send_func_table;
    globus_hashtable_t                      recv_func_table;
    globus_gridftp_server_control_resource_cb_t resource_func;
    globus_gridftp_server_control_callback_t        done_func;
    char *                                  modes;
    char *                                  types;
    char *                                  base_dir;

    globus_gridftp_server_control_auth_cb_t         auth_func;
    globus_gridftp_server_control_passive_connect_t passive_func;
    globus_gridftp_server_control_active_connect_t  active_func;
    globus_gridftp_server_control_data_destroy_t    data_destroy_func;

    globus_gridftp_server_control_transfer_func_t   default_stor;
    globus_gridftp_server_control_transfer_func_t   default_retr;
} globus_i_gsc_attr_t;


extern globus_hashtable_t               globus_i_gs_default_attr_command_hash;

/*
 *  internal functions for adding commands.
 */

typedef enum globus_gridftp_server_command_desc_e
{
    GLOBUS_GRIDFTP_SERVER_CONTROL_COMMAND_DESC_REFRESH = 0x01,
    GLOBUS_GRIDFTP_SERVER_CONTROL_COMMAND_DESC_POST_AUTH = 0x02,
    GLOBUS_GRIDFTP_SERVER_CONTROL_COMMAND_DESC_PRE_AUTH = 0x04
} globus_gridftp_server_command_desc_t;

/*
 *   959 Structures
 */
typedef enum globus_gsc_command_desc_e
{
    GLOBUS_GSC_COMMAND_POST_AUTH = 0x01,
    GLOBUS_GSC_COMMAND_PRE_AUTH = 0x02
} globus_gsc_command_desc_t;

typedef enum globus_l_gsc_state_e
{
    GLOBUS_L_GSC_STATE_OPEN,
    GLOBUS_L_GSC_STATE_PROCESSING,
    GLOBUS_L_GSC_STATE_ABORTING,
    GLOBUS_L_GSC_STATE_ABORTING_STOPPING,
    GLOBUS_L_GSC_STATE_STOPPING,
    GLOBUS_L_GSC_STATE_STOPPED,
} globus_l_gsc_state_t;

/* the server handle */
typedef struct globus_i_gsc_server_handle_s
{
    int                                     version_ctl;

    globus_mutex_t                          mutex;

    /*
     *  authentication information
     */
    char *                                  username;
    char *                                  pw;
    char *                                  banner;
    gss_cred_id_t                           cred;
    gss_cred_id_t                           del_cred;
    globus_i_gsc_auth_callback_t            auth_cb;
    uid_t                                   uid;

    char *                                  pre_auth_banner;

    /*
     *  state information  
     */
    char *                                  cwd;
    char                                    type;
    char                                    mode;
    char *                                  modes;
    char *                                  types;
    int                                     parallelism;
    globus_size_t                           send_buf;
    globus_size_t                           receive_buf;
    globus_bool_t                           refresh;
    globus_size_t                           packet_size;
    globus_bool_t                           delayed_passive;
    int                                     pasv_prt;
    int                                     pasv_max;
    globus_bool_t                           passive_only;
    globus_bool_t                           opts_delayed_passive;
    int                                     opts_pasv_prt;
    int                                     opts_pasv_max;
    int                                     opts_dc_parsing_alg;
    int                                     opts_port_prt;
    int                                     opts_port_max;

    globus_bool_t                           authenticated;
    /*
     *  user function pointers
     */
    void *                                          user_arg;

    /* transfer functions */
    globus_hashtable_t                              send_table;
    globus_hashtable_t                              recv_table;
    globus_gridftp_server_control_transfer_func_t   default_stor_cb;
    globus_gridftp_server_control_transfer_func_t   default_retr_cb;

    globus_gridftp_server_control_auth_cb_t         auth_func;

    /* data functions */
    globus_gridftp_server_control_passive_connect_t passive_func;
    globus_gridftp_server_control_active_connect_t  active_func;
    globus_gridftp_server_control_data_destroy_t    data_destroy_func;

    /* list function */
    globus_gridftp_server_control_resource_cb_t resource_func;
    /* done function */
    globus_gridftp_server_control_callback_t        done_func;

    globus_gridftp_server_control_abort_func_t      abort_func;
    void *                                          abort_arg;
    
    globus_i_gsc_data_t *                   data_object;
    globus_fifo_t                           data_q;

    globus_result_t                         cached_res;

    /* 
     *  read.c members 
     */
    globus_bool_t                           reply_outstanding;
    globus_xio_handle_t                     xio_handle;
    globus_l_gsc_state_t                    state;
    globus_fifo_t                           read_q;
    globus_fifo_t                           reply_q;
    int                                     abort_cnt;
    globus_hashtable_t                      cmd_table;
    struct globus_i_gsc_op_s *              outstanding_op;
} globus_i_gsc_server_handle_t;

typedef struct globus_l_gsc_cmd_ent_s
{
    int                                     cmd;
    char                                    cmd_name[16]; /* only 5 needed */
    globus_gsc_command_func_t               cmd_func;
    globus_gsc_command_desc_t               desc;
    char *                                  help;
    void *                                  user_arg;
    int                                     argc;
} globus_l_gsc_cmd_ent_t;

typedef struct globus_l_gsc_reply_ent_s
{
    char *                                  msg;
    globus_bool_t                           final;
    globus_i_gsc_op_t *                     op;
} globus_l_gsc_reply_ent_t;

void
globus_i_gsc_terminate(
    globus_i_gsc_server_handle_t *          server_handle);

char *
globus_i_gsc_get_help(
    globus_i_gsc_server_handle_t *          server_handle,
    const char *                            command_name);

globus_result_t
globus_i_gsc_intermediate_reply(
    globus_i_gsc_op_t *                     op,
    char *                                  reply_msg);

void
globus_i_gsc_finished_command(
    globus_i_gsc_op_t *                     op,
    char *                                  reply_msg);

globus_result_t
globus_i_gsc_command_add(
    globus_i_gsc_server_handle_t *          server_handle,
    const char *                            command_name,
    globus_gsc_command_func_t               command_func,
    globus_gsc_command_desc_t               desc,
    int                                     argc,
    const char *                            help,
    void *                                  user_arg);

globus_result_t
globus_i_gsc_authenticate(
    globus_i_gsc_op_t *                     op,
    const char *                            user,
    const char *                            pass,
    gss_cred_id_t                           cred,
    gss_cred_id_t                           del_cred,
    globus_i_gsc_auth_callback_t            cb,
    void *                                  user_arg);

globus_result_t
globus_i_gsc_resource_query(
    globus_i_gsc_op_t *                     op,
    const char *                            path,
    int                                     mask,
    globus_i_gsc_resource_callback_t        cb,
    void *                                  user_arg);

globus_result_t
globus_i_gsc_passive(
    globus_i_gsc_op_t *                     op,
    int                                     max,
    int                                     net_prt,
    globus_i_gsc_passive_callback_t         cb,
    void *                                  user_arg);

globus_result_t
globus_i_gsc_port(
    globus_i_gsc_op_t *                     op,
    const char **                           contact_strings,
    int                                     stripe_count,
    int                                     net_prt,
    globus_i_gsc_port_callback_t            cb,
    void *                                  user_arg);

void
globus_i_gsc_add_commands(
    globus_i_gsc_server_handle_t *          server_handle);

globus_result_t
globus_i_gsc_command_panic(
    globus_i_gsc_op_t *                     op);

char *
globus_i_gsc_concat_path(
    globus_i_gsc_server_handle_t *                  i_server,
    const char *                                    in_path);

#endif
