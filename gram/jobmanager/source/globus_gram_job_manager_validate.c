/*
 * Copyright 1999-2009 University of Chicago
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file globus_gram_job_manager_validate.c
 *
 * RSL Validation Support for the GRAM Job Manager.
 *
 * CVS Information:
 * $Source$
 * $Date$
 * $Revision$
 * $Author$
 */

#define GLOBUS_GRAM_VALIDATE_JOB_SUBMIT_STRING \
        "GLOBUS_GRAM_JOB_SUBMIT"
#define GLOBUS_GRAM_VALIDATE_JOB_MANAGER_RESTART_STRING \
        "GLOBUS_GRAM_JOB_MANAGER_RESTART"
#define GLOBUS_GRAM_VALIDATE_STDIO_UPDATE_STRING \
        "GLOBUS_GRAM_JOB_MANAGER_STDIO_UPDATE"

#include "globus_common.h"
#include "globus_gram_job_manager.h"
#include "globus_gram_job_manager_validation.h"
#include "globus_rsl.h"
#include "globus_rsl_assist.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

static
int
globus_l_gram_job_manager_read_validation_file(
    globus_gram_job_manager_t *         manager,
    const char *                        validation_filename);

static
int
globus_l_gram_job_manager_attribute_match(
    void *                              datum,
    void *                              args);

static
globus_bool_t
globus_l_gram_job_manager_validation_string_match(
    const char *                        str1,
    const char *                        str2);

static
int
globus_l_gram_job_manager_check_rsl_attributes(
    globus_gram_jobmanager_request_t *  request,
    globus_rsl_t *                      rsl,
    globus_gram_job_manager_validation_when_t
                                        when);

static
globus_bool_t
globus_l_gram_job_manager_attribute_exists(
    globus_list_t *                     attributes,
    char *                              attribute_name);

static
int
globus_l_gram_job_manager_insert_default_rsl(
    globus_gram_jobmanager_request_t *  request,
    globus_rsl_t *                      rsl,
    globus_gram_job_manager_validation_when_t
                                        when);

static
globus_gram_job_manager_validation_record_t *
globus_l_gram_job_manager_validation_record_new(void);

static
void
globus_l_gram_job_manager_validation_record_free(
    globus_gram_job_manager_validation_record_t *
                                        record);

static
int
globus_l_gram_job_manager_validation_rsl_error(
    const char *                        attribute);

static
int
globus_l_gram_job_manager_validation_value_error(
    globus_gram_jobmanager_request_t *  request,
    const char *                        attribute,
    const char *                        value,
    const char *                        enumerated_values);

static
int
globus_l_gram_job_manager_missing_value_error(
    const char *                        attribute);

/**
 * @param request
 *        A job request. The validation field of this job request will be
 *        updated with a list of validation records constructed from the
 *        rsl validation files associated with the job manager.
 */
extern
int
globus_gram_job_manager_validation_init(
    globus_gram_job_manager_t *         manager)
{
    char *                              validation_filename;
    char *                              scheduler_validation_filename;
    int                                 rc = GLOBUS_SUCCESS;
    globus_list_t *                     tmp_list;
    globus_gram_job_manager_validation_record_t *
                                        record;

    manager->validation_records = NULL;

    validation_filename = globus_common_create_string(
            "%s/share/globus_gram_job_manager/%s.rvf",
            manager->config->globus_location,
            "globus-gram-job-manager");

    if(validation_filename == NULL)
    {
        rc = GLOBUS_GRAM_PROTOCOL_ERROR_MALLOC_FAILED;
        goto validation_filename_failed;
    }
    scheduler_validation_filename = globus_common_create_string(
            "%s/share/globus_gram_job_manager/%s.rvf",
            manager->config->globus_location,
            manager->config->jobmanager_type);
    if(scheduler_validation_filename == NULL)
    {
        rc = GLOBUS_GRAM_PROTOCOL_ERROR_MALLOC_FAILED;
        goto scheduler_validation_filename_failed;
    }

    /* Read in validation files. Do the generic job manager one first,
     * as the scheduler-specific one overrides it.
     */
    rc = globus_l_gram_job_manager_read_validation_file(
            manager,
            validation_filename);

    if(rc != GLOBUS_SUCCESS)
    {
        goto read_validation_failed;
    }

    if(access(scheduler_validation_filename, R_OK) == 0)
    {
        rc = globus_l_gram_job_manager_read_validation_file(
                manager,
                scheduler_validation_filename);
    }

    if (manager->config->log_levels & GLOBUS_GRAM_JOB_MANAGER_LOG_TRACE)
    {
        tmp_list = manager->validation_records;

        while(!globus_list_empty(tmp_list))
        {
            char * description_escaped = NULL;
            char * default_escaped = NULL;
            char * enum_escaped = NULL;

            record = globus_list_first(tmp_list);
            tmp_list = globus_list_rest(tmp_list);

            description_escaped = globus_gram_prepare_log_string(
                    record->description);
            default_escaped = globus_gram_prepare_log_string(
                    record->default_value);
            enum_escaped = globus_gram_prepare_log_string(
                    record->enumerated_values);

            globus_gram_job_manager_log(
                    manager,
                    GLOBUS_GRAM_JOB_MANAGER_LOG_TRACE,
                    "event=gram.validation_record.info "
                    "level=TRACE "
                    "attribute=%s "
                    "description=\"%s\" "
                    "required_when=%d "
                    "default_when=%d "
                    "default_value=\"%s\" "
                    "enumerated_values=\"%s\" "
                    "\n",
                    record->attribute,
                    description_escaped,
                    record->required_when,
                    record->default_when,
                    (default_escaped != NULL) ? default_escaped : "",
                    (enum_escaped != NULL) ? enum_escaped : "");
            if (description_escaped)
            {
                free(description_escaped);
            }
            if (default_escaped)
            {
                free(default_escaped);
            }
            if (enum_escaped)
            {
                free(enum_escaped);
            }
        }
    }
    if(rc != GLOBUS_SUCCESS)
    {
        while(!globus_list_empty(manager->validation_records))
        {
            globus_l_gram_job_manager_validation_record_free(
                    globus_list_remove(&manager->validation_records,
                                       manager->validation_records));
        }
        manager->validation_records = NULL;
    }

read_validation_failed:
    free(scheduler_validation_filename);
scheduler_validation_filename_failed:
    free(validation_filename);
validation_filename_failed:
    return rc;
}
/* globus_gram_job_manager_validation_init() */

