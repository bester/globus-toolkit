/*
 * myproxy-info
 *
 * Client program to inqure a proxy on a myproxy-server
 */

#include "myproxy_common.h"	/* all needed headers included here */

static char usage[] = \
"\n"\
"Syntax: myproxy-info [-l username] ...\n"\
"        myproxy-info [-usage|-help] [-version]\n"\
"\n"\
"    Options\n"\
"    -h | --help                Displays usage\n"\
"    -u | --usage                             \n"\
"                                            \n"\
"    -v | --verbose             Display debugging messages\n"\
"    -V | --version             Displays version\n"\
"    -l | --username <username> Username for the delegated proxy\n"\
"    -s | --pshost   <hostname> Hostname of the myproxy-server\n"\
"    -p | --psport   #          Port of the myproxy-server\n"
"    -d | --dn_as_username      Use the proxy certificate subject\n"
"                               (DN) as the default username,\n"
"                               instead of the LOGNAME env. var.\n"
"\n";

struct option long_options[] =
{
    {"help",             no_argument, NULL, 'h'},
    {"pshost",     required_argument, NULL, 's'},
    {"psport",     required_argument, NULL, 'p'},
    {"usage",            no_argument, NULL, 'u'},
    {"username",   required_argument, NULL, 'l'},
    {"verbose",          no_argument, NULL, 'v'},
    {"version",          no_argument, NULL, 'V'},
    {"dn_as_username",   no_argument, NULL, 'd'},
    {0, 0, 0, 0}
};

static char short_options[] = "hus:p:l:vVd";

static char version[] =
"myproxy-destroy version " MYPROXY_VERSION " (" MYPROXY_VERSION_DATE ") "  "\n";

static int dn_as_username = 0;

/* Function declarations */
void init_arguments(int argc, char *argv[],
                    myproxy_socket_attrs_t *attrs, myproxy_request_t *request);


