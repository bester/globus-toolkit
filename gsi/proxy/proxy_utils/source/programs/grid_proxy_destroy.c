#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file grid_proxy_destroy.h
 * Globus GSI Proxy Utils
 * @author Sam Lang, Sam Meder
 *
 * $RCSfile$
 * $Revision$
 * $Date$
 */
#endif

#include "globus_common.h"
#include "globus_error.h"
#include "globus_gsi_cert_utils.h"
#include "globus_gsi_system_config.h"
#include "globus_gsi_proxy.h"
#include "globus_gsi_credential.h"
#include "globus_gsi_system_config.h"

int                                     debug = 0;

#define SHORT_USAGE_FORMAT \
"\nSyntax: %s [-help][-dryrun][-default][-all][--] [file1...]\n"

static char *  LONG_USAGE = \
"\n" \
"    Options\n" \
"    -help, -usage             Displays usage\n" \
"    -version                  Displays version\n" \
"    -dryrun                   Prints what files would have been destroyed\n" \
"    -default                  Destroys file at default proxy location\n" \
"    -all                      Destroys any delegated proxy as well\n" \
"    --                        End processing of options\n" \
"    file1 file2 ...           Destroys files listed\n" \
"\n";


#   define args_show_version() \
    { \
	char buf[64]; \
	sprintf( buf, \
		 "%s-%s", \
		 PACKAGE, \
		 VERSION); \
	fprintf(stderr, "%s", buf); \
	exit(0); \
    }

#   define args_show_short_help() \
    { \
        fprintf(stderr, \
		SHORT_USAGE_FORMAT \
		"\nOption -help will display usage.\n", \
		program); \
	exit(0); \
    }

#   define args_show_full_usage() \
    { \
	fprintf(stderr, SHORT_USAGE_FORMAT \
		"%s", \
		program, \
		LONG_USAGE); \
	exit(0); \
    }

#   define args_error_message(errmsg) \
    { \
	fprintf(stderr, "ERROR: %s\n", errmsg); \
        args_show_short_help(); \
	exit(1); \
    }

#   define args_error(argnum, argval, errmsg) \
    { \
	char buf[1024]; \
	sprintf(buf, "argument #%d (%s) : %s", argnum, argval, errmsg); \
	args_error_message(buf); \
    }

void
globus_i_gsi_proxy_utils_print_error(
    globus_result_t                     result,
    int                                 debug,
    const char *                        filename,
    int                                 line);

#define GLOBUS_I_GSI_PROXY_UTILS_PRINT_ERROR \
    globus_i_gsi_proxy_utils_print_error(result, debug, __FILE__, __LINE__)

static int
globus_i_gsi_proxy_utils_clear_and_remove(
    char *                              filename,
    int                                 flag); 

