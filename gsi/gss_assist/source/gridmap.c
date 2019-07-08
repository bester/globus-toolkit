#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file module.c
 * GSSAPI module activation code
 *
 * $RCSfile$
 * $Revision$
 * $Date $
 */
#endif

#include "globus_i_gss_assist.h"
#include "globus_gsi_system_config.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

typedef struct _gridmap_line_s {
  char *dn;
  char **user_ids;
} globus_i_gss_assist_gridmap_line_t;

#define WHITESPACE_CHARS		" \t\n"
#define QUOTING_CHARS			"\""
#define ESCAPING_CHARS			"\\"
#define COMMENT_CHARS			"#"
/* Characters seperating user ids in the gridmap file */
#define USERID_SEP_CHARS		","
/*
 * Characters that terminate a user id in the gridmap file. This
 * is a combination of whitespace and seperators.
 */
#define USERID_TERMINATOR_CHARS		USERID_SEP_CHARS WHITESPACE_CHARS

#ifndef NUL
#define NUL				'\0'
#endif

/*
 * Number of user id slots to allocate at a time
 * Arbitraty value, but must be >= 2.
 */
#define USERID_CHUNK_SIZE		4

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL

static 
globus_result_t
globus_i_gss_assist_gridmap_find_dn(
    const char * const                  dn,
    globus_i_gss_assist_gridmap_line_t **                   
                                        gline);

static 
globus_result_t
globus_i_gss_assist_gridmap_find_local_user(
    const char * const                  local_user,
    globus_i_gss_assist_gridmap_line_t **                   
                                        gline);

static 
globus_result_t
globus_i_gss_assist_gridmap_parse_line(
    char *                              line,
    globus_i_gss_assist_gridmap_line_t **                   
                                        gline);

static void 
globus_i_gss_assist_gridmap_line_free(
    globus_i_gss_assist_gridmap_line_t *                    
                                        line);

static
globus_result_t 
globus_i_gss_assist_gridmap_parse_globusid(
    const char *                        unparse,
    char **                             pparsed);

static int 
globus_i_gss_assist_xdigit_to_value(
    char                                xdigit);

#endif

/**
 * @name Gridmap
 * @ingroup globus_gsi_gss_assist
 */
/* @{ */
/**
 * 
 * Routines callable from globus based code to 
 * map a globusID to a local unix user
 *
 * GRIDMAP environment variable pointing at the
 * map file. Defaults to ~/.gridmap 
 *
 * A gridmap file is required if being run as root. 
 * if being run as a user,it is not required, and defaults to 
 * the current user who is running the command. 
 *
 * This is the same file used by the gssapi_cleartext
 * but will be used with other gssapi implementations which 
 * do not use the gridmap file. 
 *
 * @param globusidp
 *        the GSSAPI name from the client who requested
 *        authentication
 * @param useridp
 *        the resulting user ID name for the local system
 *
 * @return 
 *        0 on success
 *        -1 if bad arguments
 *        1 on error
 */
