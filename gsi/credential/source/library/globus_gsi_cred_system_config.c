#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file globus_gsi_cred_system_config.c
 * @author Sam Lang, Sam Meder
 *
 * $RCSfile$
 * $Revision$
 * $Date$
 */
#endif

#include "globus_gsi_cred_system_config.h"
#include "globus_i_gsi_credential.h"
#include <openssl/rand.h>
#include <errno.h>
#include "globus_common.h"

#ifndef DEFAULT_SECURE_TMP_DIR
#ifndef WIN32
#define DEFAULT_SECURE_TMP_DIR "/tmp"
#else
#define DEFAULT_SECURE_TMP_DIR "c:\\tmp"
#endif
#endif

#ifdef WIN32
#include "winglue.h"
#include <io.h>
#else
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <dirent.h>
#endif

#define X509_CERT_DIR                   "X509_CERT_DIR"
#define X509_CERT_FILE                  "X509_CERT_FILE"
#define X509_USER_PROXY                 "X509_USER_PROXY"
#define X509_USER_CERT                  "X509_USER_CERT"
#define X509_USER_KEY                   "X509_USER_KEY"
#define X509_USER_DELEG_FILE            "x509up_p"
#define X509_USER_PROXY_FILE            "x509up_u"

/* This is added after the CA name hash to make the policy filename */
#define SIGNING_POLICY_FILE_EXTENSION   ".signing_policy"

#ifdef WIN32
#define FILE_SEPERATOR "\\"
#define GSI_REGISTRY_DIR                "software\\Globus\\GSI"
#define X509_DEFAULT_USER_CERT          ".globus\\usercert.pem"
#define X509_DEFAULT_USER_KEY           ".globus\\userkey.pem"
#define X509_DEFAULT_PKCS12_FILE        ".globus\\usercred.p12"
#define X509_DEFAULT_TRUSTED_CERT_DIR   "SLANG: NEEDS TO BE DETERMINED"
#define X509_INSTALLED_TRUSTED_CERT_DIR "SLANG: NEEDS TO BE DETERMINED"
#define X509_LOCAL_TRUSTED_CERT_DIR     ".globus\\certificates"
#define X509_DEFAULT_CERT_DIR           "SLANG: NEEDS TO BE DETERMINED"
#define X509_INSTALLED_CERT_DIR         "etc"
#define X509_LOCAL_CERT_DIR             ".globus"
#else
#define FILE_SEPERATOR                  "/"
#define X509_DEFAULT_USER_CERT          ".globus/usercert.pem"
#define X509_DEFAULT_USER_KEY           ".globus/userkey.pem"
#define X509_DEFAULT_PKCS12_FILE        ".globus/usercred.p12"
#define X509_DEFAULT_TRUSTED_CERT_DIR   "/etc/grid-security/certificates"
#define X509_INSTALLED_TRUSTED_CERT_DIR "share/certificates"
#define X509_LOCAL_TRUSTED_CERT_DIR     ".globus/certificates"
#define X509_DEFAULT_CERT_DIR           "/etc/grid-security"
#define X509_INSTALLED_CERT_DIR         "etc"
#define X509_LOCAL_CERT_DIR             ".globus"
#endif

#define X509_HOST_PREFIX                "host"
#define X509_CERT_SUFFIX                "cert.pem"
#define X509_KEY_SUFFIX                 "key.pem"

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL

#define GLOBUS_GSI_SYSTEM_CONFIG_MALLOC_ERROR \
    globus_error_put(globus_error_wrap_errno_error( \
        GLOBUS_GSI_CREDENTIAL_MODULE, \
        errno, \
        GLOBUS_GSI_CRED_ERROR_SYSTEM_CONFIG, \
        "%s:%d: Could not allocate enough memory: %d bytes", \
        __FILE__, __LINE__, len))

globus_result_t
globus_i_gsi_cred_create_cert_dir_path(
    char **                             cert_dir,
    char **                             cert_dir_value,
    const char *                        format,
    ...)
{
    va_list                             ap;
    FILE *                              null_file_stream;
    int                                 len;
    globus_gsi_statcheck_t              status;

    static char *                       _function_name_ =
        "globus_i_gsi_cred_create_cert_dir_path";

    *cert_dir = NULL;

    va_start(ap, format);

    null_file_stream = fopen("/dev/null", "w");
    len = vfprintf(null_file_stream, format ? format : "<null value>", ap) + 1;
    fclose(null_file_stream);
    if(len < 1)
    {
        return GLOBUS_GSI_CRED_ERROR_RESULT(
            GLOBUS_GSI_CRED_ERROR_SYSTEM_CONFIG);
    }

    *cert_dir_value = globus_malloc(len);
    if(!(*cert_dir_value))
    {
        return GLOBUS_GSI_SYSTEM_CONFIG_MALLOC_ERROR;
    }

    vsnprintf(*cert_dir_value, len, format ? format : "<null value>", ap);
    
    if(format && 
       GLOBUS_I_GSI_FILE_EXISTS(*cert_dir_value, & status) == GLOBUS_SUCCESS)
    {
        *cert_dir = *cert_dir_value;
    }

    va_end(ap);

    return GLOBUS_SUCCESS;
}
    

globus_result_t
globus_i_gsi_cred_create_cert_string(
    char **                             cert_string,
    char **                             cert_string_value,
    const char *                        format,
    ...)
{
    va_list                             ap;
    FILE *                              null_file_stream;
    int                                 len;
    globus_gsi_statcheck_t              status;

    static char *                       _function_name_ =
        "globus_i_gsi_cred_create_cert_string";

    *cert_string = NULL;

    va_start(ap, format);

    null_file_stream = fopen("/dev/null", "w");
    len = vfprintf(null_file_stream, format ? format : "<null value>", ap) + 1;
    fclose(null_file_stream);

    if(len < 1)
    {
        return GLOBUS_GSI_CRED_ERROR_RESULT(
            GLOBUS_GSI_CRED_ERROR_SYSTEM_CONFIG);
    }

    *cert_string_value = globus_malloc(len);
    if(!(*cert_string_value))
    {
        return GLOBUS_GSI_SYSTEM_CONFIG_MALLOC_ERROR;
    }

    vsnprintf(*cert_string_value, len, format ? format : "<null value>", ap);

    if(format && 
       GLOBUS_I_GSI_CHECK_CERTFILE(*cert_string_value, & status) 
       == GLOBUS_SUCCESS &&
       status == GLOBUS_VALID)
    {
        *cert_string = *cert_string_value;
    }

    va_end(ap);

    return GLOBUS_SUCCESS;
}

globus_result_t
globus_i_gsi_cred_create_key_string(
    char **                             key_string,
    char **                             key_string_value,
    const char *                        format,
    ...)
{
    va_list                             ap;
    FILE *                              null_file_stream;
    int                                 len;
    globus_gsi_statcheck_t              status;

    static char *                       _function_name_ =
        "globus_i_gsi_cred_create_key_string";

    *key_string = NULL;

    va_start(ap, format);

    null_file_stream = fopen("/dev/null", "w");
    len = vfprintf(null_file_stream, format ? format : "<null value>", ap) + 1;
    fclose(null_file_stream);

    if(len < 1)
    {
        return GLOBUS_GSI_CRED_ERROR_RESULT(
            GLOBUS_GSI_CRED_ERROR_SYSTEM_CONFIG);
    }

    *key_string_value = globus_malloc(len);
    if(!(*key_string_value))
    {
        return GLOBUS_GSI_SYSTEM_CONFIG_MALLOC_ERROR;
    }

    vsnprintf(*key_string_value, len, format ? format : "<null value>", ap);

    if(format && 
       GLOBUS_I_GSI_CHECK_KEYFILE(*key_string_value, & status) 
       == GLOBUS_SUCCESS &&
       status == GLOBUS_VALID)
    {
        *key_string = *key_string_value;
    }

    va_end(ap);

    return GLOBUS_SUCCESS;
}

#endif /* GLOBUS_DONT_DOCUMENT_INTERNAL */

#ifdef WIN32  /* define all the *_win32 functions */

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL

/**
 * WIN32 - Get HOME Directory
 * @ingroup globus_i_gsi_cred_system_config_win32
 */
/* @{ */
/**
 * Get the HOME directory, currently c:\windows
 * 
 * @param home_dir
 *        The home directory of the current user
 * @return
 *        GLOBUS_SUCCESS if no error occured, otherwise
 *        an error object is returned.
 */
globus_result_t
globus_i_gsi_get_home_dir_win32(
    char **                             home_dir)
{
    const char *                        _function_name_ =
        "globus_i_gsi_get_home_dir_win32";

    *home_dir = "c:\\windows";

    if((*home_dir) == NULL)
    {
        return GLOBUS_GSI_CRED_ERROR_RESULT(
            GLOBUS_GSI_CRED_ERROR_SYSTEM_CONFIG);
    }
    return GLOBUS_SUCCESS;
}
/* @} */

/**
 * WIN32 - File Exists
 * @ingroup globus_gsi_cred_system_config_win32
 */
/* @{ */
/**
 * Check that the file exists
 *
 * @param filename the file to check
 * @param status   the status of the file
 *
 * @return 
 *        GLOBUS_SUCCESS (even if the file doesn't exist) - in some
 *        abortive cases an error object identifier is returned
 */
globus_result_t
globus_i_gsi_file_exists_win32(
    const char *                        filename,
    globus_gsi_statcheck_t *            status)
{
    struct stat                         stx;

    static char *                       _function_name_ =
        "globus_i_gsi_file_exists_win32";

    if (stat(filename,&stx) == -1)
    {
        switch (errno)
        {
        case ENOENT:
        case ENOTDIR:
            *status = GLOBUS_DOES_NOT_EXIST;
            return GLOBUS_SUCCESS;

        case EACCES:

            *status = GLOBUS_BAD_PERMISSIONS;
            return GLOBUS_SUCCESS;

        default:
            return globus_error_put(
                globus_error_wrap_errno_error(
                    GLOBUS_GSI_CREDENTIAL_MODULE,
                    errno,
                    GLOBUS_GSI_CRED_ERROR_SYSTEM_CONFIG,
                    __FILE__":__LINE__:%s: Error getting status of keyfile\n",
                    _function_name_));
        }
    }

    /*
     * use any stat output as random data, as it will 
     * have file sizes, and last use times in it. 
     */
    RAND_add((void*)&stx,sizeof(stx),2);

    if (stx.st_size == 0)
    {
        *status = GLOBUS_ZERO_LENGTH;
        return GLOBUS_SUCCESS;
    }

    *status = GLOBUS_VALID;
    return GLOBUS_SUCCESS;
}    
/* @} */


/**
 * WIN32 - Check File Status for Key
 * @ingroup globus_i_gsi_cred_system_config_win32
 */
/* @{ */
/**
 * This is a convenience function used to check the status of a 
 * private key file.  The desired status is only the current user has
 * ownership and read permissions, everyone else should not be able
 * to access it.
 * 
 * @param filename
 *        The name of the file to check the status of
 * @param status
 *        The status of the file being checked
 *        see @ref globus_gsi_statcheck_t for possible values
 *        of this variable 
 *
 * @return 
 *        GLOBUS_SUCCESS if the status of the file was able
 *        to be determined.  Otherwise, an error object
 *        identifier
 *
 * @see globus_gsi_statcheck_t
 */
