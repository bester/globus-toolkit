/*
 * Copyright 1999-2008 University of Chicago
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

#include "gssapi_test_utils.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>


#define NUM_CLIENTS 32

struct thread_arg
{
    gss_cred_id_t                       credential;
    int                                 fd;
    struct sockaddr_un *                address;
};

void *
server_func(
    void *                              arg);

void *
client_func(
    void *                              arg);

int
main()
{
    gss_cred_id_t                       credential;
    int                                 listen_fd;
    int                                 accept_fd;
    struct sockaddr_un *                address;
    struct thread_arg *                 arg = NULL;
    globus_thread_t                     thread_handle;
    int                                 i;
    
    globus_module_activate(GLOBUS_COMMON_MODULE);
    globus_module_activate(GLOBUS_GSI_GSSAPI_MODULE);

    /* acquire credentials */

    credential = globus_gsi_gssapi_test_acquire_credential();

    if(credential == GSS_C_NO_CREDENTIAL)
    {
	fprintf(stderr,"Unable to aquire credential\n");
	exit(-1);
    }
    
    /* setup listener */

    address = malloc(sizeof(struct sockaddr_un));

    memset(address,0,sizeof(struct sockaddr_un));

    address->sun_family = PF_UNIX;

    tmpnam(address->sun_path);
    
    listen_fd = socket(PF_UNIX, SOCK_STREAM, 0);

    bind(listen_fd, (struct sockaddr *) address, sizeof(struct sockaddr_un));

    listen(listen_fd,NUM_CLIENTS);

    /* start the clients here */

    for(i=0;i<NUM_CLIENTS;i++)
    {
	arg = malloc(sizeof(struct thread_arg));

	arg->address = address;

	arg->credential = credential;
	
	globus_thread_create(&thread_handle,NULL,client_func,(void *) arg);
    }
    
    /* accept connections */

    while(1)
    {
	accept_fd = accept(listen_fd,NULL,0);
	
	if(accept_fd < 0)
	{
	    abort();
	}

	arg = malloc(sizeof(struct thread_arg));

	arg->fd = accept_fd;

	arg->credential = credential;

	globus_thread_create(&thread_handle,NULL,server_func,(void *) arg);
    } 

    /* close the listener */

    close(listen_fd);
    
    /* release credentials */

    globus_gsi_gssapi_test_release_credential(&credential);

    /* free address */

    free(address);
    
    globus_module_deactivate(GLOBUS_COMMON_MODULE);
    globus_module_deactivate(GLOBUS_GSI_GSSAPI_MODULE);
}


void *
server_func(
    void *                              arg)
{
    struct thread_arg *                 thread_args;
    globus_bool_t                       authenticated;
    gss_ctx_id_t                        context_handle = GSS_C_NO_CONTEXT;
    char *                              user_id = NULL;
    gss_cred_id_t                       delegated_cred = GSS_C_NO_CREDENTIAL;
    gss_cred_id_t                       credential;
    
    thread_args = (struct thread_arg *) arg;

    /*    credential = globus_gsi_gssapi_test_acquire_credential(); */
    
    while(1)
    { 
	authenticated = globus_gsi_gssapi_test_authenticate(
	    thread_args->fd,
	    GLOBUS_TRUE, 
	    thread_args->credential, 
	    &context_handle, 
	    &user_id, 
	    &delegated_cred);
    

	if(authenticated == GLOBUS_FALSE)
	{
	    fprintf(stderr, "SERVER: Authentication failed\n");
	}


	globus_gsi_gssapi_test_cleanup(&context_handle,
				       user_id,
				       &delegated_cred);
    }
    
    /*    globus_gsi_gssapi_test_release_credential(&credential); */

    close(thread_args->fd);
    
    free(thread_args);
    
    globus_thread_exit(NULL);

    return NULL;
}


void *
client_func(
    void *                              arg)
{
    struct thread_arg *                 thread_args;
    globus_bool_t                       authenticated;
    gss_ctx_id_t                        context_handle = GSS_C_NO_CONTEXT;
    char *                              user_id = NULL;
    gss_cred_id_t                       delegated_cred = GSS_C_NO_CREDENTIAL;
    int                                 connect_fd;
    gss_cred_id_t                       credential;
    int                                 result = 0;
    
    thread_args = (struct thread_arg *) arg;

    /*    credential = globus_gsi_gssapi_test_acquire_credential(); */
    
    connect_fd = socket(PF_UNIX, SOCK_STREAM, 0);

    result = connect(connect_fd,
		     (struct sockaddr *) thread_args->address,
		     sizeof(struct sockaddr_un));
    
    if(result != 0)
    {
	abort();
    }

    while(1)
    {
	authenticated = globus_gsi_gssapi_test_authenticate(connect_fd,
							    GLOBUS_FALSE, 
							    thread_args->credential, 
							    &context_handle, 
							    &user_id, 
							    &delegated_cred);

	if(authenticated == GLOBUS_FALSE)
	{
	    fprintf(stderr, "CLIENT: Authentication failed\n");
	}
	

    
	globus_gsi_gssapi_test_cleanup(&context_handle,
				       user_id,
				       &delegated_cred);
	user_id = NULL;

    }

    
    /*    globus_gsi_gssapi_test_release_credential(&credential); */

    close(connect_fd);
    
    free(thread_args);
    
    globus_thread_exit(NULL);

    return NULL;
}