int
globus_gram_job_manager_validation_destroy(
    globus_list_t *                     validation_records)
{
    globus_list_t *                     tmp;
    globus_gram_job_manager_validation_record_t *
                                        record;

    tmp = validation_records;

    while (!globus_list_empty(tmp))
    {
        record = globus_list_first(tmp);
        tmp = globus_list_rest(tmp);

        globus_l_gram_job_manager_validation_record_free(record);
    }
    globus_list_free(validation_records);

    return GLOBUS_SUCCESS;
}


/**
 * Validate a request's RSL.
 * @ingroup globus_gram_job_manager_rsl_validation
 *
 * Validate the RSL tree defining a job request, using validation files.
 * An RSL is valid if all required RSL parameters and defined in the RSL,
 * and if all RSL parameters in the RSL tree are supported by the job
 * manager and/or scheduler.
 *
 * As a side effect, the RSL will be modified to include any missing RSL
 * paramaters which have default values defined in one of the validation
 * files.
 *
 * @param request
 *        A job request. The rsl field of this job request is validated
 *        according to the rsl validation data stored in the two files
 *        passed in to this function.
 *
 * @return Returns GLOBUS_SUCCESS if the RSL is valid, and GLOBUS_FAILURE
 *         if it is not.
 * @see globus_gram_job_manager_rsl_validation_file
 */
int
globus_gram_job_manager_validate_rsl(
    globus_gram_jobmanager_request_t *  request,
    globus_rsl_t *                      rsl,
    globus_gram_job_manager_validation_when_t
                                        when)
{
    int                                        rc;

    /* First validation: RSL is a boolean "&" */
    if(!globus_rsl_is_boolean_and(rsl))
    {
        return GLOBUS_GRAM_PROTOCOL_ERROR_BAD_RSL;
    }

    /*
     * Make sure all of the attributes match defined RSL validation records.
     */
    rc = globus_l_gram_job_manager_check_rsl_attributes(
            request,
            rsl,
            when);

    if(rc != GLOBUS_SUCCESS)
    {
        goto rsl_check_failed;
    }
    /*
     * Insert default RSL values where appropriate, make sure everything
     * which is required is defined.
     */
    rc = globus_l_gram_job_manager_insert_default_rsl(
            request,
            rsl,
            when);
    if (rc != GLOBUS_SUCCESS)
    {
        goto insert_default_rsl_failed;
    }

insert_default_rsl_failed:
rsl_check_failed:
    return rc;
}
/* globus_gram_job_manager_validate_rsl() */

/**
 * Parse a validation file.
 *
 * Parse the contents of a validation file, storing validation records
 * into the list pointed to by the request's @a validation_records datum.
 * Each record consists of a set of attribute-value pairs. All except
 * "attribute" are optional. Unrecognized attributes will be ignored.
 *
 * If an attribute appears twice in a validation file, or if it appears
 * in both the job manager and the scheduler-specific validation file,
 * then the second definition will be used, completely overriding the
 * initial definition.
 *
 * @param request
 *        The request structure containing information about the
 *        scheduler we will be using, and into which the validation
 *        records will be read.
 * @param validation_filename
 *        The name of the validation file to parse.
 *
 * @note This parser isn't very strict in what it accepts. The
 * parser ignores unrecognized attibutes, so a misplaced quote could
 * it to ignore large portions of the file.
 *
 * @retval GLOBUS_GRAM_PROTOCOL_ERROR_OPENING_VALIDATION_FILE
 *         The validation file could not be opened.
 * @retval GLOBUS_GRAM_PROTOCOL_ERROR_READING_VALIDATION_FILE
 *         The validation file could not be read or parsed.
 */
