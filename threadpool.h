#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>

void initialize_thread_pool(int thread_count);
void enqueue_request(int client_socket);
void shutdown_thread_pool();

#endif // THREADPOOL_H
