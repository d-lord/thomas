/* 
** Adapted from PSutton's multithreaded server
** Binds to a port (specified or ephemeral) and echoes capitalised crap to clients
** Also binds to a control socket and, eventually, allows control of the server
** So 'users' connect through the port, 'admins' through the socket
*/
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include "user.h"
#include "admin.h"

const char usage_msg[] =
"Usage: ./thomas [-p port] [-i interface] -l logfile -a authfile [-s socket]\n"
"-p port              if unspecified, defaults to ephemeral\n"
"-i interface         if unspecified, defaults to 127.0.0.1\n"
"-l logfile           the file to write logs to\n"
"-a authfile          the file to read authstring from\n"
"-s socket path       if unspecified, defaults to ./control-socket\n"
"";

typedef struct {
    int port; // may be 0 if ephemeral requested
    char *interface;
    char *logPath; // unused
    char *authPath; // unused
    char *controlPath;
} ProgramArgs;

#define DEFAULT_CONTROL_SOCKET "./control-socket";

char *controlPath; // NULL or defined after opening it
int controlSock; // 0 or defined after opening it

/* creates and populates the ProgramArgs struct
 * exits the program on invalid args */
ProgramArgs parse_args(int argc, char **argv) {
    ProgramArgs pa;
    pa.interface = pa.logPath = pa.authPath = NULL;
    pa.controlPath = DEFAULT_CONTROL_SOCKET;
    int c;
    extern char *optarg;
    extern int optind, optopt;
    long int tmp;
    int errors = 0;
    while ((c = getopt(argc, argv, "p:i:l:a:s:")) != -1) {
        switch (c) {
            case 'p':
                tmp = strtol(optarg, NULL, 10);
                if (tmp <= 0 || tmp >= 65535) {
                    fprintf(stderr, "Invalid argument to -p: %s\n", optarg);
                    ++errors;
                }
                pa.port = tmp;
                break;
            case 'i':
                pa.interface = optarg; // validated when we try to bind
                break;
            case 'l':
                pa.logPath = optarg; // not used
                break;
            case 'a':
                pa.authPath = optarg; // not used
                break;
            case 's':
                pa.controlPath = optarg; // validated when we try to bind
                break;
            case '?':
                fprintf(stderr, "Unknown option -%c\n", optopt);
        }
    }
    /* these options are un-implemented, so not currently required */
    /* if (pa.logPath == NULL) { */
    /*     fprintf(stderr, "Log path must be set\n"); */
    /*     ++errors; */
    /* } */
    /* if (pa.authPath == NULL) { */
    /*     fprintf(stderr, "Authfile path must be set\n"); */
    /*     ++errors; */
    /* } */
    if (errors) {
        fprintf(stderr, usage_msg);
        exit(1);
    }
    return pa;
}

void *client_thread(void *arg);

/* takes a text buffer and its len, and returns the capitalised version */
char *capitalise(char *buffer, int len)
{
    int i;

    for(i = 0; i < len; i++) {
        buffer[i] = (char)toupper((int)buffer[i]);
    }
    return buffer;
}

ProgStats init_prog_stats(void) {
    ProgStats ps;
    memset(&ps, 0, sizeof(ps));
    return ps;
}

/* delete the control socket we were using
 * if it hasn't yet been opened, do nothing */
void cleanup(void) {
    // if we close this, it cancels accept(): Software caused connection abort
    // fortunately, it doesn't seem to be required for unlink()
    //close(controlSock);
    if (controlPath != NULL) unlink(controlPath);
}
void handle_sigint(int sig) {
    cleanup();
    exit(sig);
}
void configure_sigint(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    if (sigaction(SIGINT, &sa, 0)) {
        perror("Configuring SIGINT");
    }
}

int main(int argc, char *argv[])
{
    ProgramArgs pa = parse_args(argc, argv);
    ProgStats progStats = init_prog_stats();
    controlPath = NULL;
    controlSock = 0;

    // user netcode
    int fdServer;
    fdServer = user_open_listen(&pa.port, pa.interface); // sets port if ephemeral
    printf("port after open listen is %d\n", pa.port);
    user_begin_processing(fdServer, &progStats);

    // admin sockcode
    controlSock = make_control_socket(pa.controlPath);
    controlPath = pa.controlPath;
    atexit(cleanup); // now that we've got a socket to close and unlink
    configure_sigint();
    AdminStats adminStats; // so it's in the main scope
    admin_begin_processing(controlSock, &adminStats, &progStats);

    while(1) sleep(10); // one thread mastering user, one thread mastering admin

    return 0;
}
