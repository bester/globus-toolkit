/******************************************************************************
globus_common_paths.c

Description:

    Install and deploy path discovery functions

CVS Information:

  $Source$
  $Date$
  $Revision$
  $State$
  $Author$
******************************************************************************/

#include "config.h"
#include "globus_common.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif

static char *  globus_l_common_install_path_services = GLOBUS_SERVICES_PREFIX;
static char *  globus_l_common_install_path_tools    = GLOBUS_TOOLS_PREFIX;


/******************************************************************************
                           ERROR object declaration

     this API returns ONE type of error object, where instance_data is a
           string with a somewhat descriptive error message.
******************************************************************************/

/* forward declarations */
char *
globus_l_common_path_error_message(globus_object_t *  error);

void
globus_l_common_path_error_copy( void *  source,
				 void ** dest );

void
globus_l_common_path_error_destruct( void * object );

static globus_object_type_t GLOBUS_COMMON_PATH_ERROR_DEFINITION
= globus_error_type_static_initializer( GLOBUS_ERROR_TYPE_BASE,
					globus_l_common_path_error_copy,
					globus_l_common_path_error_destruct,
					globus_l_common_path_error_message );


#define GLOBUS_COMMON_PATH_ERROR (&GLOBUS_COMMON_PATH_ERROR_DEFINITION)

#define GLOBUS_COMMON_PATH_ERROR_INSTANCE(errmsg) \
           globus_error_put(globus_l_common_path_error_instance(errmsg))

globus_object_t *
globus_l_common_path_error_instance(char * errmsg)
{
    globus_object_t  * d = globus_object_construct(GLOBUS_COMMON_PATH_ERROR);
    d->instance_data = globus_libc_strdup(errmsg);
    return d;
}

char *
globus_l_common_path_error_message(globus_object_t *  error)
{
    return globus_libc_strdup((char *) error->instance_data);
}

void
globus_l_common_path_error_copy( void *  source,
				 void ** dest )
{
    *dest = (void *) globus_libc_strdup( (char *) source );
}

void
globus_l_common_path_error_destruct( void * data )
{
    globus_free( data );
}



/******************************************************************************
               for now, install and deploy directories are found
                        using env vars GLOBUS_*_PATH
******************************************************************************/

globus_result_t
globus_l_common_env_path( char** bufp, char* name )
{
    char     errmsg[256];
    char *   p;

    *bufp = GLOBUS_NULL;
    p = globus_libc_getenv(name);
    if (!p || strlen(p)==0)
    {
	globus_libc_fprintf(stderr,"ERROR: %s not defined\n", name);
	globus_libc_sprintf(errmsg,"Environment variable %s is not set", name);
	return GLOBUS_COMMON_PATH_ERROR_INSTANCE(errmsg);
    }

    *bufp = globus_libc_strdup(p);
    if (! *bufp)
    {
	return GLOBUS_COMMON_PATH_ERROR_INSTANCE("malloc error");
    }
    
    return GLOBUS_SUCCESS;
}


globus_result_t
globus_common_install_path( char **   bufp )
{
#if 1
     return globus_l_common_env_path(bufp, "GLOBUS_LOCATION");
#else    
    /* NOTE: this is temporarily, to ease development. */
    globus_result_t  res;

    res = globus_l_common_env_path(bufp, "GLOBUS_LOCATION");

    if (res == GLOBUS_SUCCESS)
	return GLOBUS_SUCCESS;

    *bufp = globus_libc_strdup(globus_l_common_install_path_root);
    if (! *bufp)
    {
	return GLOBUS_COMMON_PATH_ERROR_INSTANCE("malloc error");
    }
    return GLOBUS_SUCCESS;
#endif
}

globus_result_t
globus_common_deploy_path( char **   bufp )
{
    return globus_l_common_env_path(bufp, "GLOBUS_DEPLOY_PATH");
}



/*****************************************************************************
  if we have a flat install structure (tools==services==prefix), then
  configure sets tools and services to "", otherwise the full paths.
*****************************************************************************/

