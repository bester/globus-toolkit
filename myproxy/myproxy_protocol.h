/*
 *
 * MyProxy protocol API
 *
 */
#ifndef __MYPROXY_PROTOCOL_H
#define __MYPROXY_PROTOCOL_H

/* Protocol commands */
typedef enum
{
    MYPROXY_GET_PROXY,
    MYPROXY_PUT_PROXY,
    MYPROXY_INFO_PROXY,
    MYPROXY_DESTROY_PROXY,
    MYPROXY_CHANGE_CRED_PASSPHRASE,
    MYPROXY_STORE_CERT,
    MYPROXY_RETRIEVE_CERT,
    MYPROXY_CONTINUE
} myproxy_proto_request_type_t;

/* server response codes */
typedef enum
{
    MYPROXY_OK_RESPONSE,
    MYPROXY_ERROR_RESPONSE,
    MYPROXY_AUTHORIZATION_RESPONSE
} myproxy_proto_response_type_t;

/* client/server socket attributes */
typedef struct 
{
  char *pshost;	
  int psport;
  int socket_fd;
  struct _gsi_socket *gsi_socket; 
} myproxy_socket_attrs_t;

/* A client request object */
#define REGULAR_EXP 1
#define MATCH_CN_ONLY 0

typedef struct
{
    char                         *version;
    char                         *username;
    char                         passphrase[MAX_PASS_LEN+1];
    char                         new_passphrase[MAX_PASS_LEN+1];
    myproxy_proto_request_type_t command_type;
    int                          proxy_lifetime;
    char                         *retrievers;
    char                         *renewers;
    char			 *credname;
    char			 *creddesc;
    char			 *authzcreds;
    char 		         *keyretrieve;
} myproxy_request_t;

/* A server response object */
typedef struct
{
  char                          *version;
  myproxy_proto_response_type_t response_type;
  authorization_data_t		**authorization_data;
  char				*error_string;
  myproxy_creds_t		*info_creds;
} myproxy_response_t;

  
/*
 * myproxy_init_client()
 *
 * Create a generic client by creating a GSI socket and connecting to a a host 
 *
 * returns the file descriptor of the connected socket or
 *   -1 if an error occurred
 */
int myproxy_init_client(myproxy_socket_attrs_t *attrs);

/*
 * myproxy_authenticate_init()
 * 
 * Perform client-side authentication
 *
 * returns -1 if unable to authenticate, 0 if authentication successful
 */ 
int myproxy_authenticate_init(myproxy_socket_attrs_t *attr,
			      const char *proxyfile);

/*
 * myproxy_authenticate_accept()
 * 
 * Perform server-side authentication and retrieve the client's DN
 *
 * returns -1 if unable to authenticate, 0 if authentication successful
 */ 
int myproxy_authenticate_accept(myproxy_socket_attrs_t *attr, 
                                char *client_name, const int namelen);

/*
 * myproxy_serialize_request()
 * 
 * Serialize a request object into a buffer to be sent over the network.
 *
 * returns the number of characters put into the buffer 
 * (not including the trailing NULL)
 */
int myproxy_serialize_request(const myproxy_request_t *request, 
			      char *data, const int datalen);


/*
 * myproxy_deserialize_request()
 * 
 * Deserialize a buffer into a request object.
 *
 * returns 0 if succesful, otherwise -1
 */
int myproxy_deserialize_request(const char *data, const int datalen, 
				myproxy_request_t *request);

/*
 * myproxy_serialize_response()
 * 
 * Serialize a response object into a buffer to be sent over the network.
 *
 * returns the number of characters put into the buffer 
 * (not including the trailing NULL)
 */
int
myproxy_serialize_response(const myproxy_response_t *response, 
                           char *data, const int datalen); 

/*
 * myproxy_deserialize_response()
 *
 * Serialize a response object into a buffer to be sent over the network.
 *
 * returns the number of characters put into the buffer 
 * (not including the trailing NULL)
 */
int myproxy_deserialize_response(myproxy_response_t *response, 
				 const char *data, const int datalen);

