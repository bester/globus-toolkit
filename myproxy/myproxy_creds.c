/*
 * myproxy_creds.c
 *
 * Routines for storing and retrieving credentials.
 *
 * See myproxy_creds.h for documentation.
 */

#include "myproxy_creds.h"
#include "myproxy_server.h"

#include "verror.h"
#include "string_funcs.h"

#include "sslutil.h"

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/param.h>
#include <assert.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * Doesn't always seem to be define in <unistd.h>
 */
char *crypt(const char *key, const char *salt);

/* Files should only be readable by me */
#define FILE_MODE               0600

/**********************************************************************
 *
 * Internal variables
 *
 */

char *storage_dir = MYPROXY_SERVER_STORE_DIR;

/**********************************************************************
 *
 * Internal functions
 *
 */

/*
 * mystrdup()
 *
 * Wrapper around strdup()
 */
static char *
mystrdup(const char *string)
{
    char *dup = NULL;
    
    assert(string != NULL);
    
    dup = strdup(string);
    
    if (dup == NULL)
    {
        verror_put_errno(errno);
        verror_put_string("strdup() failed");
    }
    
    return dup;
}


/*
 * file_exists()
 *
 * Check for existance of a file.
 *
 * Returns 1 if exists, 0 if not, -1 on error.
 */
static int
file_exists(const char *path)
{
    struct stat					statbuf;
    int						return_value = -1;

    if (path == NULL)
    {
	verror_put_errno(EINVAL);
	return -1;
    }
    
    if (stat(path, &statbuf) == -1)
    {
	switch (errno)
	{
	  case ENOENT:
	  case ENOTDIR:
	    /* File does not exist */
	    return_value = 0;
	    break;
	    
	  default:
	    /* Some error */
	    return_value = -1;
	    break;
	}
    }
    else
    {
	/* File exists */
	return_value = 1;
    }
    
    return return_value;
}



/*
 * check_storage_directory()
 *
 * Check for existance and permissions on given storage directory.
 *
 * Returns 0 if ok, -1 on error.
 */
static int
check_storage_directory(const char *path)
{
    struct stat statbuf;
    int return_code = -1;
    
    
    if (stat(path, &statbuf) == -1)
    {
        verror_put_errno(errno);
        verror_put_string("could not stat directory %s", path);
        goto error;
    }
    
    if (!S_ISDIR(statbuf.st_mode))
    {
        verror_put_string("%s is not a directory", path);
        goto error;
    }
    
    /* Make sure it's owned by me */
    if (statbuf.st_uid != getuid())
    {
        verror_put_string("bad ownership on %s", path);
        goto error;
    }
    
    /* Make sure it's not readable or writable by anyone else */
    if ((statbuf.st_mode & S_IRWXG) ||
        (statbuf.st_mode & S_IRWXO))
    {
        verror_put_string("bad permissions on %s", path);
        goto error;
    }
    
    /* Success */
    return_code = 0;
    
  error:
    return return_code;
}


/*
 * sterilize_string
 *
 * Walk through a string and make sure that is it acceptable for using
 * as part of a path.
 */
void
sterilize_string(char *string)
{
    /* Characters to be removed */
    char *bad_chars = "/";
    /* Character to replace any of above characters */
    char replacement_char = '-';
    
    assert(string != NULL);
    
    /* No '.' as first character */
    if (*string == '.')
    {
        *string = replacement_char;
    }
    
    /* Replace any bad characters with replacement_char */
    while (*string != '\0')
    {
        if (strchr(bad_chars, *string) != NULL)
        {
            *string = replacement_char;
        }

        string++;
    }

    return;
}

static char *
strmd5(const char *s, unsigned char *digest)
{
    MD5_CTX md5;
    unsigned char   d[16];
    int     i;
    char mbuf[33];

    MD5_Init(&md5);
    MD5_Update(&md5,s,strlen(s));
    MD5_Final(d,&md5);

    if (digest) 
       memcpy(digest,d,sizeof(d));
    for (i=0; i<16; i++) {
       int     dd = d[i] & 0x0f;
       mbuf[2*i+1] = dd<10 ? dd+'0' : dd-10+'a';
       dd = d[i] >> 4;
       mbuf[2*i] = dd<10 ? dd+'0' : dd-10+'a';
    }
    mbuf[32] = 0;
    return mystrdup(mbuf);
}
        
    
/*
 * get_storage_locations()
 *
 * Given an user name return the path where the credentials for that
 * username should be stored and the path where data about the credentials
 * should be stored.
 *
 * Return 0 on success, -1 on error.
 */