int main(
    int                                 argc, 
    char **                             argv)
{
    int                                 all_flag      = 0;
    int                                 default_flag  = 0;
    int                                 dryrun_flag   = 0;
    int                                 i;
    char *                              argp;
    char *                              program;
    char *                              default_file;
    char *                              default_full_file;
    char *                              dummy_dir_string;
    globus_result_t                     result = GLOBUS_SUCCESS;

    if(globus_module_activate(GLOBUS_GSI_PROXY_MODULE) != (int)GLOBUS_SUCCESS)
    {
        globus_libc_fprintf(
            stderr,
            "\n\nERROR: Couldn't load module: GLOBUS_GSI_PROXY_MODULE.\n"
            "Make sure Globus is installed correctly.\n\n");
        exit(1);
    }

    if (strrchr(argv[0],'/'))
    {
	program = strrchr(argv[0],'/') + 1;
    }
    else
    {
	program = argv[0];
    }

    for (i = 1; i < argc; i++)
    {
	argp = argv[i];

	/* '--' indicates end of options */
	if (strcmp(argp,"--") == 0)
	{
	    i++;
	    break;
	}

	/* If no leading dash assume it's start of filenames */
	if (strncmp(argp, "-", 1) != 0)
	{
	    break;
	}

	if (strcmp(argp, "-all") == 0)
        {
	    all_flag++;
        }
	if (strcmp(argp, "-default") == 0)
        {
	    default_flag++;
        }
	else if (strcmp(argp, "-dryrun") == 0)
        {
	    dryrun_flag++;
        }
	else if (strncmp(argp, "--", 2) == 0)
	{
	    args_error(i, argp, "double-dashed options not allowed");
	}
	else if((strcmp(argp, "-help") == 0) ||
                (strcmp(argp, "-usage") == 0) )
	{
	    args_show_full_usage();
	}
	else if (strcmp(argp, "-version") == 0)
	{
	    args_show_version();
	}
        else if (strcmp(argp, "-debug") == 0)
        {
            debug = 1;
        }            
	else 
	{
	    args_error(i, argp, "unknown option");
	}
    }

    /* remove the files listed on the command line first */
    for (; i < argc; i++)
    {
	globus_i_gsi_proxy_utils_clear_and_remove(argv[i], dryrun_flag);
    }

    if (default_flag)
    {
	globus_i_gsi_proxy_utils_clear_and_remove(default_full_file, 
                                                  dryrun_flag);
    }

    result = GLOBUS_GSI_SYSCONFIG_GET_PROXY_FILENAME(&default_full_file,
                                                     GLOBUS_PROXY_FILE_INPUT);
    if(result != GLOBUS_SUCCESS)
    {
        globus_libc_fprintf(
            stderr,
            "Proxy file doesn't exist or has bad permissions", 
            default_full_file);
        GLOBUS_I_GSI_PROXY_UTILS_PRINT_ERROR;
    }

    if (all_flag)
    {
        result = GLOBUS_GSI_SYSCONFIG_REMOVE_ALL_OWNED_FILES(
            default_file);
        if(result != GLOBUS_SUCCESS)
        {
            globus_libc_fprintf(
                stderr,
                "Couldn't remove the all the files "
                "owned by you in secure tmp directory.");
            GLOBUS_I_GSI_PROXY_UTILS_PRINT_ERROR;
        }
    }
	
    /* 
     * no options, remove the default file, which is the ENV
     * or the /tmp/x509up_u<uid> file
     */

    GLOBUS_GSI_SYSCONFIG_SPLIT_DIR_AND_FILENAME(default_full_file,
                                                &dummy_dir_string,
                                                &default_file);

    if (!default_flag && !all_flag)
    {
        globus_i_gsi_proxy_utils_clear_and_remove(default_full_file,
                                                  dryrun_flag);
    }

    return 0;
}


static int
globus_i_gsi_proxy_utils_clear_and_remove(
    char *                              filename,
    int                                 flag) 
{
    int                                 f;
    int                                 rec;
    int                                 left;
    long                                size;
    char                                msg[65] 
        = "Destroyed by globus_proxy_destroy\r\n";

    if (flag)
	fprintf(stderr, "Would remove %s\n", filename);
    else
    {
	f = open(filename, O_RDWR);
	if (f) 
	{
	    size = lseek(f, 0L, SEEK_END);
	    lseek(f, 0L, SEEK_SET);
	    if (size > 0) 
	    {
		rec = size / 64;
		left = size - rec * 64;
		while (rec)
		{
		    write(f, msg, 64);
		    rec--;
		}
		if (left) 
		    write(f, msg, left);
	    }
	    close(f);
	}
	remove(filename);
    }
    return 0;
}

void
globus_i_gsi_proxy_utils_print_error(
    globus_result_t                     result,
    int                                 debug,
    const char *                        filename,
    int                                 line)
{
    globus_object_t *                   error_obj;
    globus_object_t *                   base_error_obj;

    error_obj = globus_error_get(result);
    if(debug)
    {
        char *                          error_string = NULL;
        globus_libc_fprintf(stderr,
                            "\n\n%s:%d:",
                            filename, line);
        error_string = globus_error_print_chain(error_obj);
        globus_libc_fprintf(stderr, "%s\n", error_string);
        base_error_obj = error_obj;
        while(1)
        {
            if(!globus_error_get_cause(base_error_obj) || 
               globus_object_get_type(
                   globus_error_get_cause(base_error_obj)) 
               == GLOBUS_ERROR_TYPE_OPENSSL)
            {
                break;
            }
            base_error_obj = globus_error_get_cause(base_error_obj);
        }
        globus_libc_fprintf(stderr, "\nBASE CAUSE: %s\n\n", 
                            globus_error_get_long_desc(base_error_obj)); 
    }
    else 
    {
        globus_libc_fprintf(stderr,
                            "\nUse -debug for further information.\n\n");
    }
    globus_object_free(error_obj);
    exit(1);
}
