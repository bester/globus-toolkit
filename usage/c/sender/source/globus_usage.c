
#include "globus_xio.h"
#include "globus_xio_udp_driver.h"
#include "version.h"
#include "globus_usage.h"
#include "stdarg.h"

static globus_xio_stack_t               globus_l_usage_stats_stack;
static globus_xio_driver_t              globus_l_usage_stats_udp_driver;
static globus_xio_handle_t              globus_l_usage_stats_xio_handle;
static globus_mutex_t                   globus_l_usage_stats_mutex;

#define GLOBUS_L_USAGE_STATS_DEFAULT_TARGETS "usage-stats.globus.org:4810"
#define GLOBUS_L_USAGE_STATS_TIMESTAMP_OFFSET 20

enum
{
    GLOBUS_L_USAGE_STATS_DEBUG_MESSAGES = 0x01
};

GlobusDebugDefine(GLOBUS_USAGE);

#define GlobusUsageStatsDebugPrintf(LEVEL, MESSAGE) \
    GlobusDebugPrintf(GLOBUS_USAGE, LEVEL, MESSAGE)

#define GlobusUsageStatsConvertToPrintable(CHARVALUE)                          \
    (((((unsigned int)CHARVALUE) > 31) &&                                      \
      ((((unsigned int)CHARVALUE) < 127)) ? CHARVALUE : '.'))

#define GlobusUsageStatsDebugDump(LEVEL, DATA, LENGTH)                         \
    {                                                                          \
        int i = 0;                                                             \
        for(; i < LENGTH; ++i)                                                 \
        {                                                                      \
            char cv = GlobusUsageStatsConvertToPrintable(DATA[i]);             \
            GlobusUsageStatsDebugPrintf(                                       \
                LEVEL, ("%c", cv));                                            \
        }                                                                      \
    }

static int
globus_l_usage_stats_activate();

static int
globus_l_usage_stats_deactivate();

globus_module_descriptor_t
globus_i_usage_stats_module =
{
    "globus_usage_stats_module",
    globus_l_usage_stats_activate,
    globus_l_usage_stats_deactivate,
    NULL, NULL,
    &local_version
};

typedef struct globus_usage_stats_handle_s
{
    uint16_t                            code;
    uint16_t                            version;
    globus_list_t *                     targets;
    globus_list_t *                     xio_desc_list;
    const char *                        optout;
    int                                 header_length;
    unsigned char                       data[1472];
} globus_i_usage_stats_handle_t;

static
char *
globus_l_usage_stats_my_strtok(
    const char *                        str,
    size_t                              str_length,
    int *                               token_start_index,
    int *                               token_end_index,
    const char *                        delims)
{
    const char *                        locator;
    const char *                        start_token;
    int                                 delim_count, 
                                        ind, 
                                        i, 
                                        between_tokens, 
                                        len;
    char *                              new_token = NULL;
    ind = 0;
    i = 0;
    between_tokens = 0;
    delim_count = strlen(delims);
    locator = str;
    
    for(i = 0; i < delim_count; ++i)
    {
        if(*locator == delims[i])
        {
            between_tokens = 1;
        }
    }

    while(between_tokens && locator && ind < str_length)
    {
        for(i = 0; i < delim_count; ++i)
        {
            if(*locator == delims[i])
            {
                break;
            }
        }

        if(i == delim_count)
        {
            between_tokens = 0;
            break;
        }

        locator++;
        ind++;
    }

    start_token = locator;
    if(token_start_index)
    {
        *token_start_index = ind;
    }

    while(!between_tokens && *locator && ind < str_length)
    {
        for(i = 0; i < delim_count; ++i)
        {
            if(*locator == delims[i])
            {
                if(token_end_index)
                {
                    *token_end_index = ind;
                }
                between_tokens = 1;
                break;
            }
        }

        locator++;
        ind++;
    }

    if(ind == str_length)
    {
        *token_end_index = -1;
    }
    else
    {
        ind--;
    }

    len = ind + (str - start_token);
    if(len > 0)
    {
        new_token = malloc(len+1);
        memcpy(new_token, start_token, len + 1);
        new_token[len] = '\0';
    }

    return new_token;
}

static
int
globus_l_usage_stats_split_targets(
    const char *                        targets_string,
    globus_list_t **                    targets)
{
    char *                              tmpstr;
    char *                              newstr;
    int                                 start_token;
    int                                 end_token;
    char *                              token;

    newstr = globus_libc_strdup(targets_string);
    tmpstr = newstr;
    token = globus_l_usage_stats_my_strtok(
        tmpstr, strlen(tmpstr), &start_token, &end_token, "\n\t ");
    tmpstr += end_token;
        
    while(1)
    {
        globus_list_insert(targets, token);
        if(end_token < 0)
        {
            break;
        }

        token = globus_l_usage_stats_my_strtok(
            tmpstr, strlen(tmpstr), &start_token, &end_token, "\n\t ");
        tmpstr += end_token;
        if(!token)
        {
            break;
        }

        globus_list_insert(targets, token);
    }

    globus_free(newstr);
    return 0;
}