globus_result_t
globus_i_gsi_check_keyfile_win32(
    const char *                        filename,
    globus_gsi_statcheck_t *            status)
{
    struct stat                         stx;

    static char *                       _function_name_ =
        "globus_i_gsi_check_keyfile_win32";

    if (stat(filename,&stx) == -1)
    {
        switch (errno)
        {
        case ENOENT:
        case ENOTDIR:
            *status = GLOBUS_DOES_NOT_EXIST;
            return GLOBUS_SUCCESS;

        case EACCES:

            *status = GLOBUS_BAD_PERMISSIONS;
            return GLOBUS_SUCCESS;

        default:
            return globus_error_put(
                globus_error_wrap_errno_error(
                    GLOBUS_GSI_CREDENTIAL_MODULE,
                    errno,
                    GLOBUS_GSI_CRED_ERROR_SYSTEM_CONFIG,
                    __FILE__":__LINE__:%s: Error getting status of keyfile\n",
                    _function_name_));
        }
    }

    /*
     * use any stat output as random data, as it will 
     * have file sizes, and last use times in it. 
     */
    RAND_add((void*)&stx,sizeof(stx),2);

    if (stx.st_size == 0)
    {
        *status = GLOBUS_ZERO_LENGTH;
        return GLOBUS_SUCCESS;
    }

    *status = GLOBUS_VALID;
    return GLOBUS_SUCCESS;
}
/* @} */

/**
 * WIN32 - Check File Status for Cert
 * @ingroup globus_i_gsi_cred_system_config_win32
 */
/* @{ */
/**
 * This is a convenience function used to check the status of a 
 * certificate file.  The desired status is the current user has
 * ownership and read/write permissions, while group and others only
 * have read permissions.
 * 
 * @param filename
 *        The name of the file to check the status of
 * @param status
 *        The status of the file being checked
 *        see @ref globus_gsi_statcheck_t for possible values
 *        of this variable 
 *
 * @return 
 *        GLOBUS_SUCCESS if the status of the file was able
 *        to be determined.  Otherwise, an error object
 *        identifier
 *
 * @see globus_gsi_statcheck_t
 */
globus_result_t
globus_i_gsi_check_certfile_win32(
    const char *                        filename,
    globus_gsi_statcheck_t *            status)
{
    struct stat                         stx;

    static char *                       _function_name_ =
        "globus_i_gsi_check_certfile_win32";

    if (stat(filename,&stx) == -1)
    {
        switch (errno)
        {
        case ENOENT:
        case ENOTDIR:
            *status = GLOBUS_DOES_NOT_EXIST;
            return GLOBUS_SUCCESS;

        case EACCES:

            *status = GLOBUS_BAD_PERMISSIONS;
            return GLOBUS_SUCCESS;

        default:
            return globus_error_put(
                globus_error_wrap_errno_error(
                    GLOBUS_GSI_CREDENTIAL_MODULE,
                    errno,
                    GLOBUS_GSI_CRED_ERROR_SYSTEM_CONFIG,
                    __FILE__":__LINE__:%s: Error getting status of keyfile\n",
                    _function_name_));
        }
    }

    /*
     * use any stat output as random data, as it will 
     * have file sizes, and last use times in it. 
     */
    RAND_add((void*)&stx,sizeof(stx),2);

    if (stx.st_size == 0)
    {
        *status = GLOBUS_ZERO_LENGTH;
        return GLOBUS_SUCCESS;
    }

    *status = GLOBUS_VALID;
    return GLOBUS_SUCCESS;
}
/* @} */

/**
 * WIN32 - Get User ID
 * @ingroup globus_i_gsi_cred_system_config_win32
 */
/* @{ */
/**
 * Get a unique string representing the current user.  On Unix, this is just
 * the uid converted to a string.  On Windows, SLANG: NOT DETERMINED
 */
globus_result_t
globus_i_gsi_get_user_id_string_win32(
    char **                             user_id_string)
{
    int                                 uid;

    /* SLANG: need to set the string to the username or whatever */
    return GLOBUS_SUCCESS;
}
/* @} */

#endif

/**
 * WIN32 - Get Trusted CA Cert Dir
 * @ingroup globus_gsi_cred_system_config_win32
 */
/* @{ */
/**
 * Get the Trusted Certificate Directory containing the trusted
 * Certificate Authority certificates.  This directory is determined
 * in the order shown below.  Failure in one method results in attempting
 * the next.
 *
 * <ol>
 * <li> <b>X509_CERT_DIR environment variable</b> - if this is set, the
 * trusted certificates will be searched for in that directory.  This
 * variable allows the end user to specify the location of trusted
 * certificates.
 * <li> <b>"x509_cert_dir" registry key</b> - If
 * this registry key is set on windows, the directory it points to should
 * contain the trusted certificates.  The path to the registry key is
 * software\Globus\GSI
 * <li> <b>\<user home directory\>\.globus\certificates</b> - If this
 * directory exists, and the previous methods of determining the trusted
 * certs directory failed, this directory will be used.  
 * <li> <b>Host Trusted Cert Dir</b> - This location is intended
 * to be independant of the globus installation ($GLOBUS_LOCATION), and 
 * is generally only writeable by the host system administrator.  
 * SLANG: This value is not currently set for WINDOWS
 * <li> <b>Globus Install Trusted Cert Dir</b> - this
 * is $GLOBUS_LOCATION\share\certificates.  
 * </ol>
 *
 * @param cert_dir
 *        The trusted certificates directory
 * @return
 *        GLOBUS_SUCCESS if no error occurred, and a sufficient trusted
 *        certificates directory was found.  Otherwise, an error object 
 *        identifier returned.
 */
globus_result_t
globus_gsi_cred_get_cert_dir_win32(
    char **                             cert_dir)
{
    char *                              env_cert_dir = NULL;
    char *                              val_cert_dir[512];
    char *                              reg_cert_dir = NULL;
    char *                              local_cert_dir = NULL;
    char *                              default_cert_dir = NULL;
    char *                              installed_cert_dir = NULL;
    int                                 len;    
    HKEY                                hkDir = NULL;
    globus_result_t                     result;
    char *                              home;
    char *                              globus_location;

    *cert_dir = NULL;

    if((result = globus_i_gsi_cred_create_cert_dir_path(
                     cert_dir, 
                     & env_cert_dir,
                     getenv(X509_CERT_DIR))) != GLOBUS_SUCCESS)
    {
        goto error_exit;
    }

    if (!(*cert_dir))
    {
        RegOpenKey(HKEY_CURRENT_USER,GSI_REGISTRY_DIR,&hkDir);
        lval = sizeof(val_cert_dir)-1;
        if (hkDir && (RegQueryValueEx(hkDir,"x509_cert_dir",0,&type,
                                      val_cert_dir,&lval) == ERROR_SUCCESS))
        {
            if((result = globus_i_gsi_cred_create_cert_dir_path(
                             cert_dir, 
                             & reg_cert_dir,
                             val_cert_dir)) != GLOBUS_SUCCESS)
            {
                goto error_exit;
            }
        }
        RegCloseKey(hkDir);
    }

    /* now check for a trusted CA directory in the user's home directory */
    if(!(*cert_dir))
    {
        if((result = globus_i_gsi_get_home_dir(&home)) != GLOBUS_SUCCESS)
        {
            goto error_exit;
        }
            
        if (home) 
        {
            if((result = globus_i_gsi_cred_create_cert_dir_path(
                             cert_dir, 
                             & local_cert_dir,
                             "%s%s%s",
                             home,
                             FILE_SEPERATOR,
                             X509_LOCAL_TRUSTED_CERT_DIR)) != GLOBUS_SUCCESS)
            {
                goto error_exit;
            }
        }
    }

    /* now look in $GLOBUS_LOCATION/share/certificates */
    if (!(*cert_dir))
    {
        if((result = globus_i_gsi_cred_create_cert_dir_path(
                         cert_dir,
                         & installed_cert_dir,
                         X509_INSTALLED_TRUSTED_CERT_DIR)) != GLOBUS_SUCCESS)
        {
            goto error_exit;
        }
    }

    /* now check for host based default directory */
    if (!(*cert_dir))
    {
        globus_location = getenv("GLOBUS_LOCATION");
        
        if (globus_location)
        {
            if((result = globus_i_gsi_cred_create_cert_dir_path(
                             cert_dir,
                             & default_cert_dir,
                             "%s%s%s",
                             globus_location,
                             FILE_SEPERATOR,
                             X509_DEFAULT_TRUSTED_CERT_DIR)) != GLOBUS_SUCCESS)
            {
                goto error_exit;
            }
        }
    }

#ifdef DEBUG
    fprintf(stderr, "Using cert_dir = %s\n",
            (*cert_dir ? *cert_dir : "null"));
#endif /* DEBUG */

    if(!(*cert_dir))
    {
        result = globus_error_put(globus_error_construct_string(
            GLOBUS_GSI_CREDENTIAL_MODULE,
            NULL,
            "The trusted certificates directory could not be"
            "found in any of the following locations: \n"
            "1) env. var. X509_CERT_DIR=%s\n"
            "2) registry key x509_cert_dir: %s\n"
            "3) %s\n4) %s\n5) %s\n",
            env_cert_dir,
            reg_cert_dir,
            local_cert_dir,
            installed_cert_dir,
            default_cert_dir));

        goto error_exit;
    }

    result = GLOBUS_SUCCESS;
    goto done:

  error_exit:
    
    if(*cert_dir)
    {
        globus_free(*cert_dir);
        *cert_dir = NULL;
    }

 done:

    if(env_cert_dir && (env_cert_dir != (*cert_dir)))
    {
        globus_free(env_cert_dir);
    }
    if(reg_cert_dir && (reg_cert_dir != (*cert_dir)))
    {
        globus_free(reg_cert_dir);
    }
    if(local_cert_dir && (local_cert_dir != (*cert_dir)))
    {
        globus_free(local_cert_dir);
    }
    if(installed_cert_dir && (installed_cert_dir != (*cert_dir)))
    {
        globus_free(installed_cert_dir);
    }
    if(default_cert_dir && (default_cert_dir != (*cert_dir)))
    {
        globus_free(default_cert_dir);
    }

    return result;
}
/* @} */

/**
 * WIN32 - Get User Certificate Filename
 * @ingroup globus_gsi_cred_system_config_win32
 */
/* @{ */
/**
 * Get the User Certificate Filename based on the current user's
 * environment.  The following locations are searched for cert and key
 * files in order:
 * 
 * <ol>
 * <li>environment variables X509_USER_CERT and X509_USER_KEY
 * <li>registry keys x509_user_cert and x509_user_key in software\Globus\GSI
 * <li><users home directory>\.globus\usercert.pem and 
 *     <users home directory>\.globus\userkey.pem
 * <li><users home directory\.globus\usercred.p12 - this is a PKCS12 credential
 * </ol>
 *
 * @param user_cert
 *        pointer the filename of the user certificate
 * @param user_key
 *        pointer to the filename of the user key
 * @return
 *        GLOBUS_SUCCESS if the cert and key files were found in one
 *        of the possible locations, otherwise an error object identifier
 *        is returned
 */
