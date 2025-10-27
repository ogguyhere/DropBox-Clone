#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include "client_threadpool.h"
#include "worker.h"
#include "queue.h"
#include "metadata.h"

#define WORKER_POOL_SIZE 3
#define SERVER_PORT 8080

client_threadpool_t *global_client_pool = NULL;
queue_t *global_task_queue = NULL;
metadata_t *global_metadata = NULL;
pthread_t worker_threads[WORKER_POOL_SIZE];
pthread_t accept_thread;
int server_fd = -1;
volatile sig_atomic_t shutdown_flag = 0;

void signal_handler(int sig)
{
    printf("\nReceived signal %d, shutting down...\n", sig);
    fflush(stdout);
    shutdown_flag = 1;

    if (server_fd != -1) {
        shutdown(server_fd, SHUT_RDWR); // unblock accept immediately
        close(server_fd);
        server_fd = -1;
    }

    // Wake up client threads
    if (global_client_pool) {
        global_client_pool->stop = 1;
        pthread_mutex_lock(&global_client_pool->client_queue.lock);
        pthread_cond_broadcast(&global_client_pool->client_queue.not_empty);
        pthread_mutex_unlock(&global_client_pool->client_queue.lock);
    }

    // Wake up worker threads
    if (global_task_queue) {
        pthread_mutex_lock(&global_task_queue->lock);
        pthread_cond_broadcast(&global_task_queue->cond);
        pthread_mutex_unlock(&global_task_queue->lock);
    }
}


void* accept_connections(void* arg)
{
    client_threadpool_t *client_pool = (client_threadpool_t *)arg;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    int client_fd;


    while (!shutdown_flag) {
        client_fd = accept(server_fd, (struct sockaddr*)&address, &addrlen);
        if (client_fd < 0) {
            if (shutdown_flag && (errno == EBADF || errno == EINTR)) break;
            perror("Accept failed");
            continue;
        }

        if (shutdown_flag) {  // extra safety
            close(client_fd);
            break;
        }

        printf("New client connected, socket descriptor: %d\n", client_fd);
        fflush(stdout);
        enqueue_socket(client_pool, client_fd);
    }

    printf("Accept thread exiting\n");
    fflush(stdout);
    return NULL;
}

int main()
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("=== Dropbox Clone Server Starting ===\n");
    fflush(stdout);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", SERVER_PORT);
    fflush(stdout);

    global_metadata = metadata_init();
    global_task_queue = queue_init();
    global_client_pool = init_client_threadpool(global_task_queue, global_metadata);

    worker_args_t wargs[WORKER_POOL_SIZE];
    for (int i = 0; i < WORKER_POOL_SIZE; i++) {
        wargs[i].task_queue = global_task_queue;
        wargs[i].metadata = global_metadata;
        wargs[i].id = i + 1;
        pthread_create(&worker_threads[i], NULL, worker_func, &wargs[i]);
    }

    pthread_create(&accept_thread, NULL, accept_connections, global_client_pool);

    printf("=== Server Ready ===\n");
    fflush(stdout);

    pthread_join(accept_thread, NULL);

    printf("Shutting down server...\n");
    fflush(stdout);

    cleanup_client_threadpool(global_client_pool);
    global_client_pool = NULL;

    pthread_mutex_lock(&global_task_queue->lock);
    pthread_cond_broadcast(&global_task_queue->cond);
    pthread_mutex_unlock(&global_task_queue->lock);

    for (int i = 0; i < WORKER_POOL_SIZE; i++)
        pthread_join(worker_threads[i], NULL);

    queue_destroy(global_task_queue);
    metadata_destroy(global_metadata);

    printf("Server shutdown complete\n");
    fflush(stdout);

    return 0;
}