static int
globus_l_usage_stats_activate()
{
    globus_result_t                     result = GLOBUS_SUCCESS;
    int                                 rc = 0;

    globus_l_usage_stats_stack = NULL;
    globus_l_usage_stats_udp_driver = NULL;

    rc = globus_module_activate(GLOBUS_XIO_MODULE);
    if(rc != GLOBUS_SUCCESS)
    {
        return rc;
    }

    GlobusDebugInit(GLOBUS_USAGE, MESSAGES);

    globus_mutex_init(&globus_l_usage_stats_mutex, NULL);

    if((result = globus_xio_stack_init(
            &globus_l_usage_stats_stack, NULL)) != GLOBUS_SUCCESS)
    {
        return result;
    }

    if((result = globus_xio_driver_load(
            "udp", &globus_l_usage_stats_udp_driver)) != GLOBUS_SUCCESS)
    {    
        return result;
    }

    if((result = globus_xio_stack_push_driver(
            globus_l_usage_stats_stack, globus_l_usage_stats_udp_driver)) 
       != GLOBUS_SUCCESS)
    {
        return result;
    }

    result = globus_xio_handle_create(
        &globus_l_usage_stats_xio_handle,
        globus_l_usage_stats_stack);
    if(result != GLOBUS_SUCCESS)
    {
        return result;
    }

    result = globus_xio_open(
        globus_l_usage_stats_xio_handle,
        NULL, NULL);
    if(result != GLOBUS_SUCCESS)
    {
        return result;
    }

    return 0;
}

static
int
globus_l_usage_stats_deactivate()
{
  
    if(globus_l_usage_stats_xio_handle)
    {
        globus_xio_close(globus_l_usage_stats_xio_handle, NULL);
    }

    if(globus_l_usage_stats_stack)
    {
        globus_xio_stack_destroy(globus_l_usage_stats_stack);
    }
    
    globus_mutex_destroy(&globus_l_usage_stats_mutex);

    return globus_module_deactivate(GLOBUS_XIO_MODULE);
}

globus_result_t
globus_usage_stats_handle_init(
    globus_usage_stats_handle_t *       handle,
    uint16_t                            code,
    uint16_t                            version,
    const char *                        targets)
{
    globus_result_t                     result = GLOBUS_SUCCESS;
    globus_i_usage_stats_handle_t *     new_handle;
    char *                              targets_env;
    globus_list_t *                     targets_list;
    char *                              contact;
    globus_sockaddr_t                   addr;
    int                                 host[16];
    int                                 count;
    size_t                              data_length = 0;
    char                                hostname[255];
    uint16_t                            ncode;
    uint16_t                            nversion;

    new_handle = globus_malloc(sizeof(globus_i_usage_stats_handle_t));
    if(!new_handle)
    {
        return globus_error_put(
            globus_error_construct_error(
                GLOBUS_USAGE_MODULE,
                NULL,
                GLOBUS_USAGE_STATS_ERROR_TYPE_OOM,
                __FILE__,
                _globus_func_name,
                __LINE__,
                "Out of memory"));
    }

    memset(new_handle, 0, sizeof(globus_i_usage_stats_handle_t));

    new_handle->code = htons(code);
    new_handle->version = htons(code);

    memset(new_handle->data, 0, 1472);

    ncode = htons(new_handle->code);
    memcpy(new_handle->data + data_length, 
           (void *)&ncode, 2);
    data_length += 2;

    nversion = htons(new_handle->version);
    memcpy(new_handle->data + data_length, 
           (void *)&nversion, 2);
    data_length += 2;

    globus_libc_gethostaddr(&addr);
    globus_libc_addr_to_contact_string(
        &addr, 0, &contact);
    globus_libc_contact_string_to_ints(
        contact, host, &count, NULL);
    globus_libc_free(contact);

    if(count == 4)
    {
        memset(new_handle->data + data_length, 0, 12);
        data_length += 12;
    }

    memcpy(new_handle->data + data_length, host, count);
    data_length += count;

    /* timestamp will go here */
    data_length += 4;

    if(globus_libc_gethostname(hostname, 255) == 0)
    {
        data_length += sprintf(new_handle->data + data_length,
                               "HOSTNAME=%s", hostname);
    }
    new_handle->header_length = data_length;
    
    if(targets)
    {
        globus_l_usage_stats_split_targets(targets, &new_handle->targets);
    }
    else if((targets_env = globus_libc_getenv("GLOBUS_USAGE_TARGETS")) 
            != NULL)
    {
        globus_l_usage_stats_split_targets(
            targets_env, &new_handle->targets);
    }
    else
    {
        globus_l_usage_stats_split_targets(
            GLOBUS_L_USAGE_STATS_DEFAULT_TARGETS, 
            &new_handle->targets);
    }

    targets_list = new_handle->targets;
    while(targets_list)
    {
        globus_xio_data_descriptor_t *  dd;
        dd = (globus_xio_data_descriptor_t *) globus_malloc(
            sizeof(globus_xio_data_descriptor_t));
            
        result = globus_xio_data_descriptor_init(
            dd,
            globus_l_usage_stats_xio_handle);
        if(result != GLOBUS_SUCCESS)
        {
            return result;
        }

        result = globus_xio_data_descriptor_cntl(
            *dd,
            globus_l_usage_stats_udp_driver,
            GLOBUS_XIO_UDP_SET_CONTACT,
            (char *)globus_list_first(targets_list));
        if(result != GLOBUS_SUCCESS)
        {
            goto exit;
        }
        
        globus_list_insert(&new_handle->xio_desc_list, dd);
        
        targets_list = globus_list_rest(targets_list);
    }

    new_handle->optout = globus_libc_getenv("GLOBUS_USAGE_OPTOUT");
    
    *handle = new_handle;

    return GLOBUS_SUCCESS;

exit:
    return result;
}