globus_result_t
globus_gsi_cred_get_user_cert_filename_win32(
    char **                             user_cert,
    char **                             user_key)
{
    int                                 len;
    char *                              home = NULL;
    char *                              env_user_cert = NULL;
    char *                              env_user_key = NULL;
    char *                              reg_user_cert = NULL;
    char *                              reg_user_key = NULL;
    char *                              default_user_cert = NULL;
    char *                              default_user_key = NULL;
    char *                              default_pkcs12_user_cred = NULL;
    globus_result_t                     result;
    HKEY                                hkDir = NULL;
    char                                val_user_cert[512];
    char                                val_user_key[512];

    *user_cert = NULL;
    *user_key = NULL;

    /* first, check environment variables for valid filenames */

    if((result = globus_i_gsi_cred_create_cert_string(
                     user_cert,
                     & env_user_cert,
                     getenv(X509_USER_CERT))) != GLOBUS_SUCCESS ||
       (result = globus_i_gsi_cred_create_cert_string(
                     user_key,
                     & env_user_key,
                     getenv(X509_USER_KEY))) != GLOBUS_SUCCESS)
    {
        goto error_exit;
    }
       
    /* next, check windows registry keys for valid filenames */

    if(!(*user_cert) || !(*user_key))
    {
        RegOpenKey(HKEY_CURRENT_USER,GSI_REGISTRY_DIR,&hkDir);
        lval = sizeof(val_user_cert)-1;
        if (hkDir && (RegQueryValueEx(
                          hkDir,
                          "x509_user_cert",
                          0,
                          &type,
                          val_user_cert,&lval) == ERROR_SUCCESS))
        {
            if((result = globus_i_gsi_cred_create_cert_string(
                             user_cert,
                             & reg_user_cert,
                             val_user_cert)) != GLOBUS_SUCCESS ||
                (result = globus_i_gsi_cred_create_key_string(
                              user_key,
                              & reg_user_key,
                              val_user_key)) != GLOBUS_SUCCESS)
            {
                goto error_exit;
            }
        }
        RegCloseKey(hkDir);
    }


    /* next, check default locations */
    if(!(*user_cert) || !(*user_key))
    {
        if(GLOBUS_I_GSI_GET_HOME_DIR(&home) == GLOBUS_SUCCESS)
        {
            if((result = globus_i_gsi_cred_create_cert_string(
                             user_cert,
                             & default_user_cert,
                             "%s%s%s",
                             home,
                             DEFEAULT_SEPERATOR,
                             X509_DEFAULT_USER_CERT)) != GLOBUS_SUCCESS ||
               (result = globus_i_gsi_cred_create_key_string(
                              key_cert,
                              & default_key_cert,
                              "%s%s%s",
                              home,
                              DEFAULT_SEPERATOR,
                              X509_DEFAULT_USER_KEY)) != GLOBUS_SUCCESS)
            {
                goto error_exit;
            }
        }
    }

    /* if the cert & key don't exist in the default locations
     * or those specified by the environment variables, a
     * pkcs12 cert will be searched for
     */
    if(!(*user_cert) || !(*user_key))
    {
        if((result = globus_i_gsi_get_home_dir(&home)) == GLOBUS_SUCCESS)
        {
            if((result = globus_i_gsi_cred_create_key_string(
                              user_key,
                              & default_pkcs12_user_cred,
                              "%s%s%s",
                              home,
                              FILE_SEPERATOR,
                              X509_DEFAULT_PKCS12_FILE)) != GLOBUS_SUCCESS)
            {
                goto error_exit;
            }
            *user_cert = *user_key;
        }
    }

    if(!(*user_cert) || !(*user_key))
    {
        result = globus_i_gsi_credential_error_result(
            GLOBUS_GSI_CRED_ERROR_SYSTEM_CONFIG,
            __FILE__,
            _function_name_,
            __LINE__,
            "The user cert could not be found in: \n"
            "1) env. var. X509_USER_CERT=%s\n"
            "2) registry key x509_user_cert: %s\n"
            "3) %s\n4) %s\n\n"
            "The user key could not be found in:\n,"
            "1) env. var. X509_USER_KEY=%s\n"
            "2) registry key x509_user_key: %s\n"
            "3) %s\n4) %s\n",
            env_user_cert,
            reg_user_cert,
            default_user_cert,
            default_pkcs12_user_cred,
            env_user_key,
            reg_user_key,
            default_user_key,
            default_pkcs12_user_cred);

        goto error_exit;
    }

#ifdef DEBUG
    fprintf(stderr,"Using x509_user_cert=%s\n      x509_user_key =%s\n",
            (*user_cert) ? (*user_cert) : NULL, 
            (*user_key) ? (*user_key) : NULL);
#endif

    result = GLOBUS_SUCCESS;
    goto done;

 error_exit:
    
    if(*user_cert)
    {
        globus_free(*user_cert);
        *user_cert = NULL;
    }
    if(*user_key)
    {
        globus_free(*user_key);
        *user_key = NULL;
    }

 done:

    if(env_user_cert && env_user_cert != (*user_cert))
    {
        globus_free(env_user_cert);
    }
    if(env_user_key && env_user_key != (*user_key))
    {
        globus_free(env_user_key);
    }
    if(default_user_cert && default_user_cert != (*user_cert))
    {
        globus_free(default_user_cert);
    }
    if(default_user_key && default_user_key != (*user_key))
    {
        globus_free(default_user_key);
    }
    
    return result;
}
/* @} */

/**
 * WIN32 - Get Host Certificate and Key Filenames
 * @ingroup globus_gsi_cred_system_config_win32
 */
/* @{ */
/**
 * Get the Host Certificate and Key Filenames based on the current user's
 * environment.  The host cert and key are searched for in the following 
 * locations (in order):
 *
 * <ol>
 * <li>X509_USER_CERT and X509_USER_KEY environment variables
 * <li>registry keys x509_user_cert and x509_user_key in software\Globus\GSI
 * <li>SLANG: NOT DETERMINED - this is the default location
 * <li><GLOBUS_LOCATION>\etc\host[cert|key].pem
 * <li><users home directory>\.globus\host[cert|key].pem
 * </ol>
 * 
 * @param host_cert
 *        pointer to the host certificate filename
 * @param host_key
 *        pointer to the host key filename
 *
 * @return
 *        GLOBUS_SUCCESS if the host cert and key were found, otherwise
 *        an error object identifier is returned 
 */
globus_result_t
globus_gsi_cred_get_host_cert_filename_win32(
    char **                             host_cert,
    char **                             host_key)
{
    int                                 len;
    char *                              home = NULL;
    char *                              host_cert = NULL;
    char *                              host_key = NULL;
    char *                              env_host_cert = NULL;
    char *                              env_host_key = NULL;
    char *                              reg_host_cert = NULL;
    char *                              reg_host_key = NULL;
    char *                              default_host_cert = NULL;
    char *                              default_host_key = NULL;
    char *                              installed_host_cert = NULL;
    char *                              installed_host_key = NULL;
    char *                              local_host_cert = NULL;
    char *                              local_host_key = NULL;
    globus_result_t                     result;

    HKEY                                hkDir = NULL;
    char                                val_host_cert[512];
    char                                val_host_key[512];

    *host_cert = NULL;
    *host_key = NULL;

    /* first check environment variables for valid filenames */

    if((result = globus_i_gsi_cred_create_cert_string(
                     host_cert,
                     & env_host_cert,
                     getenv(X509_USER_CERT))) != GLOBUS_SUCCESS ||
       (result = globus_i_gsi_cred_create_key_string(
                     host_key,
                     & env_host_key,
                     getenv(X509_USER_KEY))) != GLOBUS_SUCCESS)
    {
        goto error_exit;
    }

    /* now check the windows registry for valid filenames */
    if(!(*host_cert) || !(*host_key))
    {
        RegOpenKey(HKEY_CURRENT_USER,GSI_REGISTRY_DIR,&hkDir);
        lval = sizeof(val_host_cert)-1;
        if (hkDir && (RegQueryValueEx(hkDir,
                                      "x509_user_cert",
                                      0,
                                      &type,
                                      val_host_cert,
                                      &lval) == ERROR_SUCCESS))
        {
            if((result = globus_i_gsi_cred_create_cert_string(
                             host_cert,
                             & reg_host_cert,
                             val_host_cert)) != GLOBUS_SUCCESS ||
               (result = globus_i_gsi_cred_create_cert_string(
                             host_key,
                             & reg_host_key,
                             val_host_key)) != GLOBUS_SUCCESS)
            {
                goto error_exit;
            }
        }
        RegCloseKey(hkDir);
    }

    /* now check default locations for valid filenames */
    if(!(*host_cert) || !(*host_key))
    {
        if((result = globus_i_gsi_get_home_dir(&home)) == GLOBUS_SUCCESS)
        {
            if((result = globus_i_gsi_cred_create_cert_string(
                             host_cert,
                             & default_host_cert,
                             "%s%s%s%s",
                             X509_DEFAULT_CERT_DIR,
                             FILE_SEPERATOR,
                             X509_HOST_PREFIX,
                             X509_CERT_SUFFIX)) != GLOBUS_SUCCESS ||
               (result = globus_i_gsi_cred_create_key_string(
                              host_key,
                              & default_key_cert,
                              "%s%s%s%s",
                              X509_DEFAULT_CERT_DIR,
                              FILE_SEPERATOR,
                              X509_HOST_PREFIX,
                              X509_KEY_SUFFIX)) != GLOBUS_SUCCESS)
            {
                goto error_exit;
            }
        }
    }

    /* now check intstalled location for host cert */
    if(!(*host_cert) || !(*host_key))
    {
        globus_location = getenv("GLOBUS_LOCATION");

        if(globus_location)
        {
            if((result = globus_i_gsi_cred_create_cert_string(
                             host_cert,
                             & installed_host_cert,
                             "%s%s%s%s%s%s",
                             globus_location,
                             FILE_SEPERATOR,
                             X509_INSTALLED_CERT_DIR,
                             FILE_SEPERATOR,
                             X509_HOST_PREFIX,
                             X509_CERT_SUFFIX)) != GLOBUS_SUCCESS ||
               (result = globus_i_gsi_cred_create_key_string(
                             host_key,
                             & installed_host_key,
                             "%s%s%s%s%s%s",
                             globus_location,
                             FILE_SEPERATOR,
                             X509_INSTALLED_CERT_DIR,
                             FILE_SEPERATOR,
                             X509_HOST_PREFIX,
                             X509_KEY_SUFFIX)) != GLOBUS_SUCCESS)
            {
                goto error_exit;
            }
        }
    }

    if(!(*host_cert) || !(*host_key))
    {
        if(GLOBUS_I_GSI_GET_HOME_DIR(&home) == GLOBUS_SUCCESS)
        {
            if((result = globus_i_gsi_create_cert_string(
                             host_cert,
                             & local_host_cert,
                             "%s%s%s%s%s%s",
                             home,
                             FILE_SEPERATOR,
                             X509_LOCAL_CERT_DIR,
                             FILE_SEPERATOR,
                             X509_HOST_PREFIX,
                             X509_CERT_SUFFIX)) != GLOBUS_SUCCESS ||
               (result = globus_i_gsi_create_key_string(
                             host_key,
                             & local_key_cert,
                             "%s%s%s%s%s%s",
                             home,
                             FILE_SEPERATOR,
                             X509_LOCAL_CERT_DIR,
                             FILE_SEPERATOR,
                             X509_HOST_PREFIX,
                             X509_KEY_SUFFIX)) != GLOBUS_SUCCESS)
            {
                goto error_exit;
            }
        }
    }

#ifdef DEBUG
    fprintf(stderr,"Using x509_user_cert=%s\n      x509_user_key =%s\n",
            host_cert, host_key);
#endif

    if(!(*host_cert) || !(*host_key))
    {
        result = globus_i_gsi_credential_error_result(
            GLOBUS_GSI_CRED_ERROR_SYSTEM_CONFIG,
            __FILE__,
            _function_name_,
            __LINE__,
            "The user cert could not be found in: \n"
            "1) env. var. X509_USER_CERT=%s\n"
            "2) registry key x509_user_cert: %s\n"
            "3) %s\n4) %s5) %s\n\n"
            "The user key could not be found in:\n,"
            "1) env. var. X509_USER_KEY=%s\n"
            "2) registry key x509_user_key: %s\n"
            "3) %s\n4) %s5) %s\n",
            env_host_cert,
            reg_host_cert,
            default_host_cert,
            installed_host_cert,
            local_host_cert,
            env_host_key,
            reg_host_key,
            default_host_key,
            installed_host_key,
            local_host_key);

        goto error_exit;
    }

    result = GLOBUS_SUCCESS;
    goto done;

 error_exit:

    if(*host_cert)
    {
        globus_free(*host_cert);
        *host_cert = NULL;
    }
    if(*host_key)
    {
        globus_free(*host_key);
        *host_key = NULL;
    }

 done:

    if(env_host_cert && env_host_cert != *host_cert)
    {
        globus_free(env_host_cert);
    }
    if(env_host_key && env_host_key != *host_key)
    {
        globus_free(env_host_key);
    }
    if(reg_host_cert && reg_host_cert != *host_cert)
    {
        globus_free(reg_host_cert);
    }
    if(reg_host_key && reg_host_key != *host_key)
    {
        globus_free(reg_host_key);
    }
    if(installed_host_cert && installed_host_cert != *host_cert)
    {
        globus_free(installed_host_cert);
    }
    if(installed_host_key && installed_host_key != *host_key)
    {
        globus_free(installed_host_key);
    }
    if(local_host_cert && local_host_cert != *host_cert)
    {
        globus_free(local_host_cert);
    }
    if(local_host_key && local_host_key != *host_key)
    {
        globus_free(local_host_key);
    }
    if(default_host_cert && default_host_cert != host_cert)
    {
        globus_free(default_host_cert);
    }
    if(default_host_key && default_host_key != host_key)
    {
        globus_free(default_host_key);
    }

    return result;
}
/* @} */