int 
globus_gss_assist_gridmap(
    char * 	                        globusidp,
    char ** 	                        useridp) 
{
    globus_result_t                     result = GLOBUS_SUCCESS;
    globus_i_gss_assist_gridmap_line_t *
                                        gline = NULL;

    static char *                       _function_name_ =
    "globus_gss_assist_gridmap";
    GLOBUS_I_GSI_GSS_ASSIST_DEBUG_ENTER;

    /* Check arguments */
    if ((globusidp == NULL) || (useridp == NULL))
    {
        GLOBUS_GSI_GSS_ASSIST_ERROR_RESULT(
            result,
            GLOBUS_GSI_GSS_ASSIST_ERROR_WITH_ARGUMENTS,
            ("Params passed to function are NULL"));
        goto exit;
    }

    *useridp = NULL;

    result = globus_i_gss_assist_gridmap_find_dn(globusidp, &gline);
    if(result != GLOBUS_SUCCESS)
    {
        GLOBUS_GSI_GSS_ASSIST_ERROR_CHAIN_RESULT(
            result,
            GLOBUS_GSI_GSS_ASSIST_ERROR_WITH_GRIDMAP);
        goto exit;
    }

    if (gline != NULL)
    {
	if ((gline->user_ids == NULL) ||
	    (gline->user_ids[0] == NULL))
	{
	    /*
	     * If we get here then something in this code is broken
	     * or the gridmap file is badly formatted or, most likely,
	     * both.
	     */
            GLOBUS_GSI_GSS_ASSIST_ERROR_RESULT(
                result,
                GLOBUS_GSI_GSS_ASSIST_ERROR_WITH_GRIDMAP,
                ("Invalid (NULL) user id values"));
            goto exit;
	}

	/* First user id is default */
	*useridp = strdup(gline->user_ids[0]);

	globus_i_gss_assist_gridmap_line_free(gline);

	if (*useridp == NULL)
	{
            GLOBUS_GSI_GSS_ASSIST_ERROR_RESULT(
                result,
                GLOBUS_GSI_GSS_ASSIST_ERROR_WITH_GRIDMAP,
                ("Duplicate string operation failed"));
	    goto exit;
	}
    }
    else
    {
        char *                          gridmap_filename = NULL;

        GLOBUS_GSI_SYSCONFIG_GET_GRIDMAP_FILENAME(&gridmap_filename);

	/* No entry found in gridmap file for this user */
        GLOBUS_GSI_GSS_ASSIST_ERROR_RESULT(
            result,
            GLOBUS_GSI_GSS_ASSIST_ERROR_IN_GRIDMAP_NO_USER_ENTRY,
            ("The DN: %s could not be mapped to a valid user in the "
             "gridmap file: %s.",
             globusidp,
             gridmap_filename ? gridmap_filename : "(NULL)"));

        free(gridmap_filename);
        goto exit;
    }

 exit:

    GLOBUS_I_GSI_GSS_ASSIST_DEBUG_EXIT;
    if(result == GLOBUS_SUCCESS)
    {
        return 0;
    }
    else
    {
        globus_object_t *               error_obj;
        error_obj = globus_error_get(result);
        globus_object_free(error_obj);

        return 1;
    }
} 
/* globus_gss_assist_gridmap() */
/* @} */

/**
 * @name User OK
 * @ingroup globus_gsi_gss_assist
 */
/* @{ */
/**
 * Check to see if a particular globusid is authorized to access
 * the given local user account.
 *
 * @param globusid
 *        the globus id in string form - this should be the user's subject
 * @param userid
 *        the local account that access is sought for
 *
 * @return
 *        0 on success (authorization allowed)
 *        -1 if bad arguments
 *        1 on error
 */
int
globus_gss_assist_userok(
    char *		                globusid,
    char *		                userid)
{
    char *                              gridmap_filename = NULL;
    globus_result_t                     result = GLOBUS_SUCCESS;
    globus_i_gss_assist_gridmap_line_t *			
                                        gline = NULL;
    char **				useridp;
    static char *                       _function_name_ =
        "globus_gss_assist_userok";
    GLOBUS_I_GSI_GSS_ASSIST_DEBUG_ENTER;

    /* Check arguments */
    if ((globusid == NULL) ||
	(userid == NULL))
    {
        GLOBUS_GSI_GSS_ASSIST_ERROR_RESULT(
            result,
            GLOBUS_GSI_GSS_ASSIST_ERROR_WITH_ARGUMENTS,
            ("Arguments passed to function are NULL"));
        goto exit;
    }
    
    result = globus_i_gss_assist_gridmap_find_dn(globusid, &gline);
    if(result != GLOBUS_SUCCESS)
    {
        GLOBUS_GSI_GSS_ASSIST_ERROR_CHAIN_RESULT(
            result,
            GLOBUS_GSI_GSS_ASSIST_ERROR_WITH_GRIDMAP);
        goto exit;
    }

    if (gline == NULL)
    {
        GLOBUS_GSI_GSS_ASSIST_ERROR_RESULT(
            result,
            GLOBUS_GSI_GSS_ASSIST_ERROR_IN_GRIDMAP_NO_USER_ENTRY,
            ("The DN: %s does not map to the username: %s",
             globusid,
             userid));
	goto exit;
    }
    if (gline->user_ids == NULL)
    {
        GLOBUS_GSI_GSS_ASSIST_ERROR_RESULT(
            result,
            GLOBUS_GSI_GSS_ASSIST_ERROR_WITH_GRIDMAP,
            ("The gridmap is malformated.  No user id's could be be found."));
        goto exit;
    }

    for (useridp = gline->user_ids; *useridp != NULL; useridp++)
    {
	if (strcmp(*useridp, userid) == 0)
	{
            goto exit;
	}
    }

    GLOBUS_GSI_SYSCONFIG_GET_GRIDMAP_FILENAME(&gridmap_filename);
    GLOBUS_GSI_GSS_ASSIST_ERROR_RESULT(
        result,
        GLOBUS_GSI_GSS_ASSIST_ERROR_USER_ID_DOESNT_MATCH,
        ("The user id: %s, doesn't match the the DN: %s, in the "
         "gridmap file: %s",
         globusid,
         userid,
         gridmap_filename));
    free(gridmap_filename);

 exit:

    if(gline)
    {
        globus_i_gss_assist_gridmap_line_free(gline);
    }

    GLOBUS_I_GSI_GSS_ASSIST_DEBUG_EXIT;
    if(result == GLOBUS_SUCCESS)
    {
        return 0;
    }
    else
    {
        globus_object_t *               error_obj;
        error_obj = globus_error_get(result);
        globus_object_free(error_obj);

        return 1;
    }
} 
/* globus_gss_assist_userok() */
/* @} */