int
main(int argc, char *argv[])
{
    char *pshost;
    char request_buffer[1024], *response_buffer = NULL;
    int requestlen, responselen;

    myproxy_socket_attrs_t *socket_attrs;
    myproxy_request_t      *client_request;
    myproxy_response_t     *server_response;

    myproxy_log_use_stream (stderr);

    socket_attrs = malloc(sizeof(*socket_attrs));
    memset(socket_attrs, 0, sizeof(*socket_attrs));

    client_request = malloc(sizeof(*client_request));
    memset(client_request, 0, sizeof(*client_request));

    server_response = malloc(sizeof(*server_response));
    memset(server_response, 0, sizeof(*server_response));

    /* setup defaults */
    client_request->version = malloc(strlen(MYPROXY_VERSION) + 1);
    strcpy(client_request->version, MYPROXY_VERSION);
    client_request->command_type = MYPROXY_INFO_PROXY;

    pshost = getenv("MYPROXY_SERVER");
    if (pshost != NULL) {
        socket_attrs->pshost = strdup(pshost);
    }

    if (getenv("MYPROXY_SERVER_PORT")) {
	socket_attrs->psport = atoi(getenv("MYPROXY_SERVER_PORT"));
    } else {
	socket_attrs->psport = MYPROXY_SERVER_PORT;
    }

    /* Initialize client arguments and create client request object */
    init_arguments(argc, argv, socket_attrs, client_request);

    /*
     * We don't need to send the real pass phrase to the server as it
     * will just use our identity to authenticate and authorize us.
     * But we need to send over a dummy pass phrase at least
     * MIN_PASS_PHASE_LEN (currently 6) characters long.
     */
    strncpy(client_request->passphrase, "DUMMY-PASSPHRASE",
            sizeof(client_request->passphrase));

    /* Set up client socket attributes */
    if (myproxy_init_client(socket_attrs) < 0) {
        fprintf(stderr, "%s\n",
                verror_get_string());
        return 1;
    }

    /* Authenticate client to server */
    if (myproxy_authenticate_init(socket_attrs, NULL /* Default proxy */) < 0) {
        fprintf(stderr, "%s\n",
                verror_get_string());
        return 1;
    }
    if (client_request->username == NULL) { /* set default username */
        char *username = NULL;
        if (dn_as_username) {
            if (ssl_get_base_subject_file(NULL,
                                          &username)) {
                fprintf(stderr,
                        "Cannot get subject name from your certificate\n");
                return 1;
            }
        } else {
            if (!(username = getenv("LOGNAME"))) {
                fprintf(stderr, "Please specify a username.\n");
                return 1;
            }
        }
        client_request->username = strdup(username);
     }
    /* Serialize client request object */
    requestlen = myproxy_serialize_request(client_request,
                                           request_buffer, sizeof(request_buffer));

    if (requestlen < 0) {
        fprintf(stderr, "Error: %s", verror_get_string());
        exit(1);
    }

    /* Send request to the myproxy-server */
    if (myproxy_send(socket_attrs, request_buffer, requestlen) < 0) {
        fprintf(stderr, "%s,\n",
                verror_get_string());
        return 1;
    }

    /* Receive a response from the server */
    responselen = myproxy_recv_ex(socket_attrs, &response_buffer);
    if (responselen < 0) {
        fprintf(stderr, "%s\n",
                verror_get_string());
        return 1;
    }

    /* Make a response object from the response buffer */

    if (myproxy_deserialize_response(server_response, response_buffer, responselen) < 0) {
        fprintf(stderr, "%s\n",verror_get_string());
        exit(1);
    }
    free(response_buffer);
    response_buffer = NULL;

    /* Check version */
    if (strcmp(server_response->version, MYPROXY_VERSION) != 0) {
        fprintf(stderr, "Invalid version number received from server\n");
	exit(1);
    }

    /* Check response */
    switch(server_response->response_type) {
    case MYPROXY_ERROR_RESPONSE:
        fprintf(stderr, "Received ERROR_RESPONSE: %s\n",
		server_response->error_string);
	exit(1);
        break;
    case MYPROXY_OK_RESPONSE:
	printf("username: %s\n", client_request->username);
	myproxy_print_cred_info(server_response->info_creds, stdout);
	break;
    default:
        fprintf(stderr, "Invalid response type received.\n");
	exit(1);
        break;
    }

    printf ("\n");
    /* free memory allocated */
    myproxy_free(socket_attrs, client_request, server_response);

    return 0;
}


void 
init_arguments(int argc, 
		       char *argv[], 
		       myproxy_socket_attrs_t *attrs,
		       myproxy_request_t *request) 
{   
    extern char *gnu_optarg;
    int arg;

    while((arg = gnu_getopt_long(argc, argv, short_options, 
                             long_options, NULL)) != EOF) 
    {
        switch(arg) 
        {  
        case 's': 	/* pshost name */
	    attrs->pshost = strdup(gnu_optarg);
            break;
        case 'p': 	/* psport */
            attrs->psport = atoi(gnu_optarg);
            break;
        case 'u': 	/* print help and exit */
            fprintf(stderr, usage);
            exit(1);
            break;
	case 'h': 	/* print help and exit */
            fprintf(stderr, usage);
            exit(1);
            break;
        case 'l':	/* username */
	    request->username = strdup(gnu_optarg);
            break;
	case 'v':
	    myproxy_debug_set_level(1);
	    break;
        case 'V':       /* print version and exit */
            fprintf(stderr, version);
            exit(1);
            break;
	case 'd':   /* use the certificate subject (DN) as the default
		       username instead of LOGNAME */
	    dn_as_username = 1;
	    break;
        default:        /* print usage and exit */ 
            fprintf(stderr, usage);
	    exit(1);
            break;	
        }
    }

    /* Check to see if myproxy-server specified */
    if (attrs->pshost == NULL) {
	fprintf(stderr, usage);
	fprintf(stderr, "Unspecified myproxy-server! Either set the MYPROXY_SERVER environment variable or explicitly set the myproxy-server via the -s flag\n");
	exit(1);
    }

    return;
}
