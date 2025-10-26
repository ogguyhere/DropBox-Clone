// src/main.c

/*
This is ---main accept loop---
--> Creates a single main thread that listens to TCP connections
--> When client connects:
 - Accept connection
 - Push socket descriptor in client queue
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include "client_threadpool.h"
#include "worker.h"
#include "queue.h"
#include "metadata.h"

#define WORKER_POOL_SIZE 3
#define SERVER_PORT 8080

// Global resources for cleanup
client_threadpool_t *global_client_pool = NULL;

queue_t *global_task_queue = NULL;
metadata_t *global_metadata = NULL;

pthread_t worker_threads[WORKER_POOL_SIZE];

// Signal handler for graceful shutdown
void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    shutdown_flag = 1;
    exit(0);
}

void* accept_connections(void* arg) {
    client_threadpool_t *client_pool = (client_threadpool_t *)arg;
    
    int server_fd, client_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    
    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf("Socket created successfully, file descriptor: %d\n", server_fd);
    
    // Set socket options to reuse address
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
    
    // Set server address structure
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SERVER_PORT);
    
    // Bind the socket to the address and port
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Socket bound to port %d\n", SERVER_PORT);
    
    // Listen for incoming connections
    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    printf("Server is listening for connections...\n");
    
    // Accept and handle connections in a loop
    while (1) {
        if ((client_fd = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            continue;
        }
        printf("New client connected, socket descriptor: %d\n", client_fd);
        enqueue_socket(client_pool, client_fd);
    }
    
    close(server_fd);
    return NULL;
}

int main() {
    // Set up signal handlers for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("=== Dropbox Clone Server Starting ===\n");
    
    // Initialize metadata
    global_metadata = metadata_init();
    if (!global_metadata) {
        fprintf(stderr, "Failed to initialize metadata\n");
        return 1;
    }
    printf("Metadata initialized\n");
    
    // Initialize task queue
    global_task_queue = queue_init();
    if (!global_task_queue) {
        fprintf(stderr, "Failed to initialize task queue\n");
        metadata_destroy(global_metadata);
        return 1;
    }
    printf("Task queue initialized\n");
    
    // Initialize worker threadpool
    worker_args_t worker_args[WORKER_POOL_SIZE];
    for (int i = 0; i < WORKER_POOL_SIZE; i++) {
        worker_args[i].task_queue = global_task_queue;
        worker_args[i].metadata = global_metadata;
        worker_args[i].id = i + 1;
        
        if (pthread_create(&worker_threads[i], NULL, worker_func, &worker_args[i]) != 0) {
            fprintf(stderr, "Failed to create worker thread %d\n", i);
            return 1;
        }
    }
    printf("Worker threadpool initialized with %d workers\n", WORKER_POOL_SIZE);
    
    // Initialize client threadpool
    global_client_pool = init_client_threadpool(global_task_queue, global_metadata);

    
    if (!global_client_pool) {
        fprintf(stderr, "Failed to initialize client threadpool\n");
        return 1;
    }
    
    // Create accept thread
    pthread_t accept_thread;
    if (pthread_create(&accept_thread, NULL, accept_connections, global_client_pool) != 0) {
        perror("Accept thread creation failed");
        exit(EXIT_FAILURE);
    }
    printf("Accept thread started\n");
    printf("=== Server Ready ===\n\n");
    
    // Wait indefinitely (threads handle everything)
    pause();
    
    // Cleanup (will reach here after signal)
    printf("Cleaning up resources...\n");
    cleanup_client_threadpool(global_client_pool);
    queue_destroy(global_task_queue);
    metadata_destroy(global_metadata);
    printf("Server shutdown complete\n");
    
    return 0;
}