/**
 * WIN32 - Get Service Certificate and Key Filenames
 * @ingroup globus_gsi_cred_system_config_win32
 */
/* @{ */
/**
 * Get the Service Certificate Filename based on the current user's
 * environment.  The host cert and key are searched for in the following 
 * locations (in order):
 *
 * <ol>
 * <li>X509_USER_CERT and X509_USER_KEY environment variables
 * <li>registry keys x509_user_cert and x509_user_key in software\Globus\GSI
 * <li>SLANG: NOT DETERMINED - this is the default location
 * <li>GLOBUS_LOCATION\etc\{service_name}\{service_name}[cert|key].pem
 *     So for example, if my service was named: myservice, the location
 *     of the certificate would be: 
 *     GLOBUS_LOCATION\etc\myservice\myservicecert.pem
 * <li><users home>\.globus\{service_name}\{service_name}[cert|key].pem
 * </ol>
 * 
 * @param service_name
 *        The name of the service which allows us to determine the
 *        locations of cert and key files to look for
 * @param service_cert
 *        pointer to the host certificate filename
 * @param service_key
 *        pointer to the host key filename
 *
 * @return
 *        GLOBUS_SUCCESS if the service cert and key were found, otherwise
 *        an error object identifier 
 */
globus_result_t
globus_gsi_cred_get_service_cert_filename_win32(
    char *                              service_name,
    char **                             service_cert_filename,
    char **                             service_key_filename)
{
    int                                 len;
    char *                              home = NULL;
    char *                              service_cert = NULL;
    char *                              service_key = NULL;
    char *                              env_service_cert = NULL;
    char *                              env_service_key = NULL;
    char *                              reg_service_cert = NULL;
    char *                              reg_service_key = NULL;
    char *                              default_service_cert = NULL;
    char *                              default_service_key = NULL;
    char *                              installed_service_cert = NULL;
    char *                              installed_service_key = NULL;
    char *                              local_service_cert = NULL;
    char *                              local_service_key = NULL;
    globus_result_t                     result;

    HKEY                                hkDir = NULL;
    char                                val_service_cert[512];
    char                                val_service_key[512];

    *service_cert = NULL;
    *service_key = NULL;

    /* first check environment variables for valid filenames */

    if((result = globus_i_gsi_cred_create_cert_string(
                     service_cert,
                     & env_service_cert,
                     getenv(X509_USER_CERT))) != GLOBUS_SUCCESS ||
       (result = globus_i_gsi_cred_create_key_string(
                     service_key,
                     & env_service_key,
                     getenv(X509_USER_KEY))) != GLOBUS_SUCCESS)
    {
        goto error_exit;
    }

    /* now check the windows registry for valid filenames */
    if(!(*service_cert) || !(*service_key))
    {
        RegOpenKey(HKEY_CURRENT_USER,GSI_REGISTRY_DIR,&hkDir);
        lval = sizeof(val_service_cert)-1;
        if (hkDir && (RegQueryValueEx(hkDir,
                                      "x509_user_cert",
                                      0,
                                      &type,
                                      val_service_cert,
                                      &lval) == ERROR_SUCCESS))
        {
            if((result = globus_i_gsi_cred_create_cert_string(
                             service_cert,
                             & reg_service_cert,
                             val_service_cert)) != GLOBUS_SUCCESS ||
               (result = globus_i_gsi_cred_create_cert_string(
                             service_key,
                             & reg_service_key,
                             val_service_key)) != GLOBUS_SUCCESS)
            {
                goto error_exit;
            }
        }
        RegCloseKey(hkDir);
    }


    /* now check default locations for valid filenames */
    if(!(*service_cert) || !(*service_key))
    {
        if((result = globus_i_gsi_get_home_dir(&home)) == GLOBUS_SUCCESS)
        {
            if((result = globus_i_gsi_cred_create_cert_string(
                             service_cert,
                             & default_service_cert,
                             "%s%s%s%s%s%s",
                             X509_DEFAULT_CERT_DIR,
                             FILE_SEPERATOR,
                             service_name,
                             FILE_SEPERATOR,
                             service_name,
                             X509_CERT_SUFFIX)) != GLOBUS_SUCCESS ||
               (result = globus_i_gsi_cred_create_key_string(
                              service_key,
                              & default_key_cert,
                              "%s%s%s%s%s%s",
                              X509_DEFAULT_CERT_DIR,
                              FILE_SEPERATOR,
                              service_name,
                              FILE_SEPERATOR,
                              service_name,
                              X509_KEY_SUFFIX)) != GLOBUS_SUCCESS)
            {
                goto error_exit;
            }
        }
    }

    /* now check intstalled location for service cert */
    if(!(*service_cert) || !(*service_key))
    {
        globus_location = getenv("GLOBUS_LOCATION");

        if(globus_location)
        {
            if((result = globus_i_gsi_cred_create_cert_string(
                             service_cert,
                             & installed_service_cert,
                             "%s%s%s%s%s%s%s%s",
                             globus_location,
                             FILE_SEPERATOR,
                             X509_INSTALLED_CERT_DIR,
                             FILE_SEPERATOR,
                             service_name,
                             FILE_SEPERATOR,
                             service_name,
                             X509_CERT_SUFFIX)) != GLOBUS_SUCCESS ||
               (result = globus_i_gsi_cred_create_key_string(
                             service_key,
                             & installed_service_key,
                             "%s%s%s%s%s%s%s%s",
                             globus_location,
                             FILE_SEPERATOR,
                             X509_INSTALLED_CERT_DIR,
                             FILE_SEPERATOR,
                             service_name,
                             FILE_SEPERATOR,
                             service_name,
                             X509_KEY_SUFFIX)) != GLOBUS_SUCCESS)
            {
                goto error_exit;
            }
        }
    }

    if(!(*service_cert) || !(*service_key))
    {
        if(GLOBUS_I_GSI_GET_HOME_DIR(&home) == GLOBUS_SUCCESS)
        {
            if((result = globus_i_gsi_create_cert_string(
                             service_cert,
                             & local_service_cert,
                             "%s%s%s%s%s%s%s",
                             home,
                             FILE_SEPERATOR,
                             X509_LOCAL_CERT_DIR,
                             FILE_SEPERATOR,
                             service_name,
                             FILE_SEPERATOR,
                             service_name,
                             X509_CERT_SUFFIX)) != GLOBUS_SUCCESS ||
               (result = globus_i_gsi_create_key_string(
                             service_key,
                             & local_key_cert,
                             "%s%s%s%s%s%s%s%s",
                             home,
                             FILE_SEPERATOR,
                             X509_LOCAL_CERT_DIR,
                             FILE_SEPERATOR,
                             service_name,
                             FILE_SEPERATOR,
                             service_name,
                             X509_KEY_SUFFIX)) != GLOBUS_SUCCESS)
            {
                goto error_exit;
            }
        }
    }

#ifdef DEBUG
    fprintf(stderr,"Using x509_user_cert=%s\n      x509_user_key =%s\n",
            service_cert, service_key);
#endif

    if(!(*service_cert) || !(*service_key))
    {
        result = globus_i_gsi_credential_error_result(
            GLOBUS_GSI_CRED_ERROR_SYSTEM_CONFIG,
            __FILE__,
            _function_name_,
            __LINE__,
            "The user cert could not be found in: \n"
            "1) env. var. X509_USER_CERT=%s\n"
            "2) registry key x509_user_cert: %s\n"
            "3) %s\n4) %s5) %s\n\n"
            "The user key could not be found in:\n,"
            "1) env. var. X509_USER_KEY=%s\n"
            "2) registry key x509_user_key: %s\n"
            "3) %s\n4) %s5) %s\n",
            env_service_cert,
            reg_service_cert,
            default_service_cert,
            installed_service_cert,
            local_service_cert,
            env_service_key,
            reg_service_key,
            default_service_key,
            installed_service_key,
            local_service_key);

        goto error_exit;
    }

    result = GLOBUS_SUCCESS;
    goto done;

 error_exit:

    if(*service_cert)
    {
        globus_free(*service_cert);
        *service_cert = NULL;
    }
    if(*service_key)
    {
        globus_free(*service_key);
        *service_key = NULL;
    }

 done:

    if(env_service_cert && env_service_cert != *service_cert)
    {
        globus_free(env_service_cert);
    }
    if(env_service_key && env_service_key != *service_key)
    {
        globus_free(env_service_key);
    }
    if(reg_service_cert && reg_service_cert != *service_cert)
    {
        globus_free(reg_service_cert);
    }
    if(reg_service_key && reg_service_key != *service_key)
    {
        globus_free(reg_service_key);
    }
    if(installed_service_cert && installed_service_cert != *service_cert)
    {
        globus_free(installed_service_cert);
    }
    if(installed_service_key && installed_service_key != *service_key)
    {
        globus_free(installed_service_key);
    }
    if(local_service_cert && local_service_cert != *service_cert)
    {
        globus_free(local_service_cert);
    }
    if(local_service_key && local_service_key != *service_key)
    {
        globus_free(local_service_key);
    }
    if(default_service_cert && default_service_cert != service_cert)
    {
        globus_free(default_service_cert);
    }
    if(default_service_key && default_service_key != service_key)
    {
        globus_free(default_service_key);
    }

    return result;
}
/* @} */