/*
 * myproxy_send()
 * 
 * Sends a buffer
 *
 * returns 0 on success, -1 on error
 */
int myproxy_send(myproxy_socket_attrs_t *attrs,
                 const char *data, const int datalen);

/*
 * myproxy_recv()
 *
 * Receives a message into the buffer.
 * Use myproxy_recv_ex() instead.
 *
 * returns bytes read on success, -1 on error, -2 on truncated response
 * 
 */
int myproxy_recv(myproxy_socket_attrs_t *attrs,
		 char *data, const int datalen);

/*
 * myproxy_recv_ex()
 *
 * Receives a message into a newly allocated buffer of correct size.
 * The caller must deallocate the buffer.
 *
 * returns bytes read on success, -1 on error
 * 
 */
int myproxy_recv_ex(myproxy_socket_attrs_t *attrs, char **data);

/*
 * myproxy_init_delegation()
 *
 * Delegates a proxy based on the credentials found in file 
 * location delegfile good for lifetime_seconds
 *
 * returns 0 on success, -1 on error 
 */
int myproxy_init_delegation(myproxy_socket_attrs_t *attrs,
			    const char *delegfile,
			    const int lifetime_seconds,
			    char *passphrase);

/*
 * myproxy_accept_delegation()
 *
 * Accepts delegated credentials into file location data
 *
 * returns 0 on success, -1 on error 
 */
int myproxy_accept_delegation(myproxy_socket_attrs_t *attrs, char *data,
			      const int datalen, char *passphrase);

/*
 * myproxy_accept_credentials()
 *
 * Accepts credentials into file location data
 *
 * returns 0 on success, -1 on error
 */
int
myproxy_accept_credentials(myproxy_socket_attrs_t *attrs,
                           char                   *delegfile,
                           int                     delegfile_len,
                           char                   *passphrase);

/*
 * myproxy_init_credentials()
 *
 * returns 0 on success, -1 on error 
 */
int
myproxy_init_credentials(myproxy_socket_attrs_t *attrs,
                         const char             *delegfile,
                         const int               lifetime,
                         char                   *passphrase);

int
myproxy_get_credentials(myproxy_socket_attrs_t *attrs,
                         const char             *delegfile,
                         const int               lifetime,
                         char                   *passphrase);

/*
 * myproxy_free()
 * 
 * Frees up memory used for creating request, response and socket objects 
 */
void myproxy_free(myproxy_socket_attrs_t *attrs, myproxy_request_t *request,
		  myproxy_response_t *response);

/*
 * myproxy_recv_response()
 *
 * Helper function that combines myproxy_recv() and
 * myproxy_deserialize_response() with some error checking.
 *
 */
int myproxy_recv_response(myproxy_socket_attrs_t *attrs,
			  myproxy_response_t *response); 

/*
 * myproxy_recv_response_ex()
 *
 * Helper function that combines myproxy_recv(),
 * myproxy_deserialize_response(), and myproxy_handle_authorization()
 * with some error checking.
 *
 */
int myproxy_recv_response_ex(myproxy_socket_attrs_t *attrs,
			     myproxy_response_t *response,
			     myproxy_request_t *client_request);

/*
 * myproxy_handle_authorization()
 *
 * If MYPROXY_AUTHORIZATION_RESPONSE is received, pass it to this
 * function to be processed.
 *
 */
int myproxy_handle_authorization(myproxy_socket_attrs_t *attrs,
				 myproxy_response_t *server_response,
				 myproxy_request_t *client_request);

/*
 * myproxy_resolve_hostname()
 *
 * Helper function to fully-qualify the given hostname for
 * authorization of server identity.
 *
 */
void myproxy_resolve_hostname(char **host);

/*
 * myproxy_endentity_store()
 *
 * Stores an end-entity credential on the server
 *
 * returns 0 on success, -1 on error
int myproxy_endentity_store( myproxy_socket_attrs_t *attrs,
                             const char *delegfile,
                             char *passphrase);
*/

#endif /* __MYPROXY_PROTOCOL_H */