static
int
globus_l_gram_job_manager_read_validation_file(
    globus_gram_job_manager_t *         manager,
    const char *                        validation_filename)
{
    FILE *                              fp;
    int                                 length;
    globus_gram_job_manager_validation_record_t *
                                        tmp = NULL;
    char *                              token_start;
    char *                              token_end;
    char *                              attribute;
    char *                              value = NULL;
    globus_list_t *                     node;
    globus_gram_job_manager_validation_record_t *
                                        old_record = NULL;
    char *                              data = NULL;
    int                                 i;
    int                                 j;
    int                                 rc = GLOBUS_SUCCESS;

    fp = fopen(validation_filename, "r");

    if(fp == NULL)
    {
        rc = GLOBUS_GRAM_PROTOCOL_ERROR_OPENING_VALIDATION_FILE;

        goto error_exit;
    }

    if(fseek(fp, 0, SEEK_END) == -1)
    {
        rc = GLOBUS_GRAM_PROTOCOL_ERROR_READING_VALIDATION_FILE;

        goto close_exit;
    }
    if((length = ftell(fp)) == -1)
    {
        rc = GLOBUS_GRAM_PROTOCOL_ERROR_READING_VALIDATION_FILE;

        goto close_exit;
    }
    if(fseek(fp, 0, SEEK_SET) == -1)
    {
        rc = GLOBUS_GRAM_PROTOCOL_ERROR_READING_VALIDATION_FILE;

        goto close_exit;
    }

    token_start = data = malloc((size_t) length + 1);

    if(token_start == NULL)
    {
        rc = GLOBUS_GRAM_PROTOCOL_ERROR_READING_VALIDATION_FILE;

        goto close_exit;
    }

    if(fread(token_start, 1, (size_t) length, fp) != length)
    {
        rc = GLOBUS_GRAM_PROTOCOL_ERROR_READING_VALIDATION_FILE;

        goto close_exit;
    }
    token_start[(size_t) length] = '\0';

    while(*token_start)
    {
        while(*token_start)
        {
            if(isspace(*token_start))
            {
                token_start++;
            }
            else if(*token_start == '#')
            {
                token_start = strchr(token_start, '\n');
            }
            else
            {
                break;
            }
        }
        token_end = strchr(token_start, ':');

        if(!token_end)
        {
            break;
        }
        attribute = malloc(token_end - token_start + 1);
        if(attribute == NULL)
        {
            rc = GLOBUS_GRAM_PROTOCOL_ERROR_READING_VALIDATION_FILE;
            goto close_exit;
        }
        memcpy(attribute, token_start, token_end - token_start);
        attribute[token_end-token_start] = '\0';
        token_start = token_end + 1; /* skip : */

        while(*token_start && isspace(*token_start))
        {
            token_start++;
        }
        if(*token_start == '"')
        {
            token_start++;

            token_end = token_start;

            do
            {
                token_end++;
                token_end = strchr(token_end, '"');
            }
            while((*token_end) && *(token_end-1) == '\\');

            value = malloc(token_end - token_start + 1);
            if(value == NULL)
            {
                rc = GLOBUS_GRAM_PROTOCOL_ERROR_READING_VALIDATION_FILE;

                goto free_attribute_exit;
            }
            for(i = 0, j = 0; token_start + i < token_end; i++)
            {
                if(token_start[i] == '\\' && token_start[i+1] == '"')
                {
                    value[j++] = token_start[++i];
                }
                else if(!(isspace(token_start[i]) && isspace(token_start[i+1])))
                {
                    value[j++] = token_start[i];
                }
            }
            value[j] = '\0';
            token_end++;

            while(*token_end && *token_end != '\n')
            {
                token_end++;
            }
            if(*token_end == '\n')
            {
                token_end++;
            }
        }
        else
        {
            token_end = strchr(token_start, '\n');
            if(token_end != NULL)
            {
                value = malloc(token_end - token_start + 1);
                if(value == NULL)
                {
                    rc = GLOBUS_GRAM_PROTOCOL_ERROR_READING_VALIDATION_FILE;

                    goto free_attribute_exit;
                }
                memcpy(value, token_start, token_end - token_start);
                value[token_end - token_start] = '\0';
                token_end++;
            }
            else
            {
                value = strdup(token_start);
                token_end = token_start + strlen(token_start);
            }
        }
        if(tmp == GLOBUS_NULL)
        {
            tmp = globus_l_gram_job_manager_validation_record_new();
            if(tmp == NULL)
            {
                rc = GLOBUS_GRAM_PROTOCOL_ERROR_READING_VALIDATION_FILE;

                goto free_value_exit;
            }
        }
        /* Compare token names against known attributes */
        if(strcasecmp(attribute, "attribute") == 0)
        {
            tmp->attribute = value;
            value = NULL;
            globus_rsl_assist_string_canonicalize(tmp->attribute);
        }
        else if(strcasecmp(attribute, "description") == 0)
        {
            tmp->description = value;
            value = NULL;
        }
        else if(strcasecmp(attribute, "requiredwhen") == 0)
        {
            tmp->required_when = 0;

            if(strstr(value, GLOBUS_GRAM_VALIDATE_JOB_SUBMIT_STRING))
            {
                tmp->required_when |= GLOBUS_GRAM_VALIDATE_JOB_SUBMIT;
            }
            if(strstr(value, GLOBUS_GRAM_VALIDATE_JOB_MANAGER_RESTART_STRING))
            {
                tmp->required_when |= GLOBUS_GRAM_VALIDATE_JOB_MANAGER_RESTART;
            }
            if(strstr(value, GLOBUS_GRAM_VALIDATE_STDIO_UPDATE_STRING))
            {
                tmp->required_when |= GLOBUS_GRAM_VALIDATE_STDIO_UPDATE;
            }
            free(value);
            value = GLOBUS_NULL;
        }
        else if(strcasecmp(attribute, "defaultwhen") == 0)
        {
            tmp->default_when = 0;

            if(strstr(value, GLOBUS_GRAM_VALIDATE_JOB_SUBMIT_STRING))
            {
                tmp->default_when |= GLOBUS_GRAM_VALIDATE_JOB_SUBMIT;
            }
            if(strstr(value, GLOBUS_GRAM_VALIDATE_JOB_MANAGER_RESTART_STRING))
            {
                tmp->default_when |= GLOBUS_GRAM_VALIDATE_JOB_MANAGER_RESTART;
            }
            if(strstr(value, GLOBUS_GRAM_VALIDATE_STDIO_UPDATE_STRING))
            {
                tmp->default_when |= GLOBUS_GRAM_VALIDATE_STDIO_UPDATE;
            }
            free(value);
            value = GLOBUS_NULL;
        }
        else if(strcasecmp(attribute, "validwhen") == 0)
        {
            tmp->valid_when = 0;

            if(strstr(value, GLOBUS_GRAM_VALIDATE_JOB_SUBMIT_STRING))
            {
                tmp->valid_when |= GLOBUS_GRAM_VALIDATE_JOB_SUBMIT;
            }
            if(strstr(value, GLOBUS_GRAM_VALIDATE_JOB_MANAGER_RESTART_STRING))
            {
                tmp->valid_when |= GLOBUS_GRAM_VALIDATE_JOB_MANAGER_RESTART;
            }
            if(strstr(value, GLOBUS_GRAM_VALIDATE_STDIO_UPDATE_STRING))
            {
                tmp->valid_when |= GLOBUS_GRAM_VALIDATE_STDIO_UPDATE;
            }
            free(value);
            value = GLOBUS_NULL;
        }
        else if(strcasecmp(attribute, "default") == 0)
        {
            tmp->default_value = value;
            value = NULL;
        }
        else if(strcasecmp(attribute, "values") == 0)
        {
            tmp->enumerated_values = value;
            value = NULL;
        }
        else if(strcasecmp(attribute, "publish") == 0)
        {
            if(strcasecmp(value, "true") == 0)
            {
                tmp->publishable = GLOBUS_TRUE;
            }
            else
            {
                tmp->publishable = GLOBUS_FALSE;
            }
            free(value);
            value = GLOBUS_NULL;
        }
        else
        {
            globus_gram_job_manager_log(
                    manager,
                    GLOBUS_GRAM_JOB_MANAGER_LOG_WARN,
                    "event=gram.validation_record.info "
                    "level=WARN "
                    "msg=\"Unknown attribute in validation file\" "
                    "path=\"%s\" "
                    "attribute=\"%s\" "
                    "value=\"%s\" "
                    "\n",
                    validation_filename,
                    attribute,
                    value);

            /* unknown attribute.... ignore */
            free(value);
            value = GLOBUS_NULL;
        }
        free(attribute);
        attribute = GLOBUS_NULL;

        token_start = token_end;

        /* Eat whitespace on end of record entry */
        while(*token_start && isspace(*token_start))
        {
            if(*token_start == '\n')
            {
                break;
            }
            else
            {
                token_start++;
            }
        }
        /* If record entry is followed by blank line or eof, then
         * store entry in list
         */
        if(*token_start == '\0' || *token_start == '\n')
        {
            node = globus_list_search_pred(
                    manager->validation_records,
                    globus_l_gram_job_manager_attribute_match,
                    tmp->attribute);

            if(node)
            {
                /*
                 * Validation record already exists; override changed
                 * values.
                 */
                old_record = globus_list_first(node);

                if(tmp->description)
                {
                    if(old_record->description)
                    {
                        free(old_record->description);
                    }
                    old_record->description = tmp->description;
                    tmp->description = NULL;
                }
                if(tmp->default_value)
                {
                    if(old_record->default_value)
                    {
                        free(old_record->default_value);
                    }
                    old_record->default_value = tmp->default_value;
                    tmp->default_value = NULL;
                }
                if(tmp->enumerated_values)
                {
                    if(old_record->enumerated_values)
                    {
                        free(old_record->enumerated_values);
                    }
                    old_record->enumerated_values = tmp->enumerated_values;
                    tmp->enumerated_values = NULL;
                }
                if(tmp->required_when != -1)
                {
                    old_record->required_when = tmp->required_when;
                }
                if(tmp->default_when != -1)
                {
                    old_record->default_when = tmp->default_when;
                }
                if(tmp->valid_when != -1)
                {
                    old_record->valid_when = tmp->valid_when;
                }
                if(tmp->publishable)
                {
                    old_record->publishable = tmp->publishable;
                }
                globus_l_gram_job_manager_validation_record_free(tmp);
                tmp = GLOBUS_NULL;
            }
            else
            {
                if(tmp->required_when == -1)
                {
                    tmp->required_when = 0;
                }
                if(tmp->default_when == -1)
                {
                    tmp->default_when = 0;
                }
                if(tmp->valid_when == -1)
                {
                    tmp->valid_when = 0;
                }
                /* Insert into validation record list */
                globus_list_insert(&manager->validation_records, tmp);
                tmp = GLOBUS_NULL;
            }
        }
    }

free_value_exit:
    if (value)
    {
        free(value);
    }
free_attribute_exit:
    if (attribute)
    {
        free(attribute);
    }
close_exit:
    fclose(fp);
error_exit:
    if(data)
    {
        free(data);
    }
    if(tmp)
    {
        globus_l_gram_job_manager_validation_record_free(tmp);
    }
    return rc;
}
/* globus_l_gram_job_manager_read_validation_file() */