/**
 * WIN32 - Get Proxy Filename
 * @ingroup globus_gsi_cred_system_config_win32
 */
/* @{ */
/**
 * Get the proxy cert filename based on the following
 * search order:
 * 
 * <ol>
 * <li> X509_USER_PROXY environment variable - This environment variable
 * is set by the at run time for the specific application.  If
 * the proxy_file_type variable is set to GLOBUS_PROXY_OUTPUT
 *  (a proxy filename for writing is requested), 
 * and the X509_USER_PROXY is set, this will be the 
 * resulting value of the user_proxy filename string passed in.  If the
 * proxy_file_type is set to GLOBUS_PROXY_INPUT and X509_USER_PROXY is 
 * set, but the file it points to does not exist, 
 * or has some other readability issues, the 
 * function will continue checking using the other methods available.
 * 
 * <li> check the registry key: x509_user_proxy.  Just as with
 * the environment variable, if the registry key is set, and proxy_file_type
 * is GLOBUS_PROXY_OUTPUT, the string set to be the proxy 
 * filename will be this registry
 * key's value.  If proxy_file_type is GLOBUS_PROXY_INPUT, and the 
 * file doesn't exist, the function will check the next method 
 * for the proxy's filename.
 * 
 * <li> Check the default location for the proxy file.  The default
 * location should be 
 * set to reside in the temp directory on that host, with the filename
 * taking the format:  x509_u<user id>
 * where <user id> is some unique string for that user on the host
 * </ol>
 *
 * @param user_proxy
 *        the proxy filename of the user
 *
 * @return
 *        GLOBUS_SUCCESS or an error object identifier
 */
globus_result_t
globus_gsi_cred_get_proxy_filename_win32(
    char **                             user_proxy,
    globus_gsi_proxy_file_type_t        proxy_file_type)
{
    char *                              env_user_proxy = NULL;
    char *                              env_value = NULL;
    char *                              default_user_proxy = NULL;
    char *                              reg_user_proxy = NULL;
    HKEY                                hkDir = NULL;
    char                                val_user_proxy[512];
    int                                 len;
    globus_result_t                     result;
    char *                              user_id_string;

    *user_proxy = NULL;

    if((env_value = getenv(X509_USER_PROXY)) != NULL &&
       (result = globus_i_gsi_cred_create_key_string(
                     user_proxy,
                     & env_user_proxy,
                     getenv(X509_USER_PROXY))) != GLOBUS_SUCCESS)
    {
        goto error_exit;
    }
    
    /* check if the proxy file type is for writing */
    if(!(*user_proxy) && env_user_proxy && proxy_file == GLOBUS_PROXY_OUTPUT)
    {
        *user_proxy = env_user_proxy;
    }

    if (!(*user_proxy))
    {
        RegOpenKey(HKEY_CURRENT_USER,GSI_REGISTRY_DIR,&hkDir);
        lval = sizeof(val_user_proxy)-1;
        if (hkDir && (RegQueryValueEx(hkDir, "x509_user_proxy", 0, &type,
                                      val_user_proxy, &lval) == ERROR_SUCCESS))
        {
            if((result = globus_i_gsi_cred_create_key_string(
                             proxy_cert,
                             & reg_user_proxy,
                             val_user_proxy)) != GLOBUS_SUCCESS)
            {
                goto error_exit;
            }
        }
        RegCloseKey(hkDir);
    }

    if(!(*user_proxy) && reg_user_proxy && proxy_file == GLOBUS_PROXY_OUTPUT)
    {
        *user_proxy = reg_user_proxy;
    }

    if (!user_proxy)
    {
        if((result = globus_i_gsi_get_user_id_string(&user_id_string))
           != GLOBUS_SUCCESS)
        {
            goto error_exit;
        }
        if((result = globus_i_gsi_create_key_string(
                          user_proxy,
                          & default_user_proxy,
                          "%s%s%s%s",
                          DEFAULT_SECURE_TMP_DIR,
                          FILE_SEPERATOR,
                          X509_USER_PROXY_FILE,
                          user_id_string)) != GLOBUS_SUCCESS)
        {
            goto error_exit;
        }
    }

    if(!(*user_proxy) && 
       default_user_proxy && 
       proxy_file_type == GLOBUS_PROXY_FILE_OUTPUT)
    {
        *user_proxy = default_user_proxy;
    }

    if(!(*user_proxy))
    {            
        result = globus_i_gsi_credential_error_result( 
            GLOBUS_GSI_CRED_ERROR_SYSTEM_CONFIG,
            __FILE__,
            _function_name_,
            __LINE__, 
            "A file location for%s the proxy cert could be found in: \n"
            "1) env. var. X509_USER_PROXY=%s\n"
            "2) registry key x509_user_proxy: %s\n"
            "3) %s\n",
            (proxy_file_type == GLOBUS_PROXY_FILE_INPUT) ? "" : " writing",
            env_user_proxy,
            reg_user_proxy,
            default_user_proxy);
        
        goto error_exit;
    }

    result = GLOBUS_SUCCESS;
    goto done;

 error_exit:
    
    if(*user_proxy)
    {
        globus_free(*user_proxy);
        *user_proxy = NULL;
    }

 done:

    if(reg_user_proxy && (reg_user_proxy != (*user_proxy)))
    {
        globus_free(reg_user_proxy);
    }
    if(default_user_proxy && (default_user_proxy != (*default_user_proxy)))
    {
        globus_free(default_user_proxy);
    }
    
    return result;
}
/* @} */

#else /* if WIN32 is not defined */

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL

/**
 * UNIX - Get HOME Directory
 * @ingroup globus_i_gsi_cred_system_config_unix
 */
/* @{ */
/**
 * Get the HOME Directory of the current user.  Should
 * be the $HOME environment variable.
 *
 * @param home_dir
 *        The home directory of the current user
 * @return
 *        GLOBUS_SUCCESS if no error occured, otherwise
 *        an error object is returned.
 */
globus_result_t
globus_i_gsi_get_home_dir_unix(
    char **                             home_dir)
{
    static char *                        _function_name_ =
        "globus_i_gsi_get_home_dir_unix";

    *home_dir = (char *) getenv("HOME");

    if((*home_dir) == NULL)
    {
        return GLOBUS_GSI_CRED_ERROR_RESULT(
            GLOBUS_GSI_CRED_ERROR_SYSTEM_CONFIG);
    }
    return GLOBUS_SUCCESS;
}
/* @} */

/**
 * UNIX - File Exists
 * @ingroup globus_i_gsi_cred_system_config_unix
 */
/* @{ */
/**
 * Check if the file exists
 *
 * @param filename the filename of the file to check for
 * @param status  the resulting status of the file
 *
 * @return
 *        GLOBUS_SUCCESS for almost all cases (even if the file
 *        doesn't exist), otherwise an error object identifier
 *        wrapping the system errno is returned
 */
globus_result_t
globus_i_gsi_file_exists_unix(
    const char *                        filename,
    globus_gsi_statcheck_t *            status)
{
    struct stat                         stx;

    static char *                       _function_name_ =
        "globus_i_gsi_file_exists_win32";

    if (stat(filename,&stx) == -1)
    {
        switch (errno)
        {
        case ENOENT:
        case ENOTDIR:
            *status = GLOBUS_DOES_NOT_EXIST;
            return GLOBUS_SUCCESS;

        case EACCES:

            *status = GLOBUS_BAD_PERMISSIONS;
            return GLOBUS_SUCCESS;

        default:
            return globus_error_put(
                globus_error_wrap_errno_error(
                    GLOBUS_GSI_CREDENTIAL_MODULE,
                    errno,
                    GLOBUS_GSI_CRED_ERROR_SYSTEM_CONFIG,
                    __FILE__":__LINE__:%s: Error getting status of keyfile\n",
                    _function_name_));
        }
    }

    /*
     * use any stat output as random data, as it will 
     * have file sizes, and last use times in it. 
     */
    RAND_add((void*)&stx,sizeof(stx),2);

    if (stx.st_size == 0)
    {
        *status = GLOBUS_ZERO_LENGTH;
        return GLOBUS_SUCCESS;
    }

    *status = GLOBUS_VALID;
    return GLOBUS_SUCCESS;
}    
/* @} */

/**
 * UNIX - Check File Status for Key
 * @ingroup globus_i_gsi_cred_system_config_unix
 */
/* @{ */
/**
 * This is a convenience function used to check the status of a 
 * private key file.  The desired status is only the current user has
 * ownership and read permissions, everyone else should not be able
 * to access it.
 * 
 * @param filename
 *        The name of the file to check the status of
 * @param status
 *        The status of the file being checked
 *        see @ref globus_gsi_statcheck_t for possible values
 *        of this variable 
 *
 * @return 
 *        GLOBUS_SUCCESS if the status of the file was able
 *        to be determined.  Otherwise, an error object
 *        identifier
 *
 * @see globus_gsi_statcheck_t
 */
globus_result_t
globus_i_gsi_check_keyfile_unix(
    const char *                        filename,
    globus_gsi_statcheck_t *            status)
{
    struct stat                         stx;

    static char *                       _function_name_ =
        "globus_i_gsi_check_keyfile_unix";

    if (stat(filename,&stx) == -1)
    {
        switch (errno)
        {
        case ENOENT:
        case ENOTDIR:
            *status = GLOBUS_DOES_NOT_EXIST;
            return GLOBUS_SUCCESS;

        case EACCES:

            *status = GLOBUS_BAD_PERMISSIONS;
            return GLOBUS_SUCCESS;

        default:
            return globus_error_put(
                globus_error_wrap_errno_error(
                    GLOBUS_GSI_CREDENTIAL_MODULE,
                    errno,
                    GLOBUS_GSI_CRED_ERROR_SYSTEM_CONFIG,
                    __FILE__":__LINE__:%s: Error getting status of keyfile\n",
                    _function_name_));
        }
    }

    /*
     * use any stat output as random data, as it will 
     * have file sizes, and last use times in it. 
     */
    RAND_add((void*)&stx,sizeof(stx),2);

    if (stx.st_uid != getuid())
    {
        *status = GLOBUS_NOT_OWNED;
        return GLOBUS_SUCCESS;
    }

    /* check that the key file is not wx by user, or rwx by group or others */
    if (stx.st_mode & (S_IXUSR | 
                       S_IRGRP | S_IWGRP | S_IXGRP |
                       S_IROTH | S_IWOTH | S_IXOTH))
    {
#ifdef DEBUG
        fprintf(stderr,"checkstat:%s:mode:%o\n",filename,stx.st_mode);
#endif
        *status = GLOBUS_BAD_PERMISSIONS;
        return GLOBUS_SUCCESS;
    }
    

    if (stx.st_size == 0)
    {
        *status = GLOBUS_ZERO_LENGTH;
        return GLOBUS_SUCCESS;
    }

    *status = GLOBUS_VALID;
    return GLOBUS_SUCCESS;
}
/* @} */

