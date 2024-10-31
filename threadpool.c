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
