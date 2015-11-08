#include "user.h"

/* takes a hostname or IP, returns IP as an in_addr */
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
int user_open_listen(int *port, char *interface) {
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
            fprintf(stderr, "Bad interface\n");
            exit(5);
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

/* handles a single incoming connection 
 * arg is an instance of UserThreadArgs on the heap */
void* user_client_thread(void* arg)
{
    int fd;
    char buffer[1024];
    ssize_t numBytesRead;

    UserThreadArgs *myArgs = (UserThreadArgs*) arg;
    pthread_mutex_lock(&myArgs->progStats->currentUsersLock);
    myArgs->progStats->currentUsers++;
    pthread_mutex_unlock(&myArgs->progStats->currentUsersLock);
    fd = myArgs->fd;
    // Repeatedly read from connected fd, capitalise text and send
    // it back
    while((numBytesRead = read(fd, buffer, 1024)) > 0) {
	capitalise(buffer, numBytesRead);
	write(fd, buffer, numBytesRead);
    }
    // Get here if EOF (client disconnected) or error

    if(numBytesRead < 0) {
	perror("Error reading from socket");
	exit(1);
    }
    // print a message to server's stdout
    printf("Done\n");
    fflush(stdout);
    // Close the connection to the client
    close(fd);

    // decrement connected users
    pthread_mutex_lock(&myArgs->progStats->currentUsersLock);
    myArgs->progStats->currentUsers--;
    pthread_mutex_unlock(&myArgs->progStats->currentUsersLock);

    free(myArgs);
    pthread_exit(NULL);	// Redundant
    return NULL;
}

/* spawns a thread to do user_process_connections
 */
void user_begin_processing(int fdServer, ProgStats *ps) {
    pthread_t threadId;
    UserMasterThreadArgs *args = malloc(sizeof(UserMasterThreadArgs));
    args->fd = fdServer;
    args->progStats = ps;
    pthread_create(&threadId, NULL, user_process_connections,
            (void*) args);
    return;
}

/* accepts new connections on the listen port and spawns threads to handle them */
void *user_process_connections(void *arg)
{
    UserMasterThreadArgs *args = (UserMasterThreadArgs*) arg;
    int fdServer = args->fd;
    int fd;
    UserThreadArgs *threadArgs;
    struct sockaddr_in fromAddr;
    socklen_t fromAddrSize;
    int error;
    char hostname[MAX_HOST_NAME_LEN];
    pthread_t threadId;

    while(1) {
        fromAddrSize = sizeof(struct sockaddr_in);
	// Block, wait for new connection 
	// (fromAddr will be populated with client address details)
        fd = accept(fdServer, (struct sockaddr*)&fromAddr, &fromAddrSize);
        if(fd < 0) {
            perror("Error accepting connection");
            exit(1);
        }
        threadArgs = malloc(sizeof(UserThreadArgs)); // thread must free this
        threadArgs->fd = fd;
        threadArgs->progStats = args->progStats;
     
	// Convert IP address into hostname
        error = getnameinfo((struct sockaddr*)&fromAddr, fromAddrSize, hostname,
                MAX_HOST_NAME_LEN, NULL, 0, 0);
        if(error) {
            fprintf(stderr, "Error getting hostname: %s\n", 
                    gai_strerror(error));
        } else {
            printf("Accepted connection from %s (%s), port %d\n", 
                    inet_ntoa(fromAddr.sin_addr), hostname,
                    ntohs(fromAddr.sin_port));
	    write(fd, "Welcome...\n", 11);
        }

	// Start a new thread to deal with client communication
	// Pass the connected file descriptor as an argument to
	// the thread (cast to void*)
	pthread_create(&threadId, NULL, user_client_thread, 
		(void*) threadArgs);
	pthread_detach(threadId);
    }
    return NULL;
}
