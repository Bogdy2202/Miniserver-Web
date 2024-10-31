#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "server.h"
#include "threadpool.h"

Server* create_server(int port, int queue_size) {
    Server* server = malloc(sizeof(Server));
    if (!server) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    server->port = port;
    server->queue_size = queue_size;

    // Creează socketul
    server->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_fd == 0) {
        perror("Socket creation failed");
        free(server);
        exit(EXIT_FAILURE);
    }

    // Configurare adresă server
    server->address.sin_family = AF_INET;
    server->address.sin_addr.s_addr = INADDR_ANY;
    server->address.sin_port = htons(port);

    // Leagă socketul de port
    if (bind(server->server_fd, (struct sockaddr*)&server->address, sizeof(server->address)) < 0) {
        perror("Binding failed");
        close(server->server_fd);
        free(server);
        exit(EXIT_FAILURE);
    }

    // Pune serverul în ascultare
    if (listen(server->server_fd, queue_size) < 0) {
        perror("Listen failed");
        close(server->server_fd);
        free(server);
        exit(EXIT_FAILURE);
    }

    return server;
}

void start_server(Server* server) {
    printf("Serverul rulează și ascultă pe portul %d...\n", server->port);
    while (1) {
        int client_socket;
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        client_socket = accept(server->server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        printf("Conexiune acceptată de la %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        // Adaugă cererea în coadă pentru a fi procesată de thread-uri
        enqueue_request(client_socket);
    }
}

void destroy_server(Server* server) {
    close(server->server_fd);
    free(server);
}