/**
 * UNIX - Check File Status for Cert
 * @ingroup globus_i_gsi_cred_system_config_unix
 */
/* @{ */
/**
 * This is a convenience function used to check the status of a 
 * certificate file.  The desired status is the current user has
 * ownership and read/write permissions, while group and others only
 * have read permissions.
 * 
 * @param filename
 *        The name of the file to check the status of
 * @param status
 *        The status of the file being checked
 *        see @ref globus_gsi_statcheck_t for possible values
 *        of this variable 
 *
 * @return 
 *        GLOBUS_SUCCESS if the status of the file was able
 *        to be determined.  Otherwise, an error object
 *        identifier
 *
 * @see globus_gsi_statcheck_t
 */
globus_result_t
globus_i_gsi_check_certfile_unix(
    const char *                        filename,
    globus_gsi_statcheck_t *            status)
{
    struct stat                         stx;

    static char *                       _function_name_ =
        "globus_i_gsi_check_certfile_unix";

    if (stat(filename,&stx) == -1)
    {
        switch (errno)
        {
        case ENOENT:
        case ENOTDIR:
            *status = GLOBUS_DOES_NOT_EXIST;
            return GLOBUS_SUCCESS;

        case EACCES:

            *status = GLOBUS_BAD_PERMISSIONS;
            return GLOBUS_SUCCESS;

        default:
            return globus_error_put(
                globus_error_wrap_errno_error(
                    GLOBUS_GSI_CREDENTIAL_MODULE,
                    errno,
                    GLOBUS_GSI_CRED_ERROR_SYSTEM_CONFIG,
                    __FILE__":__LINE__:%s: Error getting status of keyfile\n",
                    _function_name_));
        }
    }

    /*
     * use any stat output as random data, as it will 
     * have file sizes, and last use times in it. 
     */
    RAND_add((void*)&stx,sizeof(stx),2);

    if (stx.st_uid != getuid())
    {
        *status = GLOBUS_NOT_OWNED;
        return GLOBUS_SUCCESS;
    }

    /* check that the cert file is not x by user, or wx by group or others */
    if (stx.st_mode & (S_IXUSR |
                       S_IWGRP | S_IXGRP |
                       S_IWOTH | S_IXOTH))
    {
#ifdef DEBUG
        fprintf(stderr,"checkstat:%s:mode:%o\n",filename,stx.st_mode);
#endif
        *status = GLOBUS_BAD_PERMISSIONS;
        return GLOBUS_SUCCESS;
    }
    
    if (stx.st_size == 0)
    {
        *status = GLOBUS_ZERO_LENGTH;
        return GLOBUS_SUCCESS;
    }

    *status = GLOBUS_VALID;
    return GLOBUS_SUCCESS;
}
/* @} */

/**
 * UNIX - Get User ID
 * @ingroup globus_i_gsi_cred_system_config_unix
 */
/* @{ */
/**
 * Get a unique string representing the current user.  This is just
 * the uid converted to a string.  
 *
 * @param user_id_string
 *        A unique string representing the user
 *
 * @return
 *        GLOBUS_SUCCESS unless an error occurred
 */
globus_result_t
globus_i_gsi_get_user_id_string_unix(
    char **                             user_id_string)
{
    int                                 uid;
    int                                 len;
    FILE *                              null_file_stream;

    uid = getuid();

    null_file_stream = fopen("/dev/null", "w");
    len = fprintf(null_file_stream, "%d", uid);
    fclose(null_file_stream);
    *user_id_string = (char *) globus_malloc(len);
    if((*user_id_string) == NULL)
    {
        return GLOBUS_GSI_SYSTEM_CONFIG_MALLOC_ERROR;
    }
    sprintf(*user_id_string, "%d", uid);
    return GLOBUS_SUCCESS;
}
/* @} */

#endif

/**
 * UNIX - Get Trusted CA Cert Dir
 * @ingroup globus_gsi_cred_system_config_unix
 */
/* @{ */
/**
 * Get the Trusted Certificate Directory containing the trusted
 * Certificate Authority certificates.  This directory is determined
 * in the order shown below.  Failure in one method results in attempting
 * the next.
 *
 * <ol>
 * <li> <b>X509_CERT_DIR environment variable</b> - if this is set, the
 * trusted certificates will be searched for in that directory.  This
 * variable allows the end user to specify the location of trusted
 * certificates.
 * <li> <b>$HOME/.globus/certificates</b> - If this
 * directory exists, and the previous methods of determining the trusted
 * certs directory failed, this directory will be used.  
 * <li> <b>/etc/grid-security/certificates</b> - This location is intended
 * to be independant of the globus installation ($GLOBUS_LOCATION), and 
 * is generally only writeable by the host system administrator.  
 * <li> <b>$GLOBUS_LOCATION/share/certificates</b>
 * </ol>
 *
 * @param cert_dir
 *        The trusted certificates directory
 * @return
 *        GLOBUS_SUCCESS if no error occurred, and a sufficient trusted
 *        certificates directory was found.  Otherwise, an error object 
 *        identifier returned.
 */
globus_result_t
globus_gsi_cred_get_cert_dir_unix(
    char **                             cert_dir)
{
    char *                              env_cert_dir = NULL;
    char *                              local_cert_dir = NULL;
    char *                              default_cert_dir = NULL;
    char *                              installed_cert_dir = NULL;
    globus_result_t                     result;
    char *                              home;
    char *                              globus_location;

    *cert_dir = NULL;

    if((result = globus_i_gsi_cred_create_cert_dir_path(
                     cert_dir, 
                     & env_cert_dir,
                     getenv(X509_CERT_DIR))) != GLOBUS_SUCCESS)
    {
        goto error_exit;
    }

    /* now check for a trusted CA directory in the user's home directory */
    if(!(*cert_dir))
    {
        if((result = GLOBUS_I_GSI_GET_HOME_DIR(&home)) != GLOBUS_SUCCESS)
        {
            goto error_exit;
        }
            
        if (home) 
        {
            if((result = globus_i_gsi_cred_create_cert_dir_path(
                             cert_dir, 
                             & local_cert_dir,
                             "%s%s%s",
                             home,
                             FILE_SEPERATOR,
                             X509_LOCAL_TRUSTED_CERT_DIR)) != GLOBUS_SUCCESS)
            {
                goto error_exit;
            }
        }
    }

    /* now look in $GLOBUS_LOCATION/share/certificates */
    if (!(*cert_dir))
    {
        if((result = globus_i_gsi_cred_create_cert_dir_path(
                         cert_dir,
                         & installed_cert_dir,
                         X509_INSTALLED_TRUSTED_CERT_DIR)) != GLOBUS_SUCCESS)
        {
            goto error_exit;
        }
    }

    /* now check for host based default directory */
    if (!(*cert_dir))
    {
        globus_location = getenv("GLOBUS_LOCATION");
        
        if (globus_location)
        {
            if((result = globus_i_gsi_cred_create_cert_dir_path(
                             cert_dir,
                             & default_cert_dir,
                             "%s%s%s",
                             globus_location,
                             FILE_SEPERATOR,
                             X509_DEFAULT_TRUSTED_CERT_DIR)) != GLOBUS_SUCCESS)
            {
                goto error_exit;
            }
        }
    }

#ifdef DEBUG
    fprintf(stderr, "Using cert_dir = %s\n",
            (*cert_dir ? *cert_dir : "null"));
#endif /* DEBUG */

    if(!(*cert_dir))
    {
        result = globus_error_put(globus_error_construct_string(
            GLOBUS_GSI_CREDENTIAL_MODULE,
            NULL,
            "The trusted certificates directory could not be"
            "found in any of the following locations: \n"
            "1) env. var. X509_CERT_DIR=%s\n"
            "2) %s\n3) %s\n4) %s\n",
            env_cert_dir,
            local_cert_dir,
            installed_cert_dir,
            default_cert_dir));

        goto error_exit;
    }

    result = GLOBUS_SUCCESS;
    goto done;

  error_exit:
    
    if(*cert_dir)
    {
        globus_free(*cert_dir);
        *cert_dir = NULL;
    }

 done:

    if(env_cert_dir && (env_cert_dir != (*cert_dir)))
    {
        globus_free(env_cert_dir);
    }
    if(local_cert_dir && (local_cert_dir != (*cert_dir)))
    {
        globus_free(local_cert_dir);
    }
    if(installed_cert_dir && (installed_cert_dir != (*cert_dir)))
    {
        globus_free(installed_cert_dir);
    }
    if(default_cert_dir && (default_cert_dir != (*cert_dir)))
    {
        globus_free(default_cert_dir);
    }

    return result;
}
/* @} */

/**
 * UNIX - Get User Certificate Filename
 * @ingroup globus_gsi_cred_system_config_unix
 */
/* @{ */
/**
 * Get the User Certificate Filename based on the current user's
 * environment.  The following locations are searched for cert and key
 * files in order:
 * 
 * <ol>
 * <li>environment variables X509_USER_CERT and X509_USER_KEY
 * <li>$HOME/.globus/usercert.pem and 
 *     $HOME/.globus/userkey.pem
 * <li>$HOME/.globus/usercred.p12 - this is a PKCS12 credential
 * </ol>
 *
 * @param user_cert
 *        pointer the filename of the user certificate
 * @param user_key
 *        pointer to the filename of the user key
 * @return
 *        GLOBUS_SUCCESS if the cert and key files were found in one
 *        of the possible locations, otherwise an error object identifier
 *        is returned
 */