/**
 * @name Map Local User
 * @ingroup 
 */
/* @{ */
/**
 * Routine for returning the default globus ID associated with
 * a local user name. This is somewhat of a hack since there is
 * not a guarenteed one-to-one mapping. What we do is look for
 * the first entry in the gridmap file that has the local
 * user as the default login.
 *
 * @param local_user
 *        the local username to find the DN for
 * @param globusidp
 *        the first DN found that reverse maps from the local_user
 *
 * @return
 *        0 on success, otherwise an error object identifier is returned.
 *        use globus_error_get to get the error object from the id.  The
 *        resulting error object must be freed using globus_object_free
 *        when it is no longer needed.
 *
 * @see globus_error_get
 * @see globus_object_free
 */
int 
globus_gss_assist_map_local_user(
    char * 	                        local_user,
    char ** 	                        globusidp) 
{
    char *                              gridmap_filename = NULL;
    globus_result_t                     result = GLOBUS_SUCCESS;
    globus_i_gss_assist_gridmap_line_t *			
                                        gline = NULL;
    static char *                       _function_name_ =
        "globus_gss_assist_map_local_user";
    GLOBUS_I_GSI_GSS_ASSIST_DEBUG_ENTER;

    /* Check arguments */
    if ((local_user == NULL) ||
	(globusidp == NULL))
    {
        GLOBUS_GSI_GSS_ASSIST_ERROR_RESULT(
            result,
            GLOBUS_GSI_GSS_ASSIST_ERROR_WITH_ARGUMENTS,
            ("Arguments passed to the function are NULL."));
        goto exit;
    }

    *globusidp = NULL;

    result = globus_i_gss_assist_gridmap_find_local_user(local_user, &gline);
    if(result != GLOBUS_SUCCESS)
    {
	/*
	 * We failed to open the gridmap file.
	 */
        GLOBUS_GSI_GSS_ASSIST_ERROR_CHAIN_RESULT(
            result,
            GLOBUS_GSI_GSS_ASSIST_ERROR_WITH_GRIDMAP);
        goto exit;
    }

    if (gline != NULL)
    {
	if (gline->dn == NULL)
	{
            GLOBUS_GSI_GSS_ASSIST_ERROR_RESULT(
                result,
                GLOBUS_GSI_GSS_ASSIST_ERROR_WITH_GRIDMAP,
                ("The gridmap file: %s is formatted incorrectly.  No "
                 "distinguished names could be found."));
            goto exit;
        }

	/* First user id is default */
	*globusidp = strdup(gline->dn);

	if (*globusidp == NULL)
	{
	    /* strdup() failed */
            GLOBUS_GSI_GSS_ASSIST_ERROR_RESULT(
                result,
                GLOBUS_GSI_GSS_ASSIST_ERROR_WITH_GRIDMAP,
                ("The string duplication operation failed."));
            goto exit;
	}
    }
    else
    {
        GLOBUS_GSI_SYSCONFIG_GET_GRIDMAP_FILENAME(&gridmap_filename);
	/* No entry found in gridmap file for this user */
        GLOBUS_GSI_GSS_ASSIST_ERROR_RESULT(
            result,
            GLOBUS_GSI_GSS_ASSIST_ERROR_IN_GRIDMAP_NO_USER_ENTRY,
            ("No DN entry found for user: %s in gridmap file: %s",
             local_user,
             gridmap_filename));
        free(gridmap_filename);
        goto exit;
    }

 exit:

    if(gline)
    {
        globus_i_gss_assist_gridmap_line_free(gline);
    }

    GLOBUS_I_GSI_GSS_ASSIST_DEBUG_EXIT;
    if(result == GLOBUS_SUCCESS)
    {
        return 0;
    }
    else
    {
        globus_object_t *               error_obj;
        error_obj = globus_error_get(result);
        globus_object_free(error_obj);

        return 1;
    }
} 
/* globus_gss_assist_map_local_user() */
/* @} */

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL

/**
 * @name Gridmap Find DN
 * @ingroup globus_i_gsi_gss_assist
 */
/* @{ */
/**
 * Locate the entry for the given DN in the default gridmap file
 *
 * @param dn
 *        the distinguished name to search for
 * @param gline
 *        gives the line information 
 *
 * @return
 *        0 on success, otherwise an error object identifier is returned.
 *        use globus_error_get to get the error object from the id.  The
 *        resulting error object must be freed using globus_object_free
 *        when it is no longer needed.
 *
 * @see globus_error_get
 * @see globus_object_free
 */
static
globus_result_t
globus_i_gss_assist_gridmap_find_dn(
    const char * const 		        dn,
    globus_i_gss_assist_gridmap_line_t **		        
                                        gline)
{
    char *                              gridmap_filename = NULL;
    globus_result_t                     result = GLOBUS_SUCCESS;
    char *				open_mode = "r";
    FILE *				gmap_stream = NULL;
    int					found = 0;
    globus_i_gss_assist_gridmap_line_t *			
                                        gline_tmp = NULL;
    static char *                       _function_name_ =
        "globus_i_gss_assist_gridmap_find_dn";
    GLOBUS_I_GSI_GSS_ASSIST_DEBUG_ENTER;


    /* Check arguments */
    if (dn == NULL)
    {
        GLOBUS_GSI_GSS_ASSIST_ERROR_RESULT(
            result,
            GLOBUS_GSI_GSS_ASSIST_ERROR_WITH_ARGUMENTS,
            ("The DN passed to function is NULL."));
	goto exit;
    }

    result = GLOBUS_GSI_SYSCONFIG_GET_GRIDMAP_FILENAME(&gridmap_filename);
    if(result != GLOBUS_SUCCESS)
    {
        GLOBUS_GSI_GSS_ASSIST_ERROR_CHAIN_RESULT(
            result,
            GLOBUS_GSI_GSS_ASSIST_ERROR_WITH_GRIDMAP);
        goto exit;
    }

    gmap_stream = fopen(gridmap_filename, open_mode);

    if (gmap_stream == NULL)
    {
        GLOBUS_GSI_GSS_ASSIST_ERROR_RESULT(
            result,
            GLOBUS_GSI_GSS_ASSIST_ERROR_WITH_GRIDMAP,
            ("Couldn't open gridmap file: %s for reading.",
             gridmap_filename));
        goto exit;
    }

    free(gridmap_filename);
    gridmap_filename = NULL;

    do
    {
	char 				line[1024];

	if (fgets(line, sizeof(line), gmap_stream) == NULL)
        {
	    break;		/* EOF or error */
        }

        result = globus_i_gss_assist_gridmap_parse_line(line, &gline_tmp);
	if (result != GLOBUS_SUCCESS)
	{
            GLOBUS_GSI_GSS_ASSIST_ERROR_CHAIN_RESULT(
                result,
                GLOBUS_GSI_GSS_ASSIST_ERROR_WITH_GRIDMAP);
            continue;		/* Parse error */
        }

	if ((gline_tmp != NULL) && (strcmp(dn, gline_tmp->dn) == 0))
	{
            found = 1;
	}
	else
	{
	    globus_i_gss_assist_gridmap_line_free(gline_tmp);
	}

    } while (!found);

    fclose(gmap_stream);
    gmap_stream = NULL;

    if (found)
    {
        *gline = gline_tmp;
    }
    else
    {
        *gline = NULL;
    }

 exit:

    if (gridmap_filename != NULL)
    {
	free(gridmap_filename);
    }

    if (gmap_stream)
    {
        fclose(gmap_stream);
    }

    GLOBUS_I_GSI_GSS_ASSIST_DEBUG_EXIT;
    return result;
} 
/* gridmap_find_dn() */
/* @} */

/**
 * @name Find Local User
 * @ingroup globus_i_gsi_gss_assist
 */
