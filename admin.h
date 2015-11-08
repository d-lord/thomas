#ifndef ADMIN_H_
#define ADMIN_H_
/* vim: set filetype=c : */ 

// try running:
// socat - UNIX-CONNECT:control-socket

/* Contains admin functions relating to the control socket.
 * Multiple admin connections are possible
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <pthread.h>
#include "shared.h"

/* one instance is shared between admin threads */
typedef struct {
    int counter; // total administrators that have connected
    pthread_mutex_t counterLock;
} AdminStats;

/* arguments passed to the master admin thread which spawns others */
typedef struct {
    int fd; // master socket
    AdminStats* adminStats;
    ProgStats* progStats;
} AdminMasterThreadArgs;

/* arguments passed to a new admin thread including shared stats */
typedef struct {
    int fd;
    struct sockaddr_un s; // returned by accept()
    AdminStats* adminStats;
    ProgStats* progStats;
} AdminClientThreadArgs;

/*
 * creates the socket and binds it to the name specified
 * upon failure, kills the program
 * returns the file descriptor
 * resembles "user_open_listen"
 */
int make_control_socket(char *filename);

/* a single thread for a single user: tied to a file descriptor 
 * always returns NULL, returns when connection closes */
void* admin_client_thread(void*);
/* spawns and detaches the master thread for admins */
void admin_begin_processing(int, AdminStats*, ProgStats*);
/* master thread for admins: spawns individual threads for new connections */
void* admin_process_connections(void*);

#endif