/**
 * Attribute name matching search predicate.
 *
 * Compares a validation record against the desired attribute name. Used
 * as a predicate in globus_list_search_pred().
 *
 * @param datum
 *        A void * cast of a validation record.
 * @param args
 *        A void * cast of the desired attribute name.
 */
static
int
globus_l_gram_job_manager_attribute_match(
    void *                                datum,
    void *                                args)
{
    globus_gram_job_manager_validation_record_t *
                                        tmp = datum;

    return globus_l_gram_job_manager_validation_string_match(
                tmp->attribute,
                args);
}
/* globus_l_gram_job_manager_attribute_match() */

static
globus_bool_t
globus_l_gram_job_manager_validation_string_match(
    const char *                        str1,
    const char *                        str2)
{
    return (strcmp(str1, str2) == 0);
}
/* globus_l_gram_job_manager_validation_string_match() */

/**
 * Validate RSL attributes
 *
 * Checks that all of the RSL attributes in the request's RSL match
 * a validation record. If an RSL has an enumerated list of values,
 * then the value of the RSL is compared against that list.
 *
 * @param request
 *        The job request containing the RSL to validate.
 * @param when
 *        Which RSL validation time scope we will use to decide
 *        whether to use the default values or not.
 */
static
int
globus_l_gram_job_manager_check_rsl_attributes(
    globus_gram_jobmanager_request_t *  request,
    globus_rsl_t *                      rsl,
    globus_gram_job_manager_validation_when_t
                                        when)
{
    globus_list_t *                     operands;
    globus_list_t *                     node;
    globus_rsl_t *                      relation;
    char *                              attribute;
    char *                              value_str;
    globus_gram_job_manager_validation_record_t *
                                        record;
    globus_rsl_value_t *                value;
    int                                 rc = GLOBUS_SUCCESS;
    static const char *                 operation_types[] =
    {
        "??",
        "=",
        "!=",
        ">",
        ">=",
        "<",
        "<=",
        "??",
        "&",
        "|",
        "+"
    };

    operands = globus_rsl_boolean_get_operand_list(rsl);

    /* Check to make sure that every attribute is recognized by this
     * job manager.
     */
    while(!globus_list_empty(operands))
    {
        relation = globus_list_first(operands);
        operands = globus_list_rest(operands);

        if (!globus_rsl_is_relation(relation))
        {
            int operator = globus_rsl_boolean_get_operator(relation);
            if (operator > 10 || operator < 0)
            {
                operator = 0;
            }

            globus_gram_job_manager_request_log(
                    request,
                    GLOBUS_GRAM_JOB_MANAGER_LOG_ERROR,
                    "event=gram.validate_rsl.end "
                    "level=ERROR "
                    "msg=\"Required RSL relation, got boolean\" "
                    "operator=%s "
                    "status=%d "
                    "\n",
                    operation_types[operator],
                    -GLOBUS_GRAM_PROTOCOL_ERROR_BAD_RSL);
            if (request->gt3_failure_message == NULL)
            {
                request->gt3_failure_message = globus_common_create_string(
                        "Required RSL relation, got boolean %s",
                        operation_types[operator]);
            }
        }
        else if (!globus_rsl_is_relation_eq(relation))
        {
            int operator = globus_rsl_relation_get_operator(relation);

            if (operator > 10 || operator < 0)
            {
                operator = 0;
            }

            globus_gram_job_manager_request_log(
                    request,
                    GLOBUS_GRAM_JOB_MANAGER_LOG_ERROR,
                    "event=gram.validate_rsl.end "
                    "level=ERROR "
                    "msg=\"Unsupported RSL operation\" "
                    "attribute=%s "
                    "operator=%s "
                    "status=%d "
                    "\n",
                    globus_rsl_relation_get_attribute(relation),
                    operation_types[operator],
                    -GLOBUS_GRAM_PROTOCOL_ERROR_BAD_RSL);

            if (request->gt3_failure_message == NULL)
            {
                request->gt3_failure_message = globus_common_create_string(
                        "the job manager does not support the RSL operator "
                        "\"%s\" for the %s attribute",
                        operation_types[operator],
                        globus_rsl_relation_get_attribute(relation));
            }
            return GLOBUS_GRAM_PROTOCOL_ERROR_BAD_RSL;
        }
        attribute = globus_rsl_relation_get_attribute(relation);

        node = globus_list_search_pred(
                request->manager->validation_records,
                globus_l_gram_job_manager_attribute_match,
                attribute);

        if(!node)
        {
            globus_gram_job_manager_request_log(
                    request,
                    GLOBUS_GRAM_JOB_MANAGER_LOG_ERROR,
                    "event=gram.validate_rsl.end "
                    "level=ERROR "
                    "msg=\"Unsupported RSL attribute\" "
                    "attribute=%s "
                    "status=%d "
                    "\n",
                    globus_rsl_relation_get_attribute(relation),
                    -GLOBUS_GRAM_PROTOCOL_ERROR_BAD_RSL);

            if (request->gt3_failure_message == NULL)
            {
                request->gt3_failure_message = globus_common_create_string(
                        "the RSL attribute \"%s\" is not supported by the LRM adapter",
                        globus_rsl_relation_get_attribute(relation));
            }
            return GLOBUS_GRAM_PROTOCOL_ERROR_PARAMETER_NOT_SUPPORTED;
        }

        record = globus_list_first(node);

        /* Check valid_when */
        if((record->valid_when & when) == 0)
        {
            const char * whenstr = "unknown operation";

            switch(when)
            {
              case GLOBUS_GRAM_VALIDATE_JOB_SUBMIT:
                whenstr = "submit";
                rc = GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_SUBMIT_ATTRIBUTE;
                break;
              case GLOBUS_GRAM_VALIDATE_JOB_MANAGER_RESTART:
                whenstr = "restart";
                rc = GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_RESTART_ATTRIBUTE;
                break;
              case GLOBUS_GRAM_VALIDATE_STDIO_UPDATE:
                whenstr = "stdio_update";
                rc = GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_STDIO_UPDATE_ATTRIBUTE;
                break;
            }

            globus_gram_job_manager_request_log(
                    request,
                    GLOBUS_GRAM_JOB_MANAGER_LOG_ERROR,
                    "event=gram.validate_rsl.end "
                    "level=ERROR "
                    "msg=\"Invalid RSL attribute for operation\" "
                    "attribute=%s "
                    "operation=%s "
                    "status=%d "
                    "\n",
                    globus_rsl_relation_get_attribute(relation),
                    whenstr,
                    -rc);
            if (request->gt3_failure_message == NULL)
            {
                request->gt3_failure_message = globus_common_create_string(
                        "Invalid RSL attribute \"%s\" for %s",
                        globus_rsl_relation_get_attribute(relation),
                        whenstr);
            }
            return rc;
        }
        /* Check enumerated values if applicable */
        if(record->enumerated_values)
        {
            value = globus_rsl_relation_get_single_value(relation);

            if(!value)
            {
                return
                    globus_l_gram_job_manager_validation_rsl_error(attribute);
            }
            value_str = globus_rsl_value_literal_get_string(value);
            if(!value_str)
            {
                return globus_l_gram_job_manager_validation_rsl_error(
                        attribute);
            }
            if(strstr(record->enumerated_values, value_str) == GLOBUS_NULL)
            {
                rc = globus_l_gram_job_manager_validation_value_error(
                            request,
                            attribute,
                            value_str,
                            record->enumerated_values);

                globus_gram_job_manager_request_log(
                        request,
                        GLOBUS_GRAM_JOB_MANAGER_LOG_ERROR,
                        "event=gram.validate_rsl.end "
                        "level=ERROR "
                        "msg=\"RSL attribute value not in enumeration\" "
                        "attribute=%s "
                        "value=%s "
                        "enumeration=\"%s\" "
                        "status=%d "
                        "\n",
                        record->attribute,
                        value_str,
                        record->enumerated_values,
                        -rc);

                return rc;
            }
        }
    }

    return GLOBUS_SUCCESS;
}
/* globus_l_gram_job_manager_check_rsl_attributes() */

