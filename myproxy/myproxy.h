/*
 * myproxy.h
 *
 * Defines protocol for communciation with myproxy-server
 *
 */

#ifndef __MYPROXY_H
#define __MYPROXY_H

#include "gsi_socket.h"
#include "myproxy_authorization.h"

/* Maximum pass phrase length */
#define MAX_PASS_LEN  1024 /* Arbitrary */

/* Define default myproxy-server -- should probably be put in config file */
#define MYPROXY_SERVER_PORT            7512

/* specify maximum delegation lifetime allowed on myproxy-server */
#define MYPROXY_DEFAULT_HOURS          168     /* 1 week */
#define MYPROXY_DEFAULT_DELEG_HOURS    2     

/* myproxy client protocol information */
#define MYPROXY_VERSION_STRING      "VERSION="
#define MYPROXY_COMMAND_STRING      "COMMAND="
#define MYPROXY_USERNAME_STRING     "USERNAME="
#define MYPROXY_PASSPHRASE_STRING   "PASSPHRASE="
#define MYPROXY_LIFETIME_STRING     "LIFETIME="
#define MYPROXY_RETRIEVER_STRING     "RETRIEVER="
#define MYPROXY_RENEWER_STRING     "RENEWER="
#define MYPROXY_AUTHORIZATION_STRING "AUTHORIZATION_DATA="
#define MYPROXY_AUTH_SERVICE_STRING "AUTHORIZED_SERVICE="
#define MYPROXY_AUTH_CLIENT_STRING  "AUTHORIZED_CLIENT="

/* myproxy server protocol information */
#define MYPROXY_RESPONSE_STRING     "RESPONSE="
#define MYPROXY_ERROR_STRING        "ERROR="

/* number of last error */
extern int errno;

/* Protocol commands */
typedef enum
{
    MYPROXY_GET_PROXY,
    MYPROXY_PUT_PROXY,
    MYPROXY_INFO_PROXY,
    MYPROXY_DESTROY_PROXY
} myproxy_proto_request_type_t;

/* server response codes */
typedef enum
{
    MYPROXY_OK_RESPONSE,
    MYPROXY_ERROR_RESPONSE,
    MYPROXY_AUTHORIZATION_RESPONSE
} myproxy_proto_response_type_t;

/* request type */
typedef enum
{
    RETRIEVAL,
    RENEWAL
} _request_type;

/* client/server socket attributes */
typedef struct 
{
  char *pshost;	
  int psport;
  int socket_fd;
  GSI_SOCKET *gsi_socket; 
} myproxy_socket_attrs_t;

/* A client request object */
#define REGULAR_EXP 1
#define MATCH_CN_ONLY 0

typedef struct
{
    char                         *version;
    char                         *username;
    char                         passphrase[MAX_PASS_LEN+1];
    myproxy_proto_request_type_t command_type;
    int                          proxy_lifetime;
    char                         **authorized_service_dns;
    char                         **authorized_client_dns;
    char                         *retrievers;
    char                         *renewers;
} myproxy_request_t;

/* A server response object */
typedef struct
{
  char                          *version;
  myproxy_proto_response_type_t response_type;
  char                          error_string[2048];
  authorization_data_t          **authorization_data; 
  /* NULL-terminated array describing acceptable authorization methods */
} myproxy_response_t;


/*
 * myproxy_init_client()
 *
 * Create a generic client by creating a GSI socket and connecting to a a host 
 *
 * returns the file descriptor of the connected socket or -1 if an error occurred  
 */
int myproxy_init_client(myproxy_socket_attrs_t *attrs);

/*
 * myproxy_authenticate_init()
 * 
 * Perform client-side authentication
 *
 * returns -1 if unable to authenticate, 0 if authentication successful
 */ 
int myproxy_authenticate_init(myproxy_socket_attrs_t *attr, const char *proxyfile);

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
int  myproxy_serialize_request(const myproxy_request_t *request, 
			    char *data, const int datalen);


/*
 * myproxy_deserialize_request()
 * 
 * Deserialize a buffer into a request object.
 *
 * returns 0 if succesful, otherwise -1
 */
int  myproxy_deserialize_request(const char *data, const int datalen, 
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
 * Sends a buffer with possible encryption done via GSI
 *
 * returns -1 if GSI_SOCKET_write_buffer failed or 0 on success
 */
int myproxy_send(myproxy_socket_attrs_t *attrs,
                 const char *data, const int datalen);

/*
 * myproxy_recv()
 *
 * Receives a buffer with possible encryption done via GSI 
 *
 * returns GSI_SOCKET_read_buffer()
 * 
 */
int  myproxy_recv(myproxy_socket_attrs_t *attrs,
			   char *data, const int datalen);

/*
 * myproxy_init_delegation()
 *
 * Delegates a proxy based on the credentials found in file 
 * location delegfile good for lifetime_seconds
 *
 * returns 0 on success, -1 on error 
 */
int myproxy_init_delegation(myproxy_socket_attrs_t *attrs, const char *delegfile, const int lifetime_seconds);

/*
 * myproxy_accept_delegation()
 *
 * Accepts delegated credentials into file location data
 *
 * returns 0 on success, -1 on error 
 */
int myproxy_accept_delegation(myproxy_socket_attrs_t *attrs, char *data, const int datalen);

/*
 * myproxy_free()
 * 
 * Frees up memory used for creating request, response and socket objects 
 */
void myproxy_free(myproxy_socket_attrs_t *attrs, myproxy_request_t *request, myproxy_response_t *response);

/*
 * myproxy_recv_response()
 *
 * Helper function that combines myproxy_recv() and
 * myproxy_deserialize_response() with some error checking.
 *
 */
int myproxy_recv_response(myproxy_socket_attrs_t *attrs, myproxy_response_t *response); 


#endif /* __MYPROXY_H */
