#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>

typedef struct {
    int server_fd;
    struct sockaddr_in address;
    int port;
    int queue_size;
} Server;

// Funcții pentru inițializarea și rularea serverului
Server* create_server(int port, int queue_size);
void start_server(Server* server);
void destroy_server(Server* server);

#endif // SERVER_H
