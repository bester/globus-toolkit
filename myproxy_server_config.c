/*
 * myproxy_server_config.c
 *
 * Routines from reading and parsing the server configuration.
 *
 * See myproxy_server.h for documentation.
 */

#include "myproxy_server.h"
#include "vparse.h"
#include "verror.h"

#include <sys/param.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#if defined(HAVE_REGCOMP) && defined(HAVE_REGEX_H)
#include <regex.h>

#elif defined(HAVE_COMPILE) && defined(HAVE_REGEXPR_H)
#include <regexpr.h>

#else
#define NO_REGEX_SUPPORT

#endif

/**********************************************************************
 *
 * Internal Functions
 *
 */

/*
 * add_entry()
 *
 * Add a entry to an array of string, allocating as needed.
 */
static char **
add_entry(char **entries,
	  const char *entry)
{
    int current_length = 0;
    char **new_entries;
    char *my_entry;

    assert(entry != NULL);
    
    my_entry = strdup(entry);
    
    if (my_entry == NULL)
    {
	return NULL;
    }
    
    if (entries != NULL)
    {
	while (entries[current_length] != NULL)
	{
	    current_length++;
	}
    }
    
    new_entries = realloc(entries,
			  current_length + 2 /* New pointer and NULL */);
    
    if (new_entries == NULL)
    {
	return NULL;
    }
    
    new_entries[current_length] = my_entry;
    new_entries[current_length + 1] = NULL;
    
    return new_entries;
}

/*
 * line_parse_callback()
 *
 * Callback for vparse_stream().
 *
 * This function should return 0 unless it wants parsing to stop
 * which should only happen on fatal error - e.g. malloc() failing.
 */
static int
line_parse_callback(void *context_arg,
		    int line_number,
		    const char **tokens)
{
    myproxy_server_context_t *context = context_arg;
    const char *directive;
    int return_code = -1;
    int matched = 0;
    
    assert(context != NULL);
    
    if ((tokens == NULL) ||
	(*tokens == NULL))
    {
	/* Blank line */
	return 0;
    }

    directive = tokens[0];
    
    if (strcmp(directive, "allowed_clients") == 0)
    {
	int index = 1; /* Skip directive */
	
	matched = 1;
	
	while(tokens[index] != NULL)
	{
	    context->authorized_client_dns =
		add_entry(context->authorized_client_dns,
			  tokens[index]);
	    
	    if (context->authorized_client_dns == NULL)
	    {
		goto error;
	    }

	    index++;
	}
    }

    if (strcmp(directive, "allowed_services") == 0)
    {
	int index = 1; /* Skip directive */
	
	matched = 1;
	
	while(tokens[index] != NULL)
	{
	    context->authorized_service_dns =
		add_entry(context->authorized_service_dns,
			  tokens[index]);
	    
	    if (context->authorized_service_dns == NULL)
	    {
		goto error;
	    }

	    index++;
	}
    }
    
    if (!matched)
    {
	verror_put_string("Unrecognized directive \"%s\" on line %d of configuration file",
			  directive, line_number);
    }

    return_code = 0;
    
  error:
    return return_code;
}

/*
 * regex_compare()
 *
 * Does string match regex?
 *
 * Returns 1 if match, 0 if they don't and -1 on error setting verror.
 */
