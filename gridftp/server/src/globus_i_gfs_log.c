
#include "globus_i_gridftp_server.h"

/**
 * should select logging based on configuration.  log output funcs should
 * still be usable before this and will output to stderr.
 * 
 * if this fails, just print to stderr.
 */
 

static globus_logging_handle_t          globus_l_gfs_log_handle = NULL;
static globus_usage_stats_handle_t      globus_l_gfs_usage_handle = NULL;
static FILE *                           globus_l_gfs_log_file = NULL;
static FILE *                           globus_l_gfs_transfer_log_file = NULL;

int
globus_l_gfs_log_matchlevel(
    char *                              tag)
{
    int                                 out;
    GlobusGFSName(globus_l_gfs_log_matchlevel);
    GlobusGFSDebugEnter();

    if(strcasecmp(tag, "ERROR") == 0)
    {   
        out = GLOBUS_I_GFS_LOG_ERR;
    }             
    else if(strcasecmp(tag, "WARN") == 0)
    {   
        out = GLOBUS_I_GFS_LOG_WARN;
    }             
    else if(strcasecmp(tag, "INFO") == 0)
    {   
        out = GLOBUS_I_GFS_LOG_INFO;
    }             
    else if(strcasecmp(tag, "DUMP") == 0)
    {   
        out = GLOBUS_I_GFS_LOG_DUMP;
    }             
    else if(strcasecmp(tag, "ALL") == 0)
    {   
        out = GLOBUS_I_GFS_LOG_ALL;
    } 
    
    GlobusGFSDebugExit();
    return out;
}

void
globus_i_gfs_log_open()
{
    char *                              module;
    globus_logging_module_t *           log_mod;
    void *                              log_arg;
    char *                              logfilename;
    int                                 log_filemode;
    char *                              logunique;
    char *                              log_level;
    int                                 log_mask = 0;
    char *                              ptr;
    int                                 len;
    int                                 ctr;
    char *                              tag;
    globus_result_t                     result;
    GlobusGFSName(globus_i_gfs_log_open);
    GlobusGFSDebugEnter();
        
    /* parse user supplied log level string */
    log_level = globus_libc_strdup(globus_i_gfs_config_string("log_level"));
    if(log_level != NULL)
    {
        len = strlen(log_level);
        for(ctr = 0; ctr < len && isdigit(log_level[ctr]); ctr++);
        /* just a number, set log level to the supplied level || every level
            below */
        if(ctr == len)
        {
            log_mask = atoi(log_level);
            if(log_mask > 1)
            {
                log_mask |= (log_mask >> 1) | ((log_mask >> 1)  - 1);
            }
        }
        else
        {
            tag = log_level;
            while((ptr = strchr(tag, ',')) != NULL)
            {
                *ptr = '\0';
                log_mask |= globus_l_gfs_log_matchlevel(tag);
                tag = ptr + 1;
            }
            if(ptr == NULL)
            {
                log_mask |= globus_l_gfs_log_matchlevel(tag);
            }               
        }
        globus_free(log_level);
    }

    /* XXX should use the globus_extension stuff here */
    module = globus_i_gfs_config_string("log_module");
    if(module == NULL || strcmp(module, "stdio") == 0)
    {
        log_mod = &globus_logging_stdio_module;
    }
    else if(strcmp(module, "syslog") == 0)
    {
        log_mod = &globus_logging_syslog_module;
        /* set syslog options and pass in log_arg */
    }
    else
    {
        globus_libc_fprintf(stderr, 
            "Invalid logging module specified, using stdio.\n");
        log_mod = &globus_logging_stdio_module;
    }

    if(log_mod == &globus_logging_stdio_module)
    {          
        logfilename = globus_i_gfs_config_string("log_single");
        if(logfilename == NULL)
        {
            logunique = globus_i_gfs_config_string("log_unique");
            if(logunique != NULL)
            {
                logfilename = globus_common_create_string(
                    "%sgridftp.%d.log", logunique, getpid());
            }
        }
        if(logfilename != NULL)
        {            
            globus_l_gfs_log_file = fopen(logfilename, "a"); 
            if((log_filemode = globus_i_gfs_config_int("log_filemode")) != 0)
            {
                chmod(logfilename, log_filemode);
            }
            globus_free(logfilename);
        }
        if(globus_l_gfs_log_file == NULL)
        {
            globus_l_gfs_log_file = stderr;
        }
        
        log_arg = globus_l_gfs_log_file;
    }
    
    globus_logging_init(
        &globus_l_gfs_log_handle,
        GLOBUS_NULL, /* no buffered logs */
        2048,
        log_mask, 
        log_mod,
        log_arg);
        
    if((logfilename = globus_i_gfs_config_string("log_transfer")) != NULL)
    {
        globus_l_gfs_transfer_log_file = fopen(logfilename, "a"); 
        setvbuf(globus_l_gfs_transfer_log_file, NULL, _IOLBF, 0);
        if((log_filemode = globus_i_gfs_config_int("log_filemode")) != 0)
        {
            chmod(logfilename, log_filemode);
        }
        globus_free(logfilename);
    }

    if(!globus_i_gfs_config_bool("disable_usage_stats"))
    {
       result = globus_usage_stats_handle_init(
            &globus_l_gfs_usage_handle, 0, 0, NULL);      
    }

    GlobusGFSDebugExit();        
}

