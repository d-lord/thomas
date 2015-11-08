#include "admin.h"

// thanks to http://beej.us/guide/bgipc/output/html/multipage/unixsock.html
int make_control_socket(char *path) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Creating socket");
    }

    struct sockaddr_un main;
    main.sun_family = AF_UNIX;
    if (strlen(path) > 104-1) {
        fprintf(stderr, "Path is too long: we only have char[104]\n");
        exit(1);
    }
    strcpy(main.sun_path, path);
    // optionally remove the socket
    unlink(path);
    size_t len = sizeof(main.sun_family) + sizeof(main.sun_path);

    if (bind(sock, (struct sockaddr*) &main, len)) {
        perror("Error binding to control socket");
        exit(1);
    }

    // we have a file descriptor!
    printf("Bound to control socket at %s\n", path);

    return sock;
}

/* handles a single connected admin client
 * takes a pointer to an instance of AdminClientThreadArgs */
void* admin_client_thread(void* arg) {
    AdminClientThreadArgs *myArgs = (AdminClientThreadArgs*) arg;
    pthread_mutex_lock(&myArgs->adminStats->counterLock);
    int adminId = ++myArgs->adminStats->counter;
    pthread_mutex_unlock(&myArgs->adminStats->counterLock);
    printf("Admin %d connected!\n", adminId);
    FILE* write = fdopen(myArgs->fd, "w");
    pthread_mutex_lock(&myArgs->progStats->currentUsersLock);
    fprintf(write, "hello! we have %d users! goodbye!\n", 
            myArgs->progStats->currentUsers);
    pthread_mutex_unlock(&myArgs->progStats->currentUsersLock);
    fflush(write);
    /*
    const char *message = "hello! goodbye!\n";
    int len = strlen(message);
    send(myArgs->fd, message, len+1, 0);
    */

    close(myArgs->fd);
    printf("Admin %d disconnected!\n", adminId);
    free(myArgs);
    return NULL;
}

/*
 * Starts a thread to process the admin interface
 * Initialises the AdminStats struct too
 * Assumes we've bind() but not listen() the socket
 */
void admin_begin_processing(int sock, AdminStats* adminStats,
        ProgStats* progStats) {
    // create the admin master thread
    adminStats->counter = 0;
    pthread_mutex_init(&adminStats->counterLock, NULL);
    AdminMasterThreadArgs *args = malloc(sizeof(AdminMasterThreadArgs));
    args->fd = sock;
    args->adminStats = adminStats;
    args->progStats = progStats;
    pthread_t threadId;
    pthread_create(&threadId, NULL, admin_process_connections,
            (void*) args);
    pthread_detach(threadId);
}

/* creates new threads to handle connections on the admin socket */
void* admin_process_connections(void* arg) {
    AdminMasterThreadArgs *args = (AdminMasterThreadArgs*) arg;
    int masterSock = args->fd;
    int newSock;
    struct sockaddr_un remote;
    socklen_t len = sizeof(struct sockaddr_un);

    pthread_t newThread;
    AdminClientThreadArgs *newArgs;

    if (listen(masterSock, 5)) { // up to SOMAXCONN connections can queue
        perror("Error listening on control socket");
    }
    while (1) {
        newSock = accept(masterSock, (struct sockaddr*) &remote, &len);
        if (newSock == -1) {
            perror("Error accepting on control socket");
            exit(1);
        }
        newArgs = malloc(sizeof(AdminClientThreadArgs));
        newArgs->fd = newSock;
        newArgs->s = remote;
        newArgs->adminStats = args->adminStats;
        newArgs->progStats = args->progStats;
        pthread_create(&newThread, NULL, admin_client_thread,
                (void*) newArgs);
        pthread_detach(newThread);
    }
    free(args);
    return NULL;
}
