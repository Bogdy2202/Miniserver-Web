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