/**
 * Add default values to RSL and verify required parameters
 *
 * Inserts default values to RSL when an RSL parameter is not defined
 * in it. After this is complete, it checks that all RSL parameters
 * with the "required_when" flag set are present in the RSL tree.
 *
 * @param request
 *        Request which contains the RSL tree to validate.
 * @param when
 *        Which RSL validation time scope we will use to decide
 *        whether to use the default values or not.
 */
static
int
globus_l_gram_job_manager_insert_default_rsl(
    globus_gram_jobmanager_request_t *  request,
    globus_rsl_t *                      rsl,
    globus_gram_job_manager_validation_when_t
                                        when)
{
    globus_gram_job_manager_validation_record_t *
                                        record;
    globus_list_t **                    attributes;
    globus_rsl_t *                      new_relation;
    char *                              new_relation_str;
    globus_list_t *                     validation_records;
    int                                 rc = GLOBUS_SUCCESS;

    attributes = globus_rsl_boolean_get_operand_list_ref(rsl);

    validation_records = request->manager->validation_records;

    while(!globus_list_empty(validation_records))
    {
        record = globus_list_first(validation_records);
        validation_records = globus_list_rest(validation_records);

        if(record->default_value && (record->default_when&when))
        {
            if(!globus_l_gram_job_manager_attribute_exists(
                        *attributes,
                        record->attribute))
            {
                new_relation_str = globus_common_create_string(
                        "%s = %s",
                        record->attribute,
                        record->default_value);

                globus_gram_job_manager_request_log(
                        request,
                        GLOBUS_GRAM_JOB_MANAGER_LOG_TRACE,
                        "event=gram.validate_rsl.info "
                        "level=TRACE "
                        "msg=\"Inserting default RSL for attribute\" "
                        "attribute=%s "
                        "default=\"%s\" "
                        "\n",
                        record->attribute,
                        record->default_value);

                new_relation = globus_rsl_parse(new_relation_str);

                globus_list_insert(attributes, new_relation);

                free(new_relation_str);
            }
        }
        if(record->required_when & when)
        {
            if(!globus_l_gram_job_manager_attribute_exists(
                        *attributes,
                        record->attribute))
            {
                rc = globus_l_gram_job_manager_missing_value_error(
                            record->attribute);

                globus_gram_job_manager_request_log(
                        request,
                        GLOBUS_GRAM_JOB_MANAGER_LOG_ERROR,
                        "event=gram.validate_rsl.end "
                        "level=ERROR "
                        "msg=\"RSL missing required attribute\" "
                        "attribute=%s "
                        "\n",
                        record->attribute);

                return rc;
            }
        }
    }
    return rc;
}
/* globus_l_gram_job_manager_insert_default_rsl() */