void
globus_i_gfs_log_close(void)
{
    GlobusGFSName(globus_i_gfs_log_close);
    GlobusGFSDebugEnter();

    globus_logging_flush(globus_l_gfs_log_handle);
    globus_logging_destroy(globus_l_gfs_log_handle);
    if(globus_l_gfs_log_file != stderr && globus_l_gfs_log_file != NULL)
    {
        fclose(globus_l_gfs_log_file);
        globus_l_gfs_log_file = NULL;
    }
    if(globus_l_gfs_transfer_log_file != NULL)
    {
        fclose(globus_l_gfs_transfer_log_file);
        globus_l_gfs_transfer_log_file = NULL;
    }    

    if(globus_l_gfs_usage_handle != NULL)
    {
        globus_usage_stats_handle_destroy(globus_l_gfs_usage_handle);
    }
    GlobusGFSDebugExit();
}

void
globus_gfs_log_message(
    globus_gfs_log_type_t               type,
    const char *                        format,
    ...)
{
    va_list                             ap;
    GlobusGFSName(globus_gfs_log_message);
    GlobusGFSDebugEnter();
    
    va_start(ap, format);
    globus_logging_vwrite(globus_l_gfs_log_handle, type, format, ap);
    va_end(ap);

    GlobusGFSDebugExit();
}

void
globus_gfs_log_result(
    globus_gfs_log_type_t               type,
    const char *                        lead,
    globus_result_t                     result)
{
    char *                              message;
    GlobusGFSName(globus_gfs_log_result);
    GlobusGFSDebugEnter();
    
    if(result != GLOBUS_SUCCESS)
    {
        message = globus_error_print_friendly(globus_error_peek(result));
    }
    else
    {
        message = globus_libc_strdup("(unknown error)");
    }
    globus_i_gfs_log_message(type, "%s:\n%s\n", lead, message);
    globus_free(message);

    GlobusGFSDebugExit();
}


void
globus_i_gfs_log_result_warn(
    const char *                        lead,
    globus_result_t                     result)
{
    char *                              message;
    GlobusGFSName(globus_i_gfs_log_result_warn);
    GlobusGFSDebugEnter();
    
    if(result != GLOBUS_SUCCESS)
    {
        message = globus_error_print_friendly(globus_error_peek(result));
    }
    else
    {
        message = globus_libc_strdup("(unknown error)");
    }
    globus_i_gfs_log_message(GLOBUS_I_GFS_LOG_WARN, "%s:\n%s\n", lead, message);
    globus_free(message);

    GlobusGFSDebugExit();
}

void
globus_i_gfs_log_result(
    const char *                        lead,
    globus_result_t                     result)
{
    char *                              message;
    GlobusGFSName(globus_i_gfs_log_result);
    GlobusGFSDebugEnter();
    
    if(result != GLOBUS_SUCCESS)
    {
        message = globus_error_print_friendly(globus_error_peek(result));
    }
    else
    {
        message = globus_libc_strdup("(unknown error)");
    }
    globus_i_gfs_log_message(GLOBUS_I_GFS_LOG_ERR, "%s:\n%s\n", lead, message);
    globus_free(message);

    GlobusGFSDebugExit();
}

