#ifndef GLOBUS_XIO_FILE_DRIVER_INCLUDE
#define GLOBUS_XIO_FILE_DRIVER_INCLUDE

#include "globus_xio_system.h"

/**
 *  possible commands for attr cntl
 */

#define GLOBUS_XIO_FILE_INVALID_HANDLE GLOBUS_XIO_SYSTEM_INVALID_HANDLE

typedef enum
{
    /**
     * handle attrs 
     */
    /* int                              mode */
    GLOBUS_XIO_FILE_SET_MODE,
    /* int *                            mode_out */
    GLOBUS_XIO_FILE_GET_MODE,
    /* int                              flags */
    GLOBUS_XIO_FILE_SET_FLAGS,
    /* int *                            flags)out */
    GLOBUS_XIO_FILE_GET_FLAGS,
    /* globus_xio_system_handle_t       handle */
    GLOBUS_XIO_FILE_SET_HANDLE,
    
    /**
     * handle attr/cntl
     */
    /* globus_xio_system_handle_t *     handle_out */
    GLOBUS_XIO_FILE_GET_HANDLE,
    
    /**
     * handle cntl
     */
    /* globus_off_t *                   in_out_offset */
    /* globus_xio_file_whence_t         whence */
    GLOBUS_XIO_FILE_SEEK
} globus_xio_file_attr_cmd_t;

typedef enum
{
    GLOBUS_XIO_FILE_CREAT               = O_CREAT,
    GLOBUS_XIO_FILE_EXCL                = O_EXCL,
    GLOBUS_XIO_FILE_RDONLY              = O_RDONLY,
    GLOBUS_XIO_FILE_WRONLY              = O_WRONLY,
    GLOBUS_XIO_FILE_RDWR                = O_RDWR,
    GLOBUS_XIO_FILE_TRUNC               = O_TRUNC,
    GLOBUS_XIO_FILE_APPEND              = O_APPEND,
#ifdef TARGET_ARCH_CYGWIN
    GLOBUS_XIO_FILE_BINARY              = O_BINARY,
    GLOBUS_XIO_FILE_TEXT                = O_TEXT
#else
    GLOBUS_XIO_FILE_BINARY              = 0,
    GLOBUS_XIO_FILE_TEXT                = 0
#endif
} globus_xio_file_flag_t;

typedef enum
{
    GLOBUS_XIO_FILE_IRWXU               = S_IRWXU,
    GLOBUS_XIO_FILE_IRUSR               = S_IRUSR,
    GLOBUS_XIO_FILE_IWUSR               = S_IWUSR,
    GLOBUS_XIO_FILE_IXUSR               = S_IXUSR,
    GLOBUS_XIO_FILE_IRWXO               = S_IRWXO,
    GLOBUS_XIO_FILE_IROTH               = S_IROTH,
    GLOBUS_XIO_FILE_IWOTH               = S_IWOTH,
    GLOBUS_XIO_FILE_IXOTH               = S_IXOTH,
    GLOBUS_XIO_FILE_IRWXG               = S_IRWXG,
    GLOBUS_XIO_FILE_IRGRP               = S_IRGRP,
    GLOBUS_XIO_FILE_IWGRP               = S_IWGRP,
    GLOBUS_XIO_FILE_IXGRP               = S_IXGRP
} globus_xio_file_mode_t;

typedef enum
{
    GLOBUS_XIO_FILE_SEEK_SET            = SEEK_SET,
    GLOBUS_XIO_FILE_SEEK_CUR            = SEEK_CUR,
    GLOBUS_XIO_FILE_SEEK_END            = SEEK_END
} globus_xio_file_whence_t;

#endif