void
globus_usage_stats_handle_destroy(
    globus_usage_stats_handle_t         vhandle)
{
    globus_i_usage_stats_handle_t *     handle =
    (globus_i_usage_stats_handle_t *) vhandle;

    if(handle)
    {
        if(handle->targets)
        {
            globus_list_destroy_all(handle->targets, globus_libc_free);
        }
    
        if(handle->xio_desc_list)
        {
            globus_xio_data_descriptor_t *  dd;
            
            while (!globus_list_empty (handle->xio_desc_list)) 
            {
                if((dd = globus_list_remove(
                    &handle->xio_desc_list, handle->xio_desc_list)) != NULL)
                {
                    globus_xio_data_descriptor_destroy(*dd);
                    globus_free(dd);
                }
            }
        }
        
        globus_free(vhandle);
    }
}

globus_result_t
globus_usage_stats_send(
    globus_usage_stats_handle_t         handle,
    int                                 param_count,
    ...)
{
    globus_result_t                     result = GLOBUS_SUCCESS;
    va_list                             ap;

    va_start(ap, param_count);

    result = globus_usage_stats_vsend(
        handle, param_count, ap);
    
    va_end(ap);

    return result;
}   

globus_result_t
globus_usage_stats_vsend(
    globus_usage_stats_handle_t         handle,
    int                                 param_count,
    va_list                             ap)
{
    globus_list_t *                     targets_list;
    globus_result_t                     result = GLOBUS_SUCCESS;
    size_t                              data_length = 0;
    globus_abstime_t                    stamp;
    uint32_t                            nstamp;
    globus_size_t                       written;
    int                                 i = 0;

    if(handle->optout)
    {
        return result;
    }

    globus_mutex_lock(&globus_l_usage_stats_mutex);

    GlobusTimeAbstimeGetCurrent(stamp);
    nstamp = htonl(stamp.tv_sec);
    memcpy(handle->data + GLOBUS_L_USAGE_STATS_TIMESTAMP_OFFSET, 
           (void *)&nstamp, 4);

    data_length = handle->header_length;

    if(param_count > 0)
    {
        memcpy(handle->data + data_length, " ", 1);
        data_length += 1;

        for(i = 0; i < param_count; ++i)
        {
            const char *                key = va_arg(ap, char *);
            const char *                value = va_arg(ap, char *);

            if(index(value, ' '))
            {
                data_length += sprintf(
                    handle->data + data_length,
                    "%s=\"%s\" ", key, value);
            }
            else
            {
                data_length += sprintf(
                    handle->data + data_length,
                    "%s=%s ", key, value);
            }
        }
    }

    targets_list = handle->xio_desc_list;
    while(targets_list)
    {
        GlobusUsageStatsDebugPrintf(
            GLOBUS_L_USAGE_STATS_DEBUG_MESSAGES,
            ("\n==========SENDING USAGE INFO: %s==(length: %d)===\n",
             (char *)globus_list_first(targets_list), data_length));
        GlobusUsageStatsDebugDump(
            GLOBUS_L_USAGE_STATS_DEBUG_MESSAGES,
            handle->data,
            data_length);
        GlobusUsageStatsDebugPrintf(
            GLOBUS_L_USAGE_STATS_DEBUG_MESSAGES,
            ("\n=========================================================\n"));

        result = globus_xio_write(
            globus_l_usage_stats_xio_handle,
            handle->data,
            data_length,
            0,
            &written,
            *(globus_xio_data_descriptor_t *) 
                globus_list_first(targets_list));
        if(result != GLOBUS_SUCCESS)
        {
            goto exit;
        }

        targets_list = globus_list_rest(targets_list);
    }

exit:
    globus_mutex_unlock(&globus_l_usage_stats_mutex);
    return result;
}