void
globus_i_gfs_log_transfer(
    int                                 stripe_count,
    int                                 stream_count, 
    struct timeval *                    start_gtd_time,
    struct timeval *                    end_gtd_time,
    char *                              dest_ip,
    globus_size_t                       blksize,
    globus_size_t                       tcp_bs,
    const char *                        fname,
    globus_off_t                        nbytes,
    int                                 code,
    char *                              volume,
    char *                              type,
    char *                              username)
{
    time_t                              start_time_time;
    time_t                              end_time_time;
    struct tm *                         tmp_tm_time;
    struct tm                           start_tm_time;
    struct tm                           end_tm_time;
    char                                out_buf[4096];
    long                                win_size;
    GlobusGFSName(globus_i_gfs_log_transfer);
    GlobusGFSDebugEnter();

    if(globus_l_gfs_transfer_log_file == NULL)
    {
        goto err;
    }

    start_time_time = (time_t)start_gtd_time->tv_sec;
    tmp_tm_time = gmtime(&start_time_time);
    if(tmp_tm_time == NULL)
    {
        goto err;
    }
    start_tm_time = *tmp_tm_time;

    end_time_time = (time_t)end_gtd_time->tv_sec;
    tmp_tm_time = gmtime(&end_time_time);
    if(tmp_tm_time == NULL)
    {
        goto err;
    }
    end_tm_time = *tmp_tm_time;

    if(tcp_bs == 0)
    {
        win_size = 0;
/*      int                             sock;
        int                             opt_len;
        int                             opt_dir;

        if(strcmp(type, "RETR") == 0 || strcmp(type, "ERET") == 0)
        {
            opt_dir = SO_SNDBUF;
            sock = STDOUT_FILENO;
        }
        else
        {
            opt_dir = SO_RCVBUF;
            sock = STDIN_FILENO;
        }
        opt_len = sizeof(win_size);
        getsockopt(sock, SOL_SOCKET, opt_dir, &win_size, &opt_len);
*/
    }
    else
    {
        win_size = tcp_bs;
    }

    sprintf(out_buf, 
        "DATE=%04d%02d%02d%02d%02d%02d.%d "
        "HOST=%s "
        "PROG=%s "
        "NL.EVNT=FTP_INFO "
        "START=%04d%02d%02d%02d%02d%02d.%d "
        "USER=%s "
        "FILE=%s "
        "BUFFER=%ld "
        "BLOCK=%ld "
        "NBYTES=%"GLOBUS_OFF_T_FORMAT" "
        "VOLUME=%s "
        "STREAMS=%d "
        "STRIPES=%d "
        "DEST=[%s] " 
        "TYPE=%s " 
        "CODE=%d\n",
        /* end time */
        end_tm_time.tm_year + 1900,
        end_tm_time.tm_mon + 1,
        end_tm_time.tm_mday,
        end_tm_time.tm_hour,
        end_tm_time.tm_min,
        end_tm_time.tm_sec,
        (int) end_gtd_time->tv_usec,
        globus_i_gfs_config_string("fqdn"),
        "globus-gridftp-server",
        /* start time */
        start_tm_time.tm_year + 1900,
        start_tm_time.tm_mon + 1,
        start_tm_time.tm_mday,
        start_tm_time.tm_hour,
        start_tm_time.tm_min,
        start_tm_time.tm_sec,
        (int) start_gtd_time->tv_usec,
        /* other args */
        username,
        fname,
        win_size,
        (long) blksize,
        nbytes,
        volume,
        stream_count, 
        stripe_count,
        dest_ip,
        type, 
        code);
        
    fwrite(out_buf, 1, strlen(out_buf), globus_l_gfs_transfer_log_file);

    GlobusGFSDebugExit();
    return;
    
err:
    GlobusGFSDebugExitWithError();
}


