/*
 * myproxy_server.h
 *
 * Myproxy server header file
 */
#ifndef __MYPROXY_SERVER_H
#define __MYPROXY_SERVER_H

/* Minimum pass phrase length */
#define MIN_PASS_PHRASE_LEN		6

extern int errno;

typedef struct 
{
  char *my_name;                 /* My name for logging and such */
  int run_as_daemon;             /* Run as a daemon? */
  char  *config_file;            /* configuration file */     
  char **accepted_credential_dns;/* List of creds that can be stored */
  char **authorized_retriever_dns;/* List of DNs we'll delegate to */
  char **default_retriever_dns;/* List of DNs we'll delegate to */
  char **authorized_renewer_dns; /* List of DNs that can renew creds */
  char **default_renewer_dns; /* List of DNs that can renew creds */
  char *dbase_name;	 	/* Credential Storage database name */
} myproxy_server_context_t;


/**********************************************************************
 *
 * Routines from myproxy_server_config.c
 *
 */

/*
 * myproxy_server_config_read()
 *
 * Read the configuration file as indicated in the context, parse
 * it and store the results in the context.
 *
 * Returns 0 on success, -1 on error setting verror.
 */
int myproxy_server_config_read(myproxy_server_context_t *context);

/*
 * myproxy_server_check_policy_list()
 *
 * Check to see if the given client matches an entry the dn_list.
 *
 * Returns 1 if match found, 0 if no match found,
 * -1 on error, setting verror.
 */
int myproxy_server_check_policy_list(const char **dn_list,
				     const char *client_name);

/*
 * myproxy_server_check_policy()
 *
 * Check to see if the given client matches the dn_regex.
 *
 * Returns 1 if match found, 0 if no match found,
 * -1 on error, setting verror.
 */
int myproxy_server_check_policy(const char *dn_regex,
				const char *client_name);

#endif /* !__MYPROXY_SERVER_H */