static int
get_storage_locations(const char *username,
                      char *creds_path,
                      const int creds_path_len,
                      char *data_path,
                      const int data_path_len)
{
    int return_code = -1;
    char *sterile_username = NULL;
    const char *creds_suffix = ".creds";
    const char *data_suffix = ".data";
    
    assert(username != NULL);
    assert(creds_path != NULL);
    assert(data_path != NULL);
    assert(storage_dir != NULL);
    
    if (check_storage_directory(storage_dir) == -1)
    {
        goto error;
    }
    
    if (strchr(username, '/')) {
       sterile_username = strmd5(username, NULL);
       if (sterile_username == NULL)
	  goto error;
    } else {
       sterile_username = mystrdup(username);

       if (sterile_username == NULL)
       {
	   goto error;
       }
       
       sterilize_string(sterile_username);
    }
    
    creds_path[0] = '\0';
    
    if (concatenate_strings(creds_path, creds_path_len, storage_dir,
			    "/", sterile_username, creds_suffix, NULL) == -1)
    {
        verror_put_string("Internal error: creds_path too small: %s line %s",
                          __FILE__, __LINE__);
        goto error;
    }

    data_path[0] = '\0';
    
    if (concatenate_strings(data_path, data_path_len, storage_dir,
			    "/", sterile_username, data_suffix, NULL) == -1)
    {
        verror_put_string("Internal error: data_path too small: %s line %s",
                          __FILE__, __LINE__);
        goto error;
    }

    /* Success */
    return_code = 0;

  error:
    if (sterile_username != NULL)
    {
        free(sterile_username);
    }
    
    return return_code;
}

/*
 * copy_file()
 *
 * Copy source to destination, creating destination if needed.
 * Set permissions on destination to given mode.
 *
 * Returns 0 on success, -1 on error.
 */
static int
copy_file(const char *source,
          const char *dest,
          const mode_t mode)
{
    int src_fd = -1;
    int dst_fd = -1;
    int src_flags = O_RDONLY;
    int dst_flags = O_WRONLY | O_CREAT | O_TRUNC;
    char buffer[2048];
    int bytes_read;
    int return_code = -1;
    
    assert(source != NULL);
    assert(dest != NULL);
    
    src_fd = open(source, src_flags);
    
    if (src_fd == -1)
    {
        verror_put_errno(errno);
        verror_put_string("opening %s for reading", source);
        goto error;
    }
    
    dst_fd = open(dest, dst_flags, mode);
    
    if (dst_fd == -1)
    {
        verror_put_errno(errno);
        verror_put_string("opening %s for writing", dest);
        goto error;
    }
    
    do 
    {
        bytes_read = read(src_fd, buffer, sizeof(buffer));
        
        if (bytes_read == -1)
        {
            verror_put_errno(errno);
            verror_put_string("reading %s", source);
            goto error;
        }

        if (bytes_read != 0)
        {
            if (write(dst_fd, buffer, bytes_read) == -1)
            {
                verror_put_errno(errno);
                verror_put_string("writing %s", dest);
                goto error;
            }
        }
    }
    while (bytes_read > 0);
    
    /* Success */
    return_code = 0;
        
  error:
    if (src_fd != -1)
    {
        close(src_fd);
    }
    
    if (dst_fd != -1)
    {
        close(dst_fd);

        if (return_code == -1)
        {
            unlink(dest);
        }
    }
    
    return return_code;
}

/*
 * write_data_file()
 *
 * Write the data in the myproxy_creds() structure out the the
 * file name given, creating it if needed with the given mode.
 *
 * Returns 0 on success, -1 on error.
 */