globus_result_t
globus_gsi_cred_get_user_cert_filename_unix(
    char **                             user_cert,
    char **                             user_key)
{
    char *                              home = NULL;
    char *                              env_user_cert = NULL;
    char *                              env_user_key = NULL;
    char *                              default_user_cert = NULL;
    char *                              default_user_key = NULL;
    char *                              default_pkcs12_user_cred = NULL;
    globus_result_t                     result;

    static char *                       _function_name_ =
        "globus_gsi_cred_get_user_cert_filename_unix";

    *user_cert = NULL;
    *user_key = NULL;

    /* first, check environment variables for valid filenames */

    if((result = globus_i_gsi_cred_create_cert_string(
                     user_cert,
                     & env_user_cert,
                     getenv(X509_USER_CERT))) != GLOBUS_SUCCESS ||
       (result = globus_i_gsi_cred_create_cert_string(
                     user_key,
                     & env_user_key,
                     getenv(X509_USER_KEY))) != GLOBUS_SUCCESS)
    {
        goto error_exit;
    }

    /* next, check default locations */
    if(!(*user_cert) || !(*user_key))
    {
        if(GLOBUS_I_GSI_GET_HOME_DIR(&home) == GLOBUS_SUCCESS)
        {
            if((result = globus_i_gsi_cred_create_cert_string(
                             user_cert,
                             & default_user_cert,
                             "%s%s%s",
                             home,
                             FILE_SEPERATOR,
                             X509_DEFAULT_USER_CERT)) != GLOBUS_SUCCESS ||
               (result = globus_i_gsi_cred_create_key_string(
                              user_key,
                              & default_user_key,
                              "%s%s%s",
                              home,
                              FILE_SEPERATOR,
                              X509_DEFAULT_USER_KEY)) != GLOBUS_SUCCESS)
            {
                goto error_exit;
            }
        }
    }

    /* if the cert & key don't exist in the default locations
     * or those specified by the environment variables, a
     * pkcs12 cert will be searched for
     */
    if(!(*user_cert) || !(*user_key))
    {
        if((result = GLOBUS_I_GSI_GET_HOME_DIR(&home)) == GLOBUS_SUCCESS)
        {
            if((result = globus_i_gsi_cred_create_key_string(
                              user_key,
                              & default_pkcs12_user_cred,
                              "%s%s%s",
                              home,
                              FILE_SEPERATOR,
                              X509_DEFAULT_PKCS12_FILE)) != GLOBUS_SUCCESS)
            {
                goto error_exit;
            }
            *user_cert = *user_key;
        }
    }

    if(!(*user_cert) || !(*user_key))
    {
        result = globus_i_gsi_credential_error_result(
            GLOBUS_GSI_CRED_ERROR_SYSTEM_CONFIG,
            __FILE__,
            _function_name_,
            __LINE__,
            "The user cert could not be found in: \n"
            "1) env. var. X509_USER_CERT=%s\n"
            "2) %s\n3) %s\n\n"
            "The user key could not be found in:\n,"
            "1) env. var. X509_USER_KEY=%s\n"
            "2) %s\n3) %s\n",
            env_user_cert,
            default_user_cert,
            default_pkcs12_user_cred,
            env_user_key,
            default_user_key,
            default_pkcs12_user_cred);

        goto error_exit;
    }

#ifdef DEBUG
    fprintf(stderr,"Using x509_user_cert=%s\n      x509_user_key =%s\n",
            (*user_cert) ? (*user_cert) : NULL, 
            (*user_key) ? (*user_key) : NULL);
#endif

    result = GLOBUS_SUCCESS;
    goto done;

 error_exit:
    
    if(*user_cert)
    {
        globus_free(*user_cert);
        *user_cert = NULL;
    }
    if(*user_key)
    {
        globus_free(*user_key);
        *user_key = NULL;
    }

 done:

    if(env_user_cert && env_user_cert != (*user_cert))
    {
        globus_free(env_user_cert);
    }
    if(env_user_key && env_user_key != (*user_key))
    {
        globus_free(env_user_key);
    }
    if(default_user_cert && default_user_cert != (*user_cert))
    {
        globus_free(default_user_cert);
    }
    if(default_user_key && default_user_key != (*user_key))
    {
        globus_free(default_user_key);
    }
    
    return result;
}
/* @} */

/**
 * UNIX - Get Host Certificate and Key Filenames
 * @ingroup globus_gsi_cred_system_config_unix
 */
/* @{ */
/**
 * Get the Host Certificate and Key Filenames based on the current user's
 * environment.  The host cert and key are searched for in the following 
 * locations (in order):
 *
 * <ol>
 * <li>X509_USER_CERT and X509_USER_KEY environment variables
 * <li>registry keys x509_user_cert and x509_user_key in software\Globus\GSI
 * <li>SLANG: NOT DETERMINED - this is the default location
 * <li><GLOBUS_LOCATION>\etc\host[cert|key].pem
 * <li><users home directory>\.globus\host[cert|key].pem
 * </ol>
 * 
 * @param host_cert
 *        pointer to the host certificate filename
 * @param host_key
 *        pointer to the host key filename
 *
 * @return
 *        GLOBUS_SUCCESS if the host cert and key were found, otherwise
 *        an error object identifier is returned 
 */
globus_result_t
globus_gsi_cred_get_host_cert_filename_unix(
    char **                             host_cert,
    char **                             host_key)
{
    char *                              home = NULL;
    char *                              env_host_cert = NULL;
    char *                              env_host_key = NULL;
    char *                              default_host_cert = NULL;
    char *                              default_host_key = NULL;
    char *                              installed_host_cert = NULL;
    char *                              installed_host_key = NULL;
    char *                              local_host_cert = NULL;
    char *                              local_host_key = NULL;
    char *                              globus_location = NULL;
    globus_result_t                     result;

    static char *                       _function_name_ =
        "globus_gsi_cred_get_host_cert_filename_unix";

    *host_cert = NULL;
    *host_key = NULL;

    /* first check environment variables for valid filenames */

    if((result = globus_i_gsi_cred_create_cert_string(
                     host_cert,
                     & env_host_cert,
                     getenv(X509_USER_CERT))) != GLOBUS_SUCCESS ||
       (result = globus_i_gsi_cred_create_key_string(
                     host_key,
                     & env_host_key,
                     getenv(X509_USER_KEY))) != GLOBUS_SUCCESS)
    {
        goto error_exit;
    }

    /* now check default locations for valid filenames */
    if(!(*host_cert) || !(*host_key))
    {
        if((result = GLOBUS_I_GSI_GET_HOME_DIR(&home)) == GLOBUS_SUCCESS)
        {
            if((result = globus_i_gsi_cred_create_cert_string(
                             host_cert,
                             & default_host_cert,
                             "%s%s%s%s",
                             X509_DEFAULT_CERT_DIR,
                             FILE_SEPERATOR,
                             X509_HOST_PREFIX,
                             X509_CERT_SUFFIX)) != GLOBUS_SUCCESS ||
               (result = globus_i_gsi_cred_create_key_string(
                              host_key,
                              & default_host_key,
                              "%s%s%s%s",
                              X509_DEFAULT_CERT_DIR,
                              FILE_SEPERATOR,
                              X509_HOST_PREFIX,
                              X509_KEY_SUFFIX)) != GLOBUS_SUCCESS)
            {
                goto error_exit;
            }
        }
    }

    /* now check intstalled location for host cert */
    if(!(*host_cert) || !(*host_key))
    {
        globus_location = getenv("GLOBUS_LOCATION");

        if(globus_location)
        {
            if((result = globus_i_gsi_cred_create_cert_string(
                             host_cert,
                             & installed_host_cert,
                             "%s%s%s%s%s%s",
                             globus_location,
                             FILE_SEPERATOR,
                             X509_INSTALLED_CERT_DIR,
                             FILE_SEPERATOR,
                             X509_HOST_PREFIX,
                             X509_CERT_SUFFIX)) != GLOBUS_SUCCESS ||
               (result = globus_i_gsi_cred_create_key_string(
                             host_key,
                             & installed_host_key,
                             "%s%s%s%s%s%s",
                             globus_location,
                             FILE_SEPERATOR,
                             X509_INSTALLED_CERT_DIR,
                             FILE_SEPERATOR,
                             X509_HOST_PREFIX,
                             X509_KEY_SUFFIX)) != GLOBUS_SUCCESS)
            {
                goto error_exit;
            }
        }
    }

    if(!(*host_cert) || !(*host_key))
    {
        if(GLOBUS_I_GSI_GET_HOME_DIR(&home) == GLOBUS_SUCCESS)
        {
            if((result = globus_i_gsi_cred_create_cert_string(
                             host_cert,
                             & local_host_cert,
                             "%s%s%s%s%s%s",
                             home,
                             FILE_SEPERATOR,
                             X509_LOCAL_CERT_DIR,
                             FILE_SEPERATOR,
                             X509_HOST_PREFIX,
                             X509_CERT_SUFFIX)) != GLOBUS_SUCCESS ||
               (result = globus_i_gsi_cred_create_key_string(
                             host_key,
                             & local_host_key,
                             "%s%s%s%s%s%s",
                             home,
                             FILE_SEPERATOR,
                             X509_LOCAL_CERT_DIR,
                             FILE_SEPERATOR,
                             X509_HOST_PREFIX,
                             X509_KEY_SUFFIX)) != GLOBUS_SUCCESS)
            {
                goto error_exit;
            }
        }
    }

#ifdef DEBUG
    fprintf(stderr,"Using x509_user_cert=%s\n      x509_user_key =%s\n",
            host_cert, host_key);
#endif

    if(!(*host_cert) || !(*host_key))
    {
        result = globus_i_gsi_credential_error_result(
            GLOBUS_GSI_CRED_ERROR_SYSTEM_CONFIG,
            __FILE__,
            _function_name_,
            __LINE__,
            "The user cert could not be found in: \n"
            "1) env. var. X509_USER_CERT=%s\n"
            "2) %s\n3) %s4) %s\n\n"
            "The user key could not be found in:\n,"
            "1) env. var. X509_USER_KEY=%s\n"
            "2) %s\n3) %s4) %s\n",
            env_host_cert,
            default_host_cert,
            installed_host_cert,
            local_host_cert,
            env_host_key,
            default_host_key,
            installed_host_key,
            local_host_key);

        goto error_exit;
    }

    result = GLOBUS_SUCCESS;
    goto done;

 error_exit:

    if(*host_cert)
    {
        globus_free(*host_cert);
        *host_cert = NULL;
    }
    if(*host_key)
    {
        globus_free(*host_key);
        *host_key = NULL;
    }

 done:

    if(env_host_cert && env_host_cert != *host_cert)
    {
        globus_free(env_host_cert);
    }
    if(env_host_key && env_host_key != *host_key)
    {
        globus_free(env_host_key);
    }
    if(installed_host_cert && installed_host_cert != *host_cert)
    {
        globus_free(installed_host_cert);
    }
    if(installed_host_key && installed_host_key != *host_key)
    {
        globus_free(installed_host_key);
    }
    if(local_host_cert && local_host_cert != *host_cert)
    {
        globus_free(local_host_cert);
    }
    if(local_host_key && local_host_key != *host_key)
    {
        globus_free(local_host_key);
    }
    if(default_host_cert && default_host_cert != *host_cert)
    {
        globus_free(default_host_cert);
    }
    if(default_host_key && default_host_key != *host_key)
    {
        globus_free(default_host_key);
    }

    return result;
}
/* @} */

/**
 * UNIX - Get Service Certificate and Key Filenames
 * @ingroup globus_gsi_cred_system_config_unix
 */