/* @{ */
/**
 * Locate the first entry with the given local user as the default in the
 * default gridmap file.
 *
 * @param local_user
 *        the name to search for
 * @param gline
 *        the resulting gridmap_line_t contianing the user and DN information
 *
 * @return
 *        0 on success, otherwise an error object identifier is returned.
 *        use globus_error_get to get the error object from the id.  The
 *        resulting error object must be freed using globus_object_free
 *        when it is no longer needed.
 *
 * @see globus_error_get
 * @see globus_object_free
 */
static
globus_result_t
globus_i_gss_assist_gridmap_find_local_user(
    const char * const	                local_user,
    globus_i_gss_assist_gridmap_line_t **	                
                                        gline)
{
    char *				gridmap_filename = NULL;
    char *				open_mode = "r";
    FILE *				gmap_stream = NULL;
    int					found = 0;
    globus_i_gss_assist_gridmap_line_t *			
                                        gline_tmp;
    globus_result_t                     result = GLOBUS_SUCCESS;
    static char *                       _function_name_ =
        "globus_i_gss_assist_gridmap_find_local_user";
    GLOBUS_I_GSI_GSS_ASSIST_DEBUG_ENTER;

    /* Check arguments */
    if (local_user == NULL)
    {
        GLOBUS_GSI_GSS_ASSIST_ERROR_RESULT(
            result,
            GLOBUS_GSI_GSS_ASSIST_ERROR_WITH_ARGUMENTS,
            ("Arguments passed to function are NULL."));
        goto exit;
    }

    result = GLOBUS_GSI_SYSCONFIG_GET_GRIDMAP_FILENAME(&gridmap_filename);
    if(result != GLOBUS_SUCCESS)
    {
        GLOBUS_GSI_GSS_ASSIST_ERROR_CHAIN_RESULT(
            result,
            GLOBUS_GSI_GSS_ASSIST_ERROR_WITH_GRIDMAP);
        goto exit;
    }
            
    gmap_stream = fopen(gridmap_filename, open_mode);

    if (gmap_stream == NULL)
    {
        GLOBUS_GSI_GSS_ASSIST_ERROR_RESULT(
            result,
            GLOBUS_GSI_GSS_ASSIST_ERROR_WITH_GRIDMAP,
            ("Can't open the file: %s", gridmap_filename));
        goto exit;
    }

    do
    {
	char 				line[1024];

	if (fgets(line, sizeof(line), gmap_stream) == NULL)
        {
	    break;		/* EOF or error */
        }

	result = globus_i_gss_assist_gridmap_parse_line(line, &gline_tmp);
        if(result != GLOBUS_SUCCESS)
        {
	    continue;		/* Parse error */
        }

	if (gline_tmp == NULL)
	{
	    /* Empty line */
	    continue;
	}

	if((gline_tmp->user_ids != NULL) &&
           (gline_tmp->user_ids[0] != NULL) &&
           (strcmp(local_user, gline_tmp->user_ids[0]) == 0))
	{
	    found = 1;
	}
	else
	{
	    globus_i_gss_assist_gridmap_line_free(gline_tmp);
	}

    } while (!found);

    fclose(gmap_stream);
    gmap_stream = NULL;

    if (found)
	*gline = gline_tmp;
    else
	*gline = NULL;

 exit:

    if (gridmap_filename)
    {
	free(gridmap_filename);
    }

    if (gmap_stream)
	fclose(gmap_stream);

    GLOBUS_I_GSI_GSS_ASSIST_DEBUG_EXIT;
    return result;
} 
/* gridmap_find_local_user() */
/* @} */

/**
 * @name Gridmap Parse Line
 * @ingroup globus_i_gsi_gss_assist
 */
/* @{ */
/**
 * 
 * Given a line from the gridmap file, parse it returning
 * a gridmap_line_t structure. line is modified during parsing.
 * The format of the line is expected to be:
 * <DN> <userid>[,<userid>[,<userid>...]]
 * Leading and trailing whitespace is ignored.
 * userids must only have a comma between them, no whitespace.
 * Anything after the userids is ignored.
 * Anything after an unescaped comment character is ignored.
 *
 * @param line
 *        the line to parse
 * @param gline
 *        the resulting parsed gridmap line structure
 *
 * @return
 *        0 on success, otherwise an error object identifier is returned.
 *        use globus_error_get to get the error object from the id.  The
 *        resulting error object must be freed using globus_object_free
 *        when it is no longer needed.
 *
 * @see globus_error_get
 * @see globus_object_free
 */
static
globus_result_t
globus_i_gss_assist_gridmap_parse_line(
    char * 			        line,
    globus_i_gss_assist_gridmap_line_t **	                
                                        gline)
{
    char *				dn_end;
    char *				parsed_dn = NULL;
    char **				userids = NULL;
    int					num_userids = 0;
    int					userid_slots = 0;
    globus_i_gss_assist_gridmap_line_t *			
                                        gline_tmp = NULL;
    globus_result_t                     result = GLOBUS_SUCCESS;
    static char *                       _function_name_ =
        "globus_i_gss_assist_gridmap_parse_line";
    GLOBUS_I_GSI_GSS_ASSIST_DEBUG_ENTER;
    
    /* Check arguments */
    if ((line == NULL) ||
	(gline == NULL))
    {
        GLOBUS_GSI_GSS_ASSIST_ERROR_RESULT(
            result,
            GLOBUS_GSI_GSS_ASSIST_ERROR_WITH_GRIDMAP,
            ("Arguments passed to function are NULL."));
	goto exit;
    }

    /* Skip over leading whitespace */
    line += strspn(line, WHITESPACE_CHARS);

    /* Check for comment at start of line and ignore line if present */
    if (strchr(COMMENT_CHARS, *line) != NULL) 
    {
	/* Ignore line, return NULL gline */
	*gline = NULL;
        goto exit;
    }
	
    /* Check for empty line */
    if (*line == NUL)
    {
	/* Empty line, return NULL gline. */
	*gline = NULL;
	goto exit;
    }

    /* Is DN quoted? */
    if (strchr(QUOTING_CHARS, *line) != NULL)
    {
	/*
	 * Yes, skip over opening quote and look for unescaped
	 * closing double quote
	 */
	line++;
	dn_end = line;

	do
	{
	    dn_end += strcspn(dn_end, QUOTING_CHARS);

	    if (*dn_end == NUL)
            {
                GLOBUS_GSI_GSS_ASSIST_ERROR_RESULT(
                    result,
                    GLOBUS_GSI_GSS_ASSIST_ERROR_INVALID_GRIDMAP_FORMAT,
                    ("A closing quote is missing in the gridmap file, "
                     "on the line:\n%s\n",
                     line));
                goto exit;
            }

	    /* Make sure it's not escaped */
	}
	while (strchr(ESCAPING_CHARS, *(dn_end - 1)) != NULL);
    }
    else
    {
	/* No, just find next whitespace */
	dn_end = line + strcspn(line, WHITESPACE_CHARS);

	if (*dn_end == NUL)
        {
            GLOBUS_GSI_GSS_ASSIST_ERROR_RESULT(
                result,
                GLOBUS_GSI_GSS_ASSIST_ERROR_INVALID_GRIDMAP_FORMAT,
                ("Nothing follows the DN on the line:\n%s\n",
                 line));
            goto exit;
        }
    }

    /* NUL terminate DN and parse */
    *dn_end = NUL;

    result = globus_i_gss_assist_gridmap_parse_globusid(line, &parsed_dn);
    if(result != GLOBUS_SUCCESS)
    {
        GLOBUS_GSI_GSS_ASSIST_ERROR_CHAIN_RESULT(
            result,
            GLOBUS_GSI_GSS_ASSIST_ERROR_WITH_GRIDMAP);
        goto exit;
    }

    /* Skip over closing delim and any whitespace after DN */
    line = dn_end + 1;
    line += strspn(line, WHITESPACE_CHARS);

    /* Parse list of unix user ID seperated by USERID_SEP_CHARS */
    while ((*line != NUL) &&
	   (strchr(WHITESPACE_CHARS, *line) == NULL))
    {
	int					userid_len;

	/* Find end of this userid */
	userid_len = strcspn(line, USERID_TERMINATOR_CHARS);

	/* Make sure we have a slot and if not allocate it */
	if ((num_userids + 1 /* new entry */+ 1 /* for NULL */) > userid_slots)
	{
	    char **userids_tmp;
	    userid_slots += USERID_CHUNK_SIZE;
	    userids_tmp = realloc(userids, userid_slots * sizeof(char *));

	    if (!userids_tmp)
            {
                result = globus_error_put(globus_error_wrap_errno_error(
                    GLOBUS_GSI_GSS_ASSIST_MODULE,
                    errno,
                    GLOBUS_GSI_GSS_ASSIST_ERROR_ERRNO,
                    "%s:%d: Could not allocate enough memory",
                    __FILE__, __LINE__));
		goto error_exit;
            }

	    userids = userids_tmp;
	}
  
	userids[num_userids] = malloc(userid_len + 1 /* for NUL */);

	if (!userids[num_userids])
        {
            result = globus_error_put(globus_error_wrap_errno_error(
                GLOBUS_GSI_GSS_ASSIST_MODULE,
                errno,
                GLOBUS_GSI_GSS_ASSIST_ERROR_ERRNO,
                "%s:%d: Could not allocate enough memory",
                __FILE__, __LINE__));
            goto error_exit;
        }

	strncpy(userids[num_userids], line, userid_len);
	userids[num_userids][userid_len] = NUL;

	num_userids++;
	userids[num_userids] = NULL;

	line += userid_len;

	/* If we're on a seperator character, skip over it */
	if (strchr(USERID_SEP_CHARS, *line) != NULL)
        {
	    line++;
        }
    }

    /*
     * There might be more stuff on the line, but we're ignoring
     * it for now.
     */

    /* Ok, build our gridmap_line_t structure */
    gline_tmp = malloc(sizeof(*gline_tmp));

    if (gline_tmp == NULL)
    {
        result = globus_error_put(globus_error_wrap_errno_error(
            GLOBUS_GSI_GSS_ASSIST_MODULE,
            errno,
            GLOBUS_GSI_GSS_ASSIST_ERROR_ERRNO,
            "%s:%d: Could not allocate enough memory",
            __FILE__, __LINE__));
        goto error_exit;
    }

    gline_tmp->dn = parsed_dn;
    gline_tmp->user_ids = userids;

    *gline = gline_tmp;
  
    goto exit;

 error_exit:

    if (parsed_dn)
    {
        free(parsed_dn);
    }

    if (userids)
    {
	char **userids_tmp = userids;

	while (*userids_tmp != NULL)
        {
            free(*userids_tmp++);
        }

	free(userids);
    }

    if (gline_tmp)
    {
        free(gline_tmp);
    }

 exit:

    GLOBUS_I_GSI_GSS_ASSIST_DEBUG_EXIT;
    return result;
} 
/* gridmap_parse_line() */
/* @} */

/**
 * @name globus_i_gsi_gss_assist
 * @ingroup globus_i_gsi_gss_assist
 */
/* @{ */
/**
 * Frees all memory allocated to a gridmap_line_t structure.
 *
 * @param gline
 *        pointer to structure to be freed.
 * 
 * @return
 *        void
 */
static
void
globus_i_gss_assist_gridmap_line_free(
    globus_i_gss_assist_gridmap_line_t *                    
                                        gline)
{
    static char *                       _function_name_ =
        "globus_i_gss_assist_gridmap_line_free";
    GLOBUS_I_GSI_GSS_ASSIST_DEBUG_ENTER;

    if (gline != NULL)
    {
        if (gline->dn != NULL)
        {
            free(gline->dn);
        }
        
        if (gline->user_ids != NULL)
        {
            char **                           userids_tmp = gline->user_ids;
            
            while (*userids_tmp != NULL)
            {
                free(*userids_tmp++);
            }
            
            free(gline->user_ids);
        }
        
        free(gline);
    }
} 
/* gridmap_free_gridmap_line() */
/* @} */

/**
 * @name Gridmap Parse Globusid
 * @ingroup globus_i_gsi_gss_assist
 */
/* @{ */
/**
 * Given a pointer to a string containing the globusid from the
 * gridmap file, return a pointer to a string containing the
 * parsed from of the id.
 *
 * Specifically handle backslashed characters - e.g. '\\',
 * '\x4a' or '\37'.
 *
 * @param unparsed
 *        the unparsed globusid
 * @param pparsed
 *        the resulting parsed string - this should be freed when
 *        no longer needed
 * @result
 *        0 on success, otherwise an error object identifier is returned.
 *        use globus_error_get to get the error object from the id.  The
 *        resulting error object must be freed using globus_object_free
 *        when it is no longer needed.
 *
 * @see globus_error_get
 * @see globus_object_free
 */