static int
write_data_file(const struct myproxy_creds *creds,
                const char *data_file_path,
                const mode_t data_file_mode)
{
    int data_fd = -1;
    FILE *data_stream = NULL;
    int data_file_open_flags = O_WRONLY | O_CREAT | O_TRUNC;
    int return_code = -1;
        char *tmp1;
    
    /*
     * Open with open() first to minimize any race condition with
     * file permissions.
     */
    data_fd = open(data_file_path, data_file_open_flags, data_file_mode);
    
    if (data_fd == -1)
    {
        verror_put_errno(errno);
        verror_put_string("opening storage file %s", data_file_path);
        goto error;
    }

    /* Now open as stream for easier IO */
    data_stream = fdopen(data_fd, "w");
    
    if (data_stream == NULL)
    {
        verror_put_errno(errno);
        verror_put_string("reopening storage file %s", data_file_path);
        goto error;
    }

    /* Write out all the extra data associated with these credentials 
     * support for crypt() added btemko /6/16/00
     * Please, don't try to free tmp1 - crypt() uses one 
     * static string space, a la getenv()
     */
    tmp1=(char *)crypt(creds->pass_phrase, 
	&creds->owner_name[strlen(creds->owner_name)-3]);
    fprintf(data_stream, "OWNER=%s\n", creds->owner_name);
    fprintf(data_stream, "PASSPHRASE=%s\n", tmp1);
    fprintf(data_stream, "LIFETIME=%d\n", creds->lifetime);

    if (creds->retrievers != NULL)
    {
        fprintf(data_stream, "RETRIEVERS=%s\n", creds->retrievers);
    }

    if (creds->renewers != NULL)
    {	
        fprintf(data_stream, "RENEWERS=%s\n", creds->renewers);
    }

    fprintf(data_stream, "END_OPTIONS\n");

    fflush(data_stream);
    
    /* Success */
    return_code = 0;
    
  error:
    if (data_fd != -1)
    {
        close(data_fd);
        
        if (return_code == -1)
        {
            unlink(data_file_path);
        }
    }

    if (data_stream != NULL)
    {
        fclose(data_stream);
    }
    
    return return_code;
}

/*
 * read_data_file()
 *
 * Read the data contained in the given data file and fill in the
 * given creds structure.
 *
 * Returns 0 on success, -1 on error.
 */
static int
read_data_file(struct myproxy_creds *creds,
               const char *datafile_path)
{
    FILE *data_stream = NULL;
    char *data_stream_mode = "r";
    int done = 0;
    int line_number = 0;
    int return_code = -1;
    
    assert(creds != NULL);
    assert(datafile_path != NULL);
    
    data_stream = fopen(datafile_path, data_stream_mode);
    
    if (data_stream == NULL)
    {
        verror_put_errno(errno);
        verror_put_string("opening %s for reading", datafile_path);
        goto error;
    }

    while (!done)
    {
        char buffer[512];
        char *variable;
        char *value;
        int len;
        
        if (fgets(buffer, sizeof(buffer), data_stream) == NULL)
        {
            int errno_save = errno;
            
            if (feof(data_stream))
            {
                verror_put_string("unexpected EOF reading %s", datafile_path);
                goto error;
            }
            else
            {
                verror_put_errno(errno_save);
                verror_put_string("reading %s", datafile_path);
                goto error;
            }
            /* Not reached */
        }

        /* Remove carriage return from credentials */
        len = strlen(buffer);
        
        if (buffer[len - 1] == '\n')
        {
            buffer[len - 1] = '\0';
        }

        line_number++;
        
        variable = buffer;
        
        value = strchr(buffer, '=');
        
        if (value != NULL)
        {
            /* NUL-terminate variable name */
            *value = '\0';

            /* ...and advance value to point at value */
            value++;
        }

        if (strcmp(variable, "END_OPTIONS") == 0) 
        {
            done = 1;
            break;
        }
        
        /* Everything else requires values to be non-NULL */
        if (value == NULL)
        {
            verror_put_string("malformed line: %s line %d",
                              datafile_path, line_number);
            goto error;
        }
        
        if (strcmp(variable, "OWNER") == 0)
        {
            creds->owner_name = mystrdup(value);
            
            if (creds->owner_name == NULL)
            {
                goto error;
            }
            continue;
        }

        if (strcmp(variable, "PASSPHRASE") == 0)
        {
            creds->pass_phrase = mystrdup(value);
            
            if (creds->pass_phrase == NULL)
            {
                goto error;
            }
            continue;
        }
        
        if (strcmp(variable, "RETRIEVERS") == 0)
        {
            creds->retrievers = mystrdup(value);
            
            if (creds->retrievers == NULL)
            {
                goto error;
            }
            continue;
        }
        
        if (strcmp(variable, "RENEWERS") == 0)
        {
            creds->renewers = mystrdup(value);
            
            if (creds->renewers == NULL)
            {
                goto error;
            }
            continue;
        }
        
        if (strcmp(variable, "LIFETIME") == 0)
        {
            creds->lifetime = (int) strtol(value, NULL, 10);
            
            continue;
        }
        
        /* Unrecognized varibale */
        verror_put_string("unrecognized line: %s line %d",
                          datafile_path, line_number);
        goto error;
    }

    /* Success */
    return_code = 0;
    
  error:
    if (data_stream != NULL)
    {
        fclose(data_stream);
    }
    
    return return_code;
}



/**********************************************************************
 *
 * API routines
 *
 */