/**
 * Check that a relation for a required RSL attribute is present.
 *
 * @param attributes
 *        List of relations which are part of the job request's
 *        RSL.
 * @param attribute_name
 *        The name of the attribute to search for.
 */
static
globus_bool_t
globus_l_gram_job_manager_attribute_exists(
    globus_list_t *                        attributes,
    char *                                attribute_name)
{
    char *                                tmp;
    globus_rsl_t *                        relation;

    while(!globus_list_empty(attributes))
    {
        relation = globus_list_first(attributes);
        attributes = globus_list_rest(attributes);
        tmp = globus_rsl_relation_get_attribute(relation);

        if(globus_l_gram_job_manager_validation_string_match(
                    tmp,
                    attribute_name))
        {
            return GLOBUS_TRUE;
        }
    }
    return GLOBUS_FALSE;
}
/* globus_l_gram_job_manager_attribute_exists() */

static
globus_gram_job_manager_validation_record_t *
globus_l_gram_job_manager_validation_record_new(void)
{
    globus_gram_job_manager_validation_record_t *
                                        tmp;

    tmp = malloc(
            sizeof(globus_gram_job_manager_validation_record_t));

    tmp->attribute = NULL;
    tmp->description = NULL;
    tmp->default_value = NULL;
    tmp->enumerated_values = NULL;
    tmp->required_when = -1;
    tmp->default_when = -1;
    tmp->valid_when = -1;
    tmp->publishable = GLOBUS_TRUE;

    return tmp;
}
/* globus_l_gram_job_manager_validation_record_new() */