/* @{ */
/**
 * Get the Service Certificate Filename based on the current user's
 * environment.  The host cert and key are searched for in the following 
 * locations (in order):
 *
 * <ol>
 * <li>X509_USER_CERT and X509_USER_KEY environment variables
 * <li>/etc/grid-security/{service_name}/{service_name}[cert|key].pem
 * <li>GLOBUS_LOCATION\etc\{service_name}\{service_name}[cert|key].pem
 *     So for example, if my service was named: myservice, the location
 *     of the certificate would be: 
 *     GLOBUS_LOCATION\etc\myservice\myservicecert.pem
 * <li><users home>\.globus\{service_name}\{service_name}[cert|key].pem
 * </ol>
 * 
 * @param service_name
 *        The name of the service which allows us to determine the
 *        locations of cert and key files to look for
 * @param service_cert
 *        pointer to the host certificate filename
 * @param service_key
 *        pointer to the host key filename
 *
 * @return
 *        GLOBUS_SUCCESS if the service cert and key were found, otherwise
 *        an error object identifier 
 */
globus_result_t
globus_gsi_cred_get_service_cert_filename_unix(
    char *                              service_name,
    char **                             service_cert,
    char **                             service_key)
{
    char *                              home = NULL;
    char *                              env_service_cert = NULL;
    char *                              env_service_key = NULL;
    char *                              default_service_cert = NULL;
    char *                              default_service_key = NULL;
    char *                              installed_service_cert = NULL;
    char *                              installed_service_key = NULL;
    char *                              local_service_cert = NULL;
    char *                              local_service_key = NULL;
    char *                              globus_location = NULL;
    globus_result_t                     result;

    static char *                       _function_name_ =
        "globus_gsi_cred_get_service_cert_filename_unix";

    *service_cert = NULL;
    *service_key = NULL;

    /* first check environment variables for valid filenames */

    if((result = globus_i_gsi_cred_create_cert_string(
                     service_cert,
                     & env_service_cert,
                     getenv(X509_USER_CERT))) != GLOBUS_SUCCESS ||
       (result = globus_i_gsi_cred_create_key_string(
                     service_key,
                     & env_service_key,
                     getenv(X509_USER_KEY))) != GLOBUS_SUCCESS)
    {
        goto error_exit;
    }

    /* now check default locations for valid filenames */
    if(!(*service_cert) || !(*service_key))
    {
        if((result = GLOBUS_I_GSI_GET_HOME_DIR(&home)) == GLOBUS_SUCCESS)
        {
            if((result = globus_i_gsi_cred_create_cert_string(
                             service_cert,
                             & default_service_cert,
                             "%s%s%s%s%s%s",
                             X509_DEFAULT_CERT_DIR,
                             FILE_SEPERATOR,
                             service_name,
                             FILE_SEPERATOR,
                             service_name,
                             X509_CERT_SUFFIX)) != GLOBUS_SUCCESS ||
               (result = globus_i_gsi_cred_create_key_string(
                              service_key,
                              & default_service_key,
                              "%s%s%s%s%s%s",
                              X509_DEFAULT_CERT_DIR,
                              FILE_SEPERATOR,
                              service_name,
                              FILE_SEPERATOR,
                              service_name,
                              X509_KEY_SUFFIX)) != GLOBUS_SUCCESS)
            {
                goto error_exit;
            }
        }
    }

    /* now check intstalled location for service cert */
    if(!(*service_cert) || !(*service_key))
    {
        globus_location = getenv("GLOBUS_LOCATION");

        if(globus_location)
        {
            if((result = globus_i_gsi_cred_create_cert_string(
                             service_cert,
                             & installed_service_cert,
                             "%s%s%s%s%s%s%s%s",
                             globus_location,
                             FILE_SEPERATOR,
                             X509_INSTALLED_CERT_DIR,
                             FILE_SEPERATOR,
                             service_name,
                             FILE_SEPERATOR,
                             service_name,
                             X509_CERT_SUFFIX)) != GLOBUS_SUCCESS ||
               (result = globus_i_gsi_cred_create_key_string(
                             service_key,
                             & installed_service_key,
                             "%s%s%s%s%s%s%s%s",
                             globus_location,
                             FILE_SEPERATOR,
                             X509_INSTALLED_CERT_DIR,
                             FILE_SEPERATOR,
                             service_name,
                             FILE_SEPERATOR,
                             service_name,
                             X509_KEY_SUFFIX)) != GLOBUS_SUCCESS)
            {
                goto error_exit;
            }
        }
    }

    if(!(*service_cert) || !(*service_key))
    {
        if(GLOBUS_I_GSI_GET_HOME_DIR(&home) == GLOBUS_SUCCESS)
        {
            if((result = globus_i_gsi_cred_create_cert_string(
                             service_cert,
                             & local_service_cert,
                             "%s%s%s%s%s%s%s",
                             home,
                             FILE_SEPERATOR,
                             X509_LOCAL_CERT_DIR,
                             FILE_SEPERATOR,
                             service_name,
                             FILE_SEPERATOR,
                             service_name,
                             X509_CERT_SUFFIX)) != GLOBUS_SUCCESS ||
               (result = globus_i_gsi_cred_create_key_string(
                             service_key,
                             & local_service_key,
                             "%s%s%s%s%s%s%s%s",
                             home,
                             FILE_SEPERATOR,
                             X509_LOCAL_CERT_DIR,
                             FILE_SEPERATOR,
                             service_name,
                             FILE_SEPERATOR,
                             service_name,
                             X509_KEY_SUFFIX)) != GLOBUS_SUCCESS)
            {
                goto error_exit;
            }
        }
    }

#ifdef DEBUG
    fprintf(stderr,"Using x509_user_cert=%s\n      x509_user_key =%s\n",
            service_cert, service_key);
#endif

    if(!(*service_cert) || !(*service_key))
    {
        result = globus_i_gsi_credential_error_result(
            GLOBUS_GSI_CRED_ERROR_SYSTEM_CONFIG,
            __FILE__,
            _function_name_,
            __LINE__,
            "The user cert could not be found in: \n"
            "1) env. var. X509_USER_CERT=%s\n"
            "2) %s\n3) %s4) %s\n\n"
            "The user key could not be found in:\n,"
            "1) env. var. X509_USER_KEY=%s\n"
            "2) %s\n3) %s4) %s\n",
            env_service_cert,
            default_service_cert,
            installed_service_cert,
            local_service_cert,
            env_service_key,
            default_service_key,
            installed_service_key,
            local_service_key);

        goto error_exit;
    }

    result = GLOBUS_SUCCESS;
    goto done;

 error_exit:

    if(*service_cert)
    {
        globus_free(*service_cert);
        *service_cert = NULL;
    }
    if(*service_key)
    {
        globus_free(*service_key);
        *service_key = NULL;
    }

 done:

    if(env_service_cert && env_service_cert != *service_cert)
    {
        globus_free(env_service_cert);
    }
    if(env_service_key && env_service_key != *service_key)
    {
        globus_free(env_service_key);
    }
    if(installed_service_cert && installed_service_cert != *service_cert)
    {
        globus_free(installed_service_cert);
    }
    if(installed_service_key && installed_service_key != *service_key)
    {
        globus_free(installed_service_key);
    }
    if(local_service_cert && local_service_cert != *service_cert)
    {
        globus_free(local_service_cert);
    }
    if(local_service_key && local_service_key != *service_key)
    {
        globus_free(local_service_key);
    }
    if(default_service_cert && default_service_cert != *service_cert)
    {
        globus_free(default_service_cert);
    }
    if(default_service_key && default_service_key != *service_key)
    {
        globus_free(default_service_key);
    }

    return result;
}
/* @} */

/**
 * UNIX - Get Proxy Filename
 * @ingroup globus_gsi_cred_operations
 */
/* @{ */
/**
 * Get the proxy cert filename based on the following
 * search order:
 * 
 * <ol>
 * <li> X509_USER_PROXY environment variable - This environment variable
 * is set by the at run time for the specific application.  If
 * the proxy_file_type variable is set to GLOBUS_PROXY_OUTPUT
 *  (a proxy filename for writing is requested), 
 * and the X509_USER_PROXY is set, this will be the 
 * resulting value of the user_proxy filename string passed in.  If the
 * proxy_file_type is set to GLOBUS_PROXY_INPUT and X509_USER_PROXY is 
 * set, but the file it points to does not exist, 
 * or has some other readability issues, the 
 * function will continue checking using the other methods available.
 * 
 * <li> Check the default location for the proxy file of /tmp/x509_u<user_id>
 * where <user id> is some unique string for that user on the host
 * </ol>
 *
 * @param user_proxy
 *        the proxy filename of the user
 *
 * @return
 *        GLOBUS_SUCCESS or an error object identifier
 */
globus_result_t
globus_gsi_cred_get_proxy_filename_unix(
    char **                             user_proxy,
    globus_gsi_proxy_file_type_t        proxy_file_type)
{
    char *                              env_user_proxy = NULL;
    char *                              env_value = NULL;
    char *                              default_user_proxy = NULL;
    globus_result_t                     result;
    char *                              user_id_string;

    static char *                       _function_name_ =
        "globus_gsi_cred_get_proxy_filename_unix";

    *user_proxy = NULL;

    if((env_value = getenv(X509_USER_PROXY)) != NULL &&
       (result = globus_i_gsi_cred_create_key_string(
                     user_proxy,
                     & env_user_proxy,
                     getenv(X509_USER_PROXY))) != GLOBUS_SUCCESS)
    {
        goto error_exit;
    }
    
    /* check if the proxy file type is for writing */
    if(!(*user_proxy) && env_user_proxy && 
       proxy_file_type == GLOBUS_PROXY_FILE_OUTPUT)
    {
        *user_proxy = env_user_proxy;
    }

    if (!user_proxy)
    {
        if((result = GLOBUS_I_GSI_GET_USER_ID_STRING(&user_id_string))
           != GLOBUS_SUCCESS)
        {
            goto error_exit;
        }
        if((result = globus_i_gsi_cred_create_key_string(
                          user_proxy,
                          & default_user_proxy,
                          "%s%s%s%s",
                          DEFAULT_SECURE_TMP_DIR,
                          FILE_SEPERATOR,
                          X509_USER_PROXY_FILE,
                          user_id_string)) != GLOBUS_SUCCESS)
        {
            goto error_exit;
        }
    }

    if(!(*user_proxy) && 
       default_user_proxy && 
       proxy_file_type == GLOBUS_PROXY_FILE_OUTPUT)
    {
        *user_proxy = default_user_proxy;
    }

    if(!(*user_proxy))
    {            
        result = globus_i_gsi_credential_error_result( 
            GLOBUS_GSI_CRED_ERROR_SYSTEM_CONFIG,
            __FILE__,
            _function_name_,
            __LINE__, 
            "A file location for%s the proxy cert could be found in: \n"
            "1) env. var. X509_USER_PROXY=%s\n"
            "2) %s\n",
            (proxy_file_type == GLOBUS_PROXY_FILE_INPUT) ? "" : " writing",
            env_user_proxy,
            default_user_proxy);
        
        goto error_exit;
    }

    result = GLOBUS_SUCCESS;
    goto done;

 error_exit:
    
    if(*user_proxy)
    {
        globus_free(*user_proxy);
        *user_proxy = NULL;
    }

 done:

    if(default_user_proxy && (default_user_proxy != (*user_proxy)))
    {
        globus_free(default_user_proxy);
    }
    
    return result;
}
/* @} */

#endif /* done defining *_unix functions */