int
myproxy_creds_store(const struct myproxy_creds *creds)
{
    char creds_path[MAXPATHLEN] = "";
    char data_path[MAXPATHLEN] = "";
    mode_t data_file_mode = FILE_MODE;
    mode_t creds_file_mode = FILE_MODE;
    int return_code = -1;
    
    if ((creds == NULL) ||
        (creds->user_name == NULL) ||
        (creds->pass_phrase == NULL) ||
        (creds->owner_name == NULL) ||
        (creds->location == NULL) ||
        (creds->restrictions != NULL))
    {
        verror_put_errno(EINVAL);
        return -1;
    }
  
    if (get_storage_locations(creds->user_name,
                              creds_path, sizeof(creds_path),
                              data_path, sizeof(data_path)) == -1)
    {
        goto error;
    }

    /*
     * If credentials already exist for this username then we need
     * to check to make sure new credentials have the same owner.
     */

    if (write_data_file(creds, data_path, data_file_mode) == -1)
    {
        goto error;
    }

    if (copy_file(creds->location, creds_path, creds_file_mode) == -1)
    {
        goto error;
    }

    /* Success */
    return_code = 0;
    
  error:
    /* XXX */
    /* Remove files on error */
    if (return_code == -1)
    {
        unlink(data_path);
        unlink(creds_path);
    }

    return return_code;
}

int
myproxy_creds_fetch_entry(char *user_name, struct myproxy_creds *creds)
{
   char creds_path[MAXPATHLEN];
   char data_path[MAXPATHLEN];

   if (user_name == NULL || creds == NULL) {
      verror_put_errno(EINVAL);
      return -1;
   }

   if (get_storage_locations(user_name,
	                     creds_path, sizeof(creds_path),
			     data_path, sizeof(data_path)) == -1)
      return -1;

   if (read_data_file(creds, data_path) == -1)
      return -1;

   creds->user_name = mystrdup(user_name);
   creds->location = mystrdup(creds_path);
   creds->restrictions = NULL;
   return 0;
}

int
myproxy_creds_retrieve(struct myproxy_creds *creds)
{
    char creds_path[MAXPATHLEN];
    char data_path[MAXPATHLEN];
    struct myproxy_creds retrieved_creds;
    int return_code = -1;
        char *tmp1=NULL;
    
    if ((creds == NULL) ||
        (creds->user_name == NULL) ||
        (creds->pass_phrase == NULL))
    {
        verror_put_errno(EINVAL);
        return -1;
    }

    memset(&retrieved_creds, 0, sizeof(retrieved_creds));
    
    if (get_storage_locations(creds->user_name,
                              creds_path, sizeof(creds_path),
                              data_path, sizeof(data_path)) == -1)
    {
        goto error;
    }

    if (read_data_file(&retrieved_creds, data_path) == -1)
    {
	verror_put_string("can't read credentials");
        goto error;
    }

    /* Copy creds */
    if (creds->owner_name != NULL)
       free(creds->owner_name);
    if (creds->location != NULL)
       free(creds->location);
    creds->owner_name = mystrdup(retrieved_creds.owner_name);
    creds->location = mystrdup(creds_path);
    creds->lifetime = retrieved_creds.lifetime;
    creds->restrictions = NULL;
    
    if ((creds->owner_name == NULL) ||
        (creds->location == NULL))
    {
        goto error;
    }
   
    if (retrieved_creds.retrievers != NULL)
    {
       creds->retrievers = retrieved_creds.retrievers;
    }
    else
    {
       creds->retrievers = NULL;
    }

    if (retrieved_creds.renewers != NULL)
    {
       creds->renewers = retrieved_creds.renewers;
    }
    else
    {
       creds->renewers = NULL;
    }
 
    /* Success */
    return_code = 0;
    
  error:
    if (return_code < 0)
    {
        /*
         * Don't want to free user_name or pass_phrase as caller supplied
         * these.
         */
        if (creds->owner_name != NULL)
        {
            free(creds->owner_name);
            creds->owner_name = NULL;
        }
        if (creds->location != NULL)
        {
            free(creds->location);
            creds->location = NULL;
        }
        creds->lifetime = 0;
    }

    myproxy_creds_free_contents(&retrieved_creds);
    
    return return_code;
}


