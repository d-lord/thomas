#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

typedef struct Data {
    char *name;
    char *secret;
    FILE *log;
    int port; // 1 to 65534, or 0 if undefined and not yet ephemeralised
    char *interface;
    char **argv;
} Data;

/* prints, kills the program */
void error(int e) {
    char *str;
    switch (e) {
        case 1:
            str = "Usage: station name authfile log-file [port] [host]";
            break;
        case 2:
            str = "Invalid name/auth";
            break;
        case 3:
            str = "Unable to open log";
            break;
        case 4:
            str = "Invalid port";
            break;
        case 5:
            str = "Listen error";
            break;
        case 6:
            str = "Unable to connect to station";
            break;
        case 7:
            str = "Duplicate station names";
            break;
        case 99:
            str = "Unspecified system call failure";
            break;
        case 0:
            str = "";
            break;
        default:
            str = "Unknown error";
    }
    fprintf(stderr, e ? "%s\n" : "%s", str); // effective
    exit(e);
}

/* merrily assumes it'll fit in memory.
 * dynamically allocates space, consumes '\n'
 * also assumes it's opened in read mode */
char *read_a_line(FILE *f) {
    int i = 0, size = 10;
    if (f == NULL) {
        char *str = malloc(sizeof(char));
        str[0] = '\0';
        return str;
    }
    char *str = malloc(sizeof(char) * size);
    char c;
    while (((c = fgetc(f)) != '\n') && (c != EOF)) {
        str[i++] = c;
        if (i >= size - 1) {
            size *= 2;
            str = realloc(str, sizeof(char) * size);
            if (str == NULL) {
                fprintf(stderr, "So we *are* tested with huge lines.\n");
                error(99);
            }
        }
    }
    str[i] = '\0';
    return str;
}

/* parses the arguments, handles errors 1-3, otherwise returns arg data */
Data *parse_args(int argc, char **argv) {
    Data *d = malloc(sizeof(Data) * 1);
    if (argc < 4 || argc > 6) {
        error(1);
    }
    // read in auth secret
    FILE *auth = fopen(argv[2], "r");
    d->secret = read_a_line(auth);
    if ((strlen(argv[2]) == 0) || auth == NULL || strlen(d->secret) == 0) {
        error(2);
    }
    fclose(auth);
    if (argc >= 5) {
        long int p = strtol(argv[4], NULL, 2 * 5);
        if (p <= 0 || p >= 65535) {
            // there's some LONG_MIN stuff but it's covered by these
            error(4);
        }
        d->port = (int) p;
    } else {
        d->port = 0; // sentinel
    }
    d->log = fopen(argv[3], "w");
    if (d->log == NULL) {
        fprintf(stderr, "Placeholder error: unable to open log file\n");
        error(99); // TODO: presumably joel will specify this
    }
    d->interface = (argc == 6) ? argv[5] : NULL;
    return d;
}

/* set up our signal junk
 * SIGHUP triggers a log write
 * SIGPIPE gets ignored
 * outdated: all threads should sigmask except the sigwait() thread
 * so it can't all be done in one function
 */
void signal_init() {
    signal(SIGPIPE, SIG_IGN);
    struct sigaction sa;
    // sa.
    if (sigaction(SIGHUP, &sa, NULL)) {
        error(99);
    }
}

/* straight from peter: takes a hostname, returns its IP as an in_addr */
struct in_addr *name_to_ip_addr(char *hostname)
{
    int error;
    struct addrinfo *addressInfo;

    error = getaddrinfo(hostname, NULL, NULL, &addressInfo);
    if(error) {
	return NULL;
    }
    return &(((struct sockaddr_in*)(addressInfo->ai_addr))->sin_addr);
}

/* returns the file descriptor opened
 * if port is 0, an ephemeral port will be used and assigned to port
 * prints the port to stdout as per spec
 */
int open_listen(int *port, char *interface) {
    int fd;
    struct sockaddr_in serverAddr;
    int optVal;

    // Create IPv4 TCP socket
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) {
        perror("Error creating socket");
        exit(1);
    }

    // Allow address (IP addr + port num) to be reused immediately
    optVal = 1;
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optVal, sizeof(int)) < 0) {
        perror("Error setting socket option");
        exit(1);
    }

    // Set up address structure for the server address
    // Request port (incl. ephemeral), request interface if specified
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(*port);
    struct in_addr *t = name_to_ip_addr(interface);
    if (interface == NULL) {
        serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        if (t == NULL) {
            error(5);
        }
        serverAddr.sin_addr = *t;
    }

    // Bind socket to this address
    if(bind(fd, (struct sockaddr*)&serverAddr, 
            sizeof(struct sockaddr_in)) < 0) {
        perror("Error binding socket to port");
        exit(1);
    }

    // Indicate we're ready to accept connections on that socket
    // SOMAXCONN is max num of connection requests to be queued by the
    // OS (default 128)
    if(listen(fd, SOMAXCONN) < 0) {
        perror("Error listening");
        exit(1);
    }

    // read and print assigned port using getaddrinfo
    struct sockaddr_in info;
    socklen_t len = sizeof(info);
    // populate the info struct
    if (getsockname(fd, (struct sockaddr*) &info, &len)) {
        perror("Error getting socket info");
        exit(1);
    }
    *port = ntohs(info.sin_port);
    printf("Listening on port %d on interface %s\n", *port, 
            interface ? interface : "INADDR_ANY");

    return fd;
}

int main(int argc, char **argv) {
    Data *data = parse_args(argc, argv);
    int mainSocket = open_listen(&data->port, data->interface); // returns fd
    mainSocket;
    sleep(10);
    return 0;
}