static int
regex_compare(const char *regex,
	      const char *string)
{
    int			result;

#ifndef NO_REGEX_SUPPORT
    char 		*buf;
    char		*bufp;

    /*
     * First we convert the regular expression from the human-readable
     * form (e.g. *.domain.com) to the machine-readable form
     * (e.g. ^.*\.domain\.com$).
     *
     * Make a buffer large enough to hold the largest possible converted
     * regex from the string plus our extra characters (one at the
     * begining, one at the end, plus a NULL).
     */
    buf = (char *) malloc(2 * strlen(regex) + 3);

    if (!buf)
    {
	verror_put_errno(errno);
	verror_put_string("malloc() failed");
	return -1;
    }

    bufp = buf;
    *bufp++ = '^';

    while (*regex)
    {
	switch(*regex)
	{

	case '*':
	    /* '*' turns into '.*' */
	    *bufp++ = '.';
	    *bufp++ = '*';
	    break;

	case '?':
	    /* '?' turns into '.' */
	    *bufp++ = '.';
	    break;

	    /* '.' needs to be escaped to '\.' */
	case '.':
	    *bufp++ = '\\';
	    *bufp++ = '.';
	    break;

	default:
	    *bufp++ = *regex;
	}

	regex++;
    }

    *bufp++ = '$';
    *bufp++ = '\0';

#ifdef HAVE_REGCOMP
    {
	regex_t preg;

	if (regcomp(&preg, buf, REG_EXTENDED))
	{
	    verror_put_string("Error parsing string \"%s\"",
			      regex);
	    /* Non-fatal error, just indicate failure to match */
	    result = 0;
	}
	else
	{
	    result = (regexec(&preg, string, 0, NULL, 0) == 0);
	    regfree(&preg);
	}
    }

#elif HAVE_COMPILE
    {
	char *expbuf;

	expbuf = compile(buf, NULL, NULL);

	if (!expbuf)
	{
	    verror_put_string("Error parsing string \"%s\"",
			      regex);
	    /* Non-fatal error, just indicate failure to match */
	    result = 0;

	} else {
	    result = step(string, expbuf);
	    free(expbuf);
	}
    }
#else

    /*
     * If we've gotten here then there is an error in the configuration
     * process or this file's #ifdefs
     */
    error -  No regular expression support found.

#endif

    if (buf)
	free(buf);

#else /* NOREGEX_SUPPORT */

    /* No regular expression support */
    result = (strcmp(regex, string) == 0);

#endif /* NO_REGEX_SUPPORT */

    return result;

}


/*
 * is_name_in_list()
 *
 * Is the given name in the given list of regular expressions.
 *
 * Returns 1 if it is, 0 if it isn't, -1 on error setting verror.
 */
static int
is_name_in_list(const char **list,
		const char *name)
{
    int return_code = -1;

    assert(name != NULL);
    
    if (list == NULL)
    {
	/* Empty list */
	return_code = 0;
	goto done;
    }

    while (*list != NULL)
    {
	int rc;
	
	rc = regex_compare(*list, name);
	
	if (rc != 0)
	{
	    return_code = rc;
	    goto done;
	}
	
	list++;
    }
    
    /* If we got here we failed to find the name in the list */
    return_code = 0;

  done:
    return return_code;
}



/**********************************************************************
 *
 * API Functions
 *
 */

int
myproxy_server_config_read(myproxy_server_context_t *context)
{
    char config_file[MAXPATHLEN];
    FILE *config_stream = NULL;
    const char *config_open_mode = "r";
    int rc;
    int return_code = -1;

    if (context == NULL) 
    {
	verror_put_errno(EINVAL);
	return -1;
    }
    
    if (context->config_file != NULL)
    {
	snprintf(config_file, sizeof(config_file), "%s",
		 context->config_file);
    }
    else
    {
	verror_put_string("No configuration file specified");
	goto error;
    }

    config_stream = fopen(config_file, config_open_mode);

    if (config_stream == NULL)
    {
	verror_put_errno(errno);
	verror_put_string("opening configuration file \"%s\"", config_file);
	goto error;
    }
    
    context->authorized_client_dns = NULL;
    context->authorized_service_dns = NULL;
    
    /* Clear any outstanding error */
    verror_clear();
    
    rc = vparse_stream(config_stream,
		       NULL /* Default vparse options */,
		       line_parse_callback,
		       context);
    
    if (rc == -1)
    {
	verror_put_string("Error parsing configuration file %s",
			  config_file);
	goto error;
    }

    if (verror_is_error())
    {
	/* Some sort of error occurred during parsing */
	goto error;
    }
    
    /* Success */
    return_code = 0;
    
  error:
    if (config_stream != NULL)
    {
	fclose(config_stream);
    }
    
    return return_code;
}


int
myproxy_server_check_client(myproxy_server_context_t *context,
			    const char *client_name)
{
    int return_code = -1;
    
    if ((context == NULL) ||
	(client_name == NULL))
    {
	verror_put_errno(EINVAL);
	goto error;
    }

    /* Why is this cast needed? */
    return_code = is_name_in_list((const char **) context->authorized_client_dns,
				  client_name);

  error:
    return return_code;
}

int
myproxy_server_check_service(myproxy_server_context_t *context,
			     const char *service_name)
{
    int return_code = -1;
    
    if ((context == NULL) ||
	(service_name == NULL))
    {
	verror_put_errno(EINVAL);
	goto error;
    }

    /* Why is this cast needed? */
    return_code = is_name_in_list((const char **) context->authorized_service_dns,
				  service_name);

  error:
    return return_code;
}