int
myproxy_creds_exist(const char *user_name)
{
    char creds_path[MAXPATHLEN] = "";
    char data_path[MAXPATHLEN] = "";
    int rc;

    if (user_name == NULL)
    {
	verror_put_errno(EINVAL);
	return -1;
    }

    if (get_storage_locations(user_name,
                              creds_path, sizeof(creds_path),
                              data_path, sizeof(data_path)) == -1)
    {
	return -1;
    }

    rc = file_exists(creds_path);
    
    switch(rc)
    {
      case 0:
	/* File does not exist */
	return 0;

      case 1:
	/* File exists, keep checking */
	break;
	
      case -1:
	/* Error */
	return -1;

      default:
	/* Should not be here */
	verror_put_string("file_exists(%s) return unknown value (%d)",
			  creds_path, rc);
	return -1;
    }

    rc = file_exists(data_path);
    
    switch(rc)
    {
      case 0:
	/* File does not exist */
	return 0;

      case 1:
	/* File exists, keep checking */
	break;
	
      case -1:
	/* Error */
	return -1;

      default:
	/* Should not be here */
	verror_put_string("file_exists(%s) return unknown value (%d)",
			  data_path, rc);
	return -1;
    }
    
    /* Everything seems to exist */
    
    /* XXX Should check for expiration? */

    return 1;
}

int
myproxy_creds_is_owner(const char		*username,
			const char		*client_name)
{
    char creds_path[MAXPATHLEN];
    char data_path[MAXPATHLEN];
    struct myproxy_creds retrieved_creds;
    int return_code = -1;

    memset(&retrieved_creds, 0, sizeof(retrieved_creds));

    assert(username != NULL);
    assert(client_name != NULL);
    
    if (get_storage_locations(username,
                              creds_path, sizeof(creds_path),
                              data_path, sizeof(data_path)) == -1)
    {
        goto error;
    }

    if (read_data_file(&retrieved_creds, data_path) == -1)
    {
        goto error;
    }

    if (strcmp(retrieved_creds.owner_name, client_name) == 0)
    {
	/* Is owner */
	return_code = 1;
    }
    else
    {
	/* Is not owner */
	return_code = 0;
    }
    
  error:
    myproxy_creds_free_contents(&retrieved_creds);
    
    return return_code;
}

int
myproxy_creds_delete(const struct myproxy_creds *creds)
{
    char creds_path[MAXPATHLEN];
    char data_path[MAXPATHLEN];
    struct myproxy_creds tmp_creds;
    int return_code = -1;
        char *tmp1=NULL;
    
    if ((creds == NULL) ||
        (creds->user_name == NULL))
    {
        verror_put_errno(EINVAL);
        return -1;
    }
    
    if (get_storage_locations(creds->user_name,
                              creds_path, sizeof(creds_path),
                              data_path, sizeof(data_path)) == -1)
    {
        goto error;
    }

    if (read_data_file(&tmp_creds, data_path) == -1)
    {
        goto error;
    }
    
    if (unlink(creds_path) == -1)
    {
        verror_put_errno(errno);
        verror_put_string("deleting credentials file %s", creds_path);
        goto error;
    }
    
    if (unlink(data_path) == -1)
    {
        verror_put_errno(errno);
        verror_put_string("deleting credentials data file %s", creds_path);
        goto error;
    }

    /* Success */
    return_code = 0;
    
  error:
    return return_code;
}

int
myproxy_creds_info(struct myproxy_creds *creds)
{
    char creds_path[MAXPATHLEN];
    char data_path[MAXPATHLEN];
    struct myproxy_creds tmp_creds;
    int return_code = -1;
    time_t end_time;

    if ((creds == NULL) || (creds->user_name == NULL)) {
       verror_put_errno(EINVAL);
       return -1;
    }

    if (get_storage_locations(creds->user_name,
	                      creds_path, sizeof(creds_path),
			      data_path, sizeof(data_path)) == -1) {
       goto error;
    }

    if (ssl_get_times(creds_path, &creds->start_time, &creds->end_time) != 0)
       goto error;

    return_code = 0;

error:
    return return_code;
}

void myproxy_creds_free_contents(struct myproxy_creds *creds)
{
    if (creds == NULL)
    {
        return;
    }
    
    if (creds->user_name != NULL)
    {
        free(creds->user_name);
        creds->user_name = NULL;
    }

    if (creds->pass_phrase != NULL)
    {
        free(creds->pass_phrase);
        creds->pass_phrase = NULL;
    }

    if (creds->owner_name != NULL)
    {
        free(creds->owner_name);
        creds->owner_name = NULL;
    }
    
    if (creds->location != NULL)
    {
        free(creds->location);
        creds->location = NULL;
    }
}

void myproxy_set_storage_dir(char *dir)
{ 
    storage_dir=dir;
}