void
globus_i_gfs_log_usage_stats(
    int                                 stripe_count,
    int                                 stream_count, 
    struct timeval *                    start_gtd_time,
    struct timeval *                    end_gtd_time,
    char *                              dest_ip,
    globus_size_t                       blksize,
    globus_size_t                       tcp_bs,
    const char *                        fname,
    globus_off_t                        nbytes,
    int                                 code,
    char *                              volume,
    char *                              type,
    char *                              username)
{
    time_t                              start_time_time;
    time_t                              end_time_time;
    struct tm *                         tmp_tm_time;
    struct tm                           start_tm_time;
    struct tm                           end_tm_time;
    long                                win_size;
    char                                start_b[256];
    char                                end_b[256];
    char                                ver_b[256];
    char                                block_b[256]; 
    char                                buffer_b[256];
    char                                nbytes_b[256];
    char                                streams_b[256];
    char                                stripes_b[256];
    char                                code_b[256];
    globus_result_t                     result;
    GlobusGFSName(globus_i_gfs_log_usage_stats);
    GlobusGFSDebugEnter();

    if(globus_l_gfs_usage_handle == NULL)
    {
        goto err;
    }

    start_time_time = (time_t)start_gtd_time->tv_sec;
    tmp_tm_time = gmtime(&start_time_time);
    if(tmp_tm_time == NULL)
    {
        goto err;
    }
    start_tm_time = *tmp_tm_time;

    end_time_time = (time_t)end_gtd_time->tv_sec;
    tmp_tm_time = gmtime(&end_time_time);
    if(tmp_tm_time == NULL)
    {
        goto err;
    }
    end_tm_time = *tmp_tm_time;

    if(tcp_bs == 0)
    {
        win_size = 0;
/*      int                             sock;
        int                             opt_len;
        int                             opt_dir;

        if(strcmp(type, "RETR") == 0 || strcmp(type, "ERET") == 0)
        {
            opt_dir = SO_SNDBUF;
            sock = STDOUT_FILENO;
        }
        else
        {
            opt_dir = SO_RCVBUF;
            sock = STDIN_FILENO;
        }
        opt_len = sizeof(win_size);
        getsockopt(sock, SOL_SOCKET, opt_dir, &win_size, &opt_len);
*/
    }
    else
    {
        win_size = tcp_bs;
    }
    
    sprintf(start_b, "%04d%02d%02d%02d%02d%02d.%d", 
        start_tm_time.tm_year + 1900,
        start_tm_time.tm_mon + 1,
        start_tm_time.tm_mday,
        start_tm_time.tm_hour,
        start_tm_time.tm_min,
        start_tm_time.tm_sec,
        (int) start_gtd_time->tv_usec);
    sprintf(end_b, "%04d%02d%02d%02d%02d%02d.%d", 
        end_tm_time.tm_year + 1900,
        end_tm_time.tm_mon + 1,
        end_tm_time.tm_mday,
        end_tm_time.tm_hour,
        end_tm_time.tm_min,
        end_tm_time.tm_sec,
        (int) end_gtd_time->tv_usec);
    sprintf(ver_b, "%s", globus_i_gfs_config_string("version_string"));
    sprintf(buffer_b, "%ld", win_size);
    sprintf(block_b, "%ld",(long) blksize);
    sprintf(nbytes_b, "%"GLOBUS_OFF_T_FORMAT, nbytes);
    sprintf(streams_b, "%d", stream_count);
    sprintf(stripes_b, "%d", stripe_count);
    sprintf(code_b, "%d", code);

    result = globus_usage_stats_send(
        globus_l_gfs_usage_handle,
        10,
        /* include an end time, incase we don't actually send this til well
        after transfer finished? */
        "START", start_b,
        "END", end_b,
        "VER", ver_b,
        "BUFFER", buffer_b,
        "BLOCK", block_b,
        "NBYTES", nbytes_b,
        "STREAMS", streams_b,
        "STRIPES", stripes_b,
        "TYPE", type,
        "CODE", code_b);
    
    if(result != GLOBUS_SUCCESS)
    {
        goto err;
    }
    
    GlobusGFSDebugExit();
    return;
    
err:
    GlobusGFSDebugExitWithError();
}