globus_result_t
globus_l_common_subpath( char **  bufp, char * subpath )
{
    *bufp = GLOBUS_NULL;
    if (strlen(subpath) == 0)
    {
	return globus_common_install_path( bufp );
    }

    *bufp = globus_libc_strdup(subpath);
    if (! *bufp)
    {
	return GLOBUS_COMMON_PATH_ERROR_INSTANCE("malloc error");
    }

    return GLOBUS_SUCCESS;
}


globus_result_t
globus_common_services_path( char **   bufp )
{
    return globus_l_common_subpath(bufp,
				   globus_l_common_install_path_services);
}


globus_result_t
globus_common_tools_path( char **   bufp )
{
    return globus_l_common_subpath(bufp,
				   globus_l_common_install_path_tools);
}


/*****************************************************************************
                                 help function
  fgets() usually doesn't return the last line correctly if there's no 
  trailing \n. This function returns 1 when information was read and 0 when
  there is no more info left.

*****************************************************************************/
static int   globus_l_common_path_fgets_c = 0;

void
globus_l_common_path_fgets_init()
{
    globus_l_common_path_fgets_c = 0;
}

int
globus_l_common_path_fgets( char* buf, int bufsize, FILE* fp )
{
    int          c;
    int          n;
    
    c = globus_l_common_path_fgets_c;
    if (c==EOF)
	return 0;
    c=0;
    n=0;
    while (n<bufsize && EOF!=(c=fgetc(fp)) && c!='\n')
    {
	buf[n++] = c;
    }

    buf[n] = '\0';
    globus_l_common_path_fgets_c = c;
    
    return 1;
}


/*****************************************************************************
  processes a config file in the deploy dir and retrieves the value of
  a ATTRIBUTE=VALUE line in that file.
*****************************************************************************/

globus_result_t
globus_common_get_attribute_from_config_file( char *   deploy_path,
					      char *   file_location,
					      char *   attribute,
					      char **  value )
{
    globus_result_t   result;
    FILE *            fp;
    char *            p;
    char *            q;
    char *            deploy;
    char *            filename;
    char *            format;
    char              attr[200];    /* assumes a name won't be longer... */
    char              buf[2000];    /* assumes a line won't be longer... */
    int               attr_len;
    int               status;

    result = GLOBUS_SUCCESS;
    *value = GLOBUS_NULL;
    deploy = deploy_path;

    if (!deploy && (result=globus_common_deploy_path(&deploy)))
	return result;

    filename = globus_malloc(strlen(deploy) +
			     strlen(file_location) + 1 + 1 );
    if (!filename)
	return GLOBUS_COMMON_PATH_ERROR_INSTANCE("malloc error");
    
    globus_libc_sprintf(filename,
			"%s/%s",
			deploy,
			file_location);

    if (!deploy_path)
	globus_free(deploy);

    fp = fopen(filename,"r");
    if (!fp)
    {
	globus_libc_sprintf(buf,
			    "failed to open %s",
			    filename);
	return GLOBUS_COMMON_PATH_ERROR_INSTANCE( buf );
    }

    globus_l_common_path_fgets_init();
    p=GLOBUS_NULL;

    globus_libc_sprintf(attr, "%s=", attribute);
    attr_len = strlen(attr);

    while(!p && (status=globus_l_common_path_fgets(buf,sizeof(buf),fp)))
    {
	/* any white space? */
	q = buf;
	while (*q==' ' || *q=='\t')
	    q++;

	if (strncmp(q, attr, attr_len) == 0)
	    p = q + attr_len;
    }

    fclose(fp);
    globus_free(filename);
    if (p)
    {
	/* any enclosing quotes or trailing white space? */
	if (*p == '"')
	    ++p;

	q = p + strlen(p) - 1;  /* last char before \0 */
	while (q > p && (*q==' ' || *q=='\t' || *q=='\n' || *q=='"'))
	{
	    *q = '\0';
	    --q;
	}
    }

    if (!p || strlen(p)==0)
    {
	globus_libc_sprintf(buf,
			    "could not resolve %s from config file",
			    attribute);
	return GLOBUS_COMMON_PATH_ERROR_INSTANCE( buf );
    }

    *value = globus_libc_strdup(p);
    if (! *value)
	return GLOBUS_COMMON_PATH_ERROR_INSTANCE("malloc error");

    return GLOBUS_SUCCESS;
}

