//main.c
#include <stdio.h>
#include <stdlib.h>
#include "server.h"
#include "threadpool.h"

int main(int argc, char *argv[]) {
    // Verifică argumentele pentru port și numărul de threads
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <port> <num_threads>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    int num_threads = atoi(argv[2]);

    // Creează serverul
    Server* server = create_server(port, 10); // 10 este numărul maxim de conexiuni în așteptare

    // Inițializează pool-ul de thread-uri
    initialize_thread_pool(num_threads);

    // Pornește serverul - aceasta va rula în buclă infinită
    start_server(server);

    // Închide serverul și eliberează resursele (deși nu ajungem aici în acest exemplu simplu)
    destroy_server(server);
    shutdown_thread_pool();

    return 0;
}


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



#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>

void initialize_thread_pool(int thread_count);
void enqueue_request(int client_socket);
void shutdown_thread_pool();

#endif // THREADPOOL_H


#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "threadpool.h"
#include "request_handler.h"

#define QUEUE_SIZE 10

static int client_queue[QUEUE_SIZE];
static int queue_front = 0, queue_rear = 0, queue_count = 0;
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

void* thread_function(void* arg) {
    while (1) {
        int client_socket;
        
        // Extrage conexiunea din coadă
        pthread_mutex_lock(&queue_mutex);
        while (queue_count == 0) {
            pthread_cond_wait(&queue_cond, &queue_mutex);
        }
        client_socket = client_queue[queue_front];
        queue_front = (queue_front + 1) % QUEUE_SIZE;
        queue_count--;
        pthread_cond_signal(&queue_cond);
        pthread_mutex_unlock(&queue_mutex);

        // Procesează cererea
        handle_request(client_socket);
    }
    return NULL;
}

void initialize_thread_pool(int thread_count) {
    pthread_t thread;
    for (int i = 0; i < thread_count; i++) {
        if (pthread_create(&thread, NULL, thread_function, NULL) != 0) {
            perror("Thread creation failed");
            exit(EXIT_FAILURE);
        }
    }
}

void enqueue_request(int client_socket) {
    pthread_mutex_lock(&queue_mutex);
    while (queue_count == QUEUE_SIZE) {
        pthread_cond_wait(&queue_cond, &queue_mutex);
    }
    client_queue[queue_rear] = client_socket;
    queue_rear = (queue_rear + 1) % QUEUE_SIZE;
    queue_count++;
    pthread_cond_signal(&queue_cond);
    pthread_mutex_unlock(&queue_mutex);
}

void shutdown_thread_pool() {
    // Nu implementăm aici, dar este loc pentru o funcție care oprește threads în mod curat.
}



#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

void handle_request(int client_socket);
void serve_html(int client_socket);
void serve_css(int client_socket);
void handle_get_request(int client_socket, const char* request);
void handle_post_request(int client_socket, const char* request);
void send_404(int client_socket);

#endif // REQUEST_HANDLER_H


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "request_handler.h"

#define BUFFER_SIZE 1024

void handle_request(int client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_received = read(client_socket, buffer, BUFFER_SIZE - 1);
    if (bytes_received < 0) {
        perror("Read failed");
        close(client_socket);
        return;
    }

    buffer[bytes_received] = '\0';

    if (strncmp(buffer, "GET / ", 6) == 0) {
        serve_html(client_socket);
    } else if (strncmp(buffer, "GET /style.css", 14) == 0) {
        serve_css(client_socket);
    } else if (strncmp(buffer, "POST", 4) == 0) {
        handle_post_request(client_socket, buffer);
    } else {
        send_404(client_socket);
    }

    close(client_socket);
}

void serve_html(int client_socket) {
    FILE *file = fopen("index.html", "r");
    if (!file) {
        send_404(client_socket);
        return;
    }

    char response[BUFFER_SIZE];
    char content[BUFFER_SIZE * 4];
    size_t content_length = fread(content, 1, sizeof(content), file);
    fclose(file);

    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %zu\r\n"
             "\r\n"
             "%s",
             content_length, content);
    send(client_socket, response, strlen(response), 0);
}

void serve_css(int client_socket) {
    FILE *file = fopen("style.css", "r");
    if (!file) {
        send_404(client_socket);
        return;
    }

    char response[BUFFER_SIZE];
    char content[BUFFER_SIZE * 4];
    size_t content_length = fread(content, 1, sizeof(content), file);
    fclose(file);

    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/css\r\n"
             "Content-Length: %zu\r\n"
             "\r\n"
             "%s",
             content_length, content);
    send(client_socket, response, strlen(response), 0);
}

void handle_get_request(int client_socket, const char* request) {
    // Răspuns simplu la o cerere GET
    char *response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 14\r\n\r\nThis is GET!";
    send(client_socket, response, strlen(response), 0);
}

void handle_post_request(int client_socket, const char* request) {
    // Extrage corpul cererii POST (după antetul HTTP)
    char *body = strstr(request, "\r\n\r\n");
    if (body) {
        body += 4; // Avansează după "\r\n\r\n"
        printf("Received POST data: %s\n", body);
    }

    // Răspuns simplu la o cerere POST
    char *response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 15\r\n\r\nThis is POST!";
    send(client_socket, response, strlen(response), 0);
}

void send_404(int client_socket) {
    char *response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 13\r\n\r\n404 Not Found";
    send(client_socket, response, strlen(response), 0);
}
