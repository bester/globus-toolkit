/*
 * myproxy-get-delegation
 *
 * Webserver program to retrieve a delegated credential from a myproxy-server
 */

#include "myproxy_common.h"	/* all needed headers included here */

static char usage[] = \
"\n"
"Syntax: myproxy-get-delegation [-t hours] [-l username] ...\n"
"        myproxy-get-delegation [-usage|-help] [-version]\n"
"\n"
"   Options\n"
"       -h | --help                       Displays usage\n"
"       -u | --usage                                    \n"
"                                                      \n"
"       -v | --verbose                    Display debugging messages\n"
"       -V | --version                    Displays version\n"
"       -l | --username        <username> Username for the delegated proxy\n"
"       -t | --proxy_lifetime  <hours>    Lifetime of proxies delegated by\n" 
"                                         the server (default 12 hours)\n"
"       -o | --out             <path>     Location of delegated proxy\n"
"       -s | --pshost          <hostname> Hostname of the myproxy-server\n"
"       -p | --psport          <port #>   Port of the myproxy-server\n"
"       -a | --authorization   <path>     Use credential for authorization\n"
"                                         (instead of passphrase)\n"
"       -d | --dn_as_username             Use subject of the authorization\n"
"                                         credential as the default username\n"
"                                         instead of the LOGNAME env. var.\n"
"       -k | --credname        <name>     Specify credential name\n"
"       -S | --stdin_pass                 Read passphrase from stdin\n"
"\n";

struct option long_options[] =
{
    {"help",                   no_argument, NULL, 'h'},
    {"pshost",           required_argument, NULL, 's'},
    {"psport",           required_argument, NULL, 'p'},
    {"proxy_lifetime",   required_argument, NULL, 't'},
    {"out",              required_argument, NULL, 'o'},
    {"usage",                  no_argument, NULL, 'u'},
    {"username",         required_argument, NULL, 'l'},
    {"verbose",                no_argument, NULL, 'v'},
    {"version",                no_argument, NULL, 'V'},
    {"authorization",    required_argument, NULL, 'r'},
    {"dn_as_username",         no_argument, NULL, 'd'},
    {"credname",	 required_argument, NULL, 'k'},
    {"stdin_pass",             no_argument, NULL, 'S'},
    {0, 0, 0, 0}
};

static char short_options[] = "hus:p:l:t:o:vVa:dk:S";

static char version[] =
"myproxy-get-delegation version " MYPROXY_VERSION " (" MYPROXY_VERSION_DATE ") "  "\n";

void 
init_arguments(int argc, char *argv[], 
	       myproxy_socket_attrs_t *attrs,
	       myproxy_request_t *request); 

/*
 * Use setvbuf() instead of setlinebuf() since cygwin doesn't support
 * setlinebuf().
 */
#define my_setlinebuf(stream)	setvbuf((stream), (char *) NULL, _IOLBF, 0)

/* location of delegated proxy */
char *outputfile = NULL;
char *creds_to_authorization = NULL;
static int dn_as_username = 0;
static int read_passwd_from_stdin = 0;

int
main(int argc, char *argv[]) 
{    
    myproxy_socket_attrs_t *socket_attrs;
    myproxy_request_t      *client_request;
    myproxy_response_t     *server_response;

    my_setlinebuf(stdout);
    my_setlinebuf(stderr);

    socket_attrs = malloc(sizeof(*socket_attrs));
    memset(socket_attrs, 0, sizeof(*socket_attrs));

    client_request = malloc(sizeof(*client_request));
    memset(client_request, 0, sizeof(*client_request));

    server_response = malloc(sizeof(*server_response));
    memset(server_response, 0, sizeof(*server_response));

    /* Setup defaults */
    myproxy_set_delegation_defaults(socket_attrs,client_request);

    /* Initialize client arguments and create client request object */
    init_arguments(argc, argv, socket_attrs, client_request);

    if (!outputfile) {
	GLOBUS_GSI_SYSCONFIG_GET_PROXY_FILENAME(&outputfile,
						GLOBUS_PROXY_FILE_OUTPUT);
    }

    if (creds_to_authorization == NULL) {
       /* Allow user to provide a passphrase */
	int rval;
	if (read_passwd_from_stdin) {
	    rval = myproxy_read_passphrase_stdin(client_request->passphrase, sizeof(client_request->passphrase));
	} else {
	    rval = myproxy_read_passphrase(client_request->passphrase,
					   sizeof(client_request->passphrase));
	}
	if (rval == -1) {
	    fprintf(stderr, "Error reading passphrase\n");
	    return 1;
	}
    }

    if (dn_as_username && creds_to_authorization) {
	if (client_request->username) {
	    free(client_request->username);
	    client_request->username = NULL;
	}
	if (ssl_get_base_subject_file(creds_to_authorization,
		                     &client_request->username)) {
	  fprintf(stderr, "Cannot get subject name from your certificate %s\n",
		  creds_to_authorization);
	  return 1;
	}
    }

    if (myproxy_get_delegation(socket_attrs, client_request, 
	    creds_to_authorization, server_response, outputfile)!=0) {
	fprintf(stderr, "Failed to receive a proxy.\n");
	return 1;
    }
    printf("A proxy has been received for user %s in %s\n",
           client_request->username, outputfile);
    free(outputfile);
    verror_clear();

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
	case 't':       /* Specify proxy lifetime in seconds */
	  request->proxy_lifetime = 60*60*atoi(gnu_optarg);
	  break;
        case 's': 	/* pshost name */
	    attrs->pshost = strdup(gnu_optarg);
            break;
        case 'p': 	/* psport */
            attrs->psport = atoi(gnu_optarg);
            break;
	case 'h': 	/* print help and exit */
            fprintf(stderr, usage);
            exit(1);
            break;
        case 'u': 	/* print help and exit */
            fprintf(stderr, usage);
            exit(1);
            break;
        case 'l':	/* username */
            request->username = strdup(gnu_optarg);
            break;
	case 'o':	/* output file */
	    outputfile = strdup(gnu_optarg);
            break;    
	case 'a':       /* special authorization */
	    creds_to_authorization = strdup(gnu_optarg);
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
	case 'k':   /* credential name */
	    request->credname = strdup (gnu_optarg);
	    break;
	case 'S':
	    read_passwd_from_stdin = 1;
	    break;
        default:        /* print usage and exit */ 
	    fprintf(stderr, usage);
	    exit(1);
	    break;	
        }
    }

    /* Check to see if myproxy-server specified */
    if (attrs->pshost == NULL) {
	fprintf(stderr, "Unspecified myproxy-server! Either set the MYPROXY_SERVER environment variable or explicitly set the myproxy-server via the -s flag\n");
	exit(1);
    }

    /* Check to see if username specified */
    if (request->username == NULL) {
	fprintf(stderr, usage);
	fprintf(stderr, "Please specify a username!\n");
	exit(1);
    }

    return;
}