/**
 * Free a validation record
 *
 * Frees all strings referenced by the validation record, and 
 * then frees the record itself.
 *
 * @param record
 *        The record to free.
 */
static
void
globus_l_gram_job_manager_validation_record_free(
    globus_gram_job_manager_validation_record_t *
                                            record)
{
    if(!record)
    {
        return;
    }
    if(record->attribute)
    {
        free(record->attribute);
    }
    if(record->description)
    {
        free(record->description);
    }
    if(record->default_value)
    {
        free(record->default_value);
    }
    if(record->enumerated_values)
    {
        free(record->enumerated_values);
    }
    free(record);
}
/* globus_l_gram_job_manager_validation_record_free() */

#define HANDLE_RSL_ERROR(param,error) \
    if(globus_l_gram_job_manager_validation_string_match( \
                attribute, param)) \
    { \
        return error; \
    }

/**
 * Decide what type of RSL error to return when the value of @a attribute.
 * is not of the appropriate type.
 *
 * @param attribute
 *        Attribute to check.
 *
 * @note This should go away when we have better error reporting in the
 *       GRAM protocol.
 */
static
int
globus_l_gram_job_manager_validation_rsl_error(
    const char *                        attribute)
{
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_ARGUMENTS_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_ARGUMENTS)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_COUNT_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_COUNT)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_DIR_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_DIRECTORY)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_DRY_RUN_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_DRYRUN)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_ENVIRONMENT_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_ENVIRONMENT)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_EXECUTABLE_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_EXECUTABLE)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_FILE_CLEANUP_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_FILE_CLEANUP)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_FILE_STAGE_IN_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_FILE_STAGE_IN)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_FILE_STAGE_IN_SHARED_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_FILE_STAGE_IN_SHARED)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_FILE_STAGE_OUT_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_FILE_STAGE_OUT)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_GASS_CACHE_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_GASS_CACHE)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_MYJOB_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_MYJOB)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_HOST_COUNT_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_HOST_COUNT)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_JOB_TYPE_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_JOBTYPE)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_MAX_CPU_TIME_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_MAX_CPU_TIME)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_MAX_MEMORY_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_MAX_MEMORY)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_MAX_TIME_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_MAXTIME)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_MAX_WALL_TIME_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_MAX_WALL_TIME)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_MIN_MEMORY_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_MIN_MEMORY)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_PROJECT_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_PROJECT)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_QUEUE_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_QUEUE)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_REMOTE_IO_URL_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_REMOTE_IO_URL)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_RESTART_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_RESTART)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_SAVE_STATE_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_SAVE_STATE)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_SCRATCHDIR_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_SCRATCH)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_STDERR_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_STDERR)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_STDERR_POSITION_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_STDERR_POSITION)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_STDIN_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_STDIN)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_STDOUT_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_STDOUT)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_STDOUT_POSITION_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_STDOUT_POSITION)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_TWO_PHASE_COMMIT_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_TWO_PHASE_COMMIT)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_PROXY_TIMEOUT_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_RSL_PROXY_TIMEOUT)

    return GLOBUS_GRAM_PROTOCOL_ERROR_RSL_SCHEDULER_SPECIFIC;
}
/* globus_l_gram_job_manager_validation_rsl_error() */

