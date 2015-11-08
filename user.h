#ifndef NET_H_
#define NET_H_

/* Contains all the network code I've seen 100x in tutes 
 * some of it has my tweaks, though */

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
#include "shared.h"

#define MAX_HOST_NAME_LEN 128

typedef struct {
    int fd;
    ProgStats* progStats;
} UserThreadArgs;

typedef struct {
    int fd; // net socket we're listening on
    ProgStats* progStats;
} UserMasterThreadArgs;

/* takes a hostname or IP, returns IP as an in_addr */
struct in_addr *name_to_ip_addr(char*);

/* returns the file descriptor opened
 * if port is 0, an ephemeral port will be used and assigned to port
 * prints the port to stdout as per spec
 */
int user_open_listen(int*, char*);

/* a single thread for a single user: tied to a file descriptor */
void* user_client_thread(void*);
/* spawns the master thread for users */
void user_begin_processing(int fdServer, ProgStats *ps);
/* is the master thread for users */
void* user_process_connections(void*);

#endif