static
globus_result_t
globus_i_gss_assist_gridmap_parse_globusid(
    const char *			unparsed,
    char **				pparsed)
{
    /* Is the current character escaped? (Previous char was backslash) */
    int					escaped = 0;

  /* Buffer we are putting resulting name into */
    char *				buffer = NULL;

    /* Buffer's length in bytes */
    int					buffer_len = 0;

    /* And our current position in buffer */
    int					buffer_index = 0;

    /* Character we're currently looking at */
    char			        unparsed_char;

    globus_result_t                     result = GLOBUS_SUCCESS;
    static char *                       _function_name_ =
        "globus_i_gss_assist_gridmap_parse_globusid";
    GLOBUS_I_GSI_GSS_ASSIST_DEBUG_ENTER;

    /*
     * Check input parameters for legality
     */
    if ((unparsed == NULL) ||
	(pparsed == NULL))
    {
        GLOBUS_GSI_GSS_ASSIST_ERROR_RESULT(
            result,
            GLOBUS_GSI_GSS_ASSIST_ERROR_WITH_ARGUMENTS,
            ("Arguments passed to function are NULL."));
        goto exit;
    }

    buffer_len = strlen(unparsed);
    buffer = malloc(buffer_len);

    if (buffer == NULL)
    {
        globus_error_put(globus_error_wrap_errno_error( 
            GLOBUS_GSI_GSS_ASSIST_MODULE, 
            errno, 
            GLOBUS_GSI_GSS_ASSIST_ERROR_ERRNO, 
            "%s:%d: Could not allocate enough memory",
            __FILE__, __LINE__));
        goto exit;
    }

    /*
     * Walk through the name, parsing as we go
     */
    while ((unparsed_char = *(unparsed++)) != NUL)
    {
	/* Unescaped backslash */
	if (strchr(ESCAPING_CHARS, unparsed_char) && !escaped)
	{
	    escaped = 1;
	    continue;
	}

	/* Escaped hex character - e.g. '\xfe' */
	if ((unparsed_char == 'x') && escaped)
	{
	    if (isxdigit(*unparsed) &&
		isxdigit(*(unparsed + 1)))
	    {
		/* Set unparsed_char to value represented by hex value */
		unparsed_char =
		    (globus_i_gss_assist_xdigit_to_value(*unparsed) << 4) +
		    globus_i_gss_assist_xdigit_to_value(*(unparsed + 1));
	
		unparsed += 2;
	    }
	    /* else just fall through */
	}

	/*
         * Ok, we now have the character in unparsed_char to be appended
         * to our output string.
         *
         * First, make sure we have enough room in our output buffer.
         */

	if ((buffer_index + 1 /* for NUL */) >= buffer_len)
	{
	    /* Grow buffer */
	    char *tmp_buffer;

	    buffer_len *= 2;

	    tmp_buffer = realloc(buffer, buffer_len);

	    if (tmp_buffer == NULL)
	    {
		free(buffer);
		globus_error_put(globus_error_wrap_errno_error(
                    GLOBUS_GSI_GSS_ASSIST_MODULE,
                    errno,
                    GLOBUS_GSI_GSS_ASSIST_ERROR_ERRNO,
                    "%s:%d: Could not allocate enough memory",
                    __FILE__, __LINE__));
                goto exit;
	    }
            
	    buffer = tmp_buffer;
	}
        
	buffer[buffer_index++] = unparsed_char;
	buffer[buffer_index] = NUL;

	escaped = 0;
    }
    
    /* XXX What if escaped == 1 here? */
    /* Success */
    
    *pparsed = buffer;
    
 exit:
    
    GLOBUS_I_GSI_GSS_ASSIST_DEBUG_EXIT;
    return result;
} 
/* gridmap_parse_globusid() */
/* @} */

/**
 * @name Hexadecimal Digit to Integer
 * @ingroup globus_i_gsi_gss_assist
 */
/* @{ */
/**
 * Convert an ascii character representing a hexadecimal digit
 * into an integer.
 *
 * @param xdigit
 *        character contianing the hexidecimal digit
 *
 * @return
 *        the value in the xdigit, or -1 if error
 */
static int
globus_i_gss_assist_xdigit_to_value(
    char 				xdigit)
{
    if ((xdigit >= '0') && (xdigit <= '9'))
	return (xdigit - '0');

    if ((xdigit >= 'a') && (xdigit <= 'f'))
	return (xdigit - 'a' + 0xa);

    if ((xdigit >= 'A') && (xdigit <= 'F'))
	return (xdigit - 'A' + 0xa);

    /* Illegal digit */
    return -1;
} 
/* xdigit_to_value() */
/* @} */

#endif /* GLOBUS_DONT_DOCUMENT_INTERNAL */