static
int
globus_l_gram_job_manager_validation_value_error(
    globus_gram_jobmanager_request_t *  request,
    const char *                        attribute,
    const char *                        value,
    const char *                        enumerated_values)
{
    if (request->gt3_failure_message == NULL)
    {
        request->gt3_failure_message = globus_common_create_string(
                "RSL attribute \"%s\" has value \"%s\" which is not one of the allowed values (%s)",
                attribute,
                value,
                enumerated_values);
    }

    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_COUNT_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_COUNT)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_MYJOB_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_GRAM_MYJOB)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_HOST_COUNT_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_HOST_COUNT)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_JOB_TYPE_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_JOBTYPE)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_MAX_CPU_TIME_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_MAX_CPU_TIME)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_MAX_MEMORY_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_MAX_MEMORY)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_MAX_TIME_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_MAXTIME)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_MAX_WALL_TIME_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_MAX_WALL_TIME)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_MIN_MEMORY_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_MIN_MEMORY)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_PROJECT_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_PROJECT)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_QUEUE_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_QUEUE)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_SAVE_STATE_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_SAVE_STATE)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_SCRATCHDIR_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_SCRATCH)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_STDERR_POSITION_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_STDERR_POSITION)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_STDOUT_POSITION_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_STDOUT_POSITION)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_TWO_PHASE_COMMIT_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_TWO_PHASE_COMMIT)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_PROXY_TIMEOUT_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_PROXY_TIMEOUT)

    return GLOBUS_GRAM_PROTOCOL_ERROR_RSL_SCHEDULER_SPECIFIC;
}
/* globus_l_gram_job_manager_validation_value_error() */

static
int
globus_l_gram_job_manager_missing_value_error(
    const char *                        attribute)
{
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_EXECUTABLE_PARAM,
                     GLOBUS_GRAM_PROTOCOL_ERROR_UNDEFINED_EXE)

    return GLOBUS_GRAM_PROTOCOL_ERROR_UNDEFINED_ATTRIBUTE;
}
/* globus_l_gram_job_manager_missing_value_error() */

#endif /* !GLOBUS_DONT_DOCUMENT_INTERNAL */
