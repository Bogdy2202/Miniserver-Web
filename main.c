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
