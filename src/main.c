/*
this is ---main accept loop---

--> creates a single main thread that listens to tcp connections
--> when client connects:
        - accept connection
        - push socket descriptor in client queue
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>         // internet sockets
#include <pthread.h>
#include "client_threadpool.h" 


// ******************* Member B 
// What It Does: This is a dummy function that takes a socket descriptor (sockfd) and prints a message.
// It simulates pushing the socket into the Client Queue, which Member B will implement with thread-safe logic (e.g., using mutexes).

void enqueue_socket(int sockfd) {
    pthread_mutex_lock(&queue_mutex); // Use the mutex from client_threadpool.c
    if (queue_size < THREAD_POOL_SIZE) { // Limit to queue size
        socket_queue[queue_size] = sockfd;
        queue_size++;
        printf("Socket %d enqueued to threadpool (queue size: %d)\n", sockfd, queue_size);
    } else {
        printf("***** Queue full, dropping socket %d\n", sockfd);
        close(sockfd);
    }
    pthread_mutex_unlock(&queue_mutex);
    pthread_cond_signal(&queue_cond); // Wake up waiting threads
}

void* accept_connections(void* arg) {
    int server_fd, client_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf("Socket created successfully, file descriptor: %d\n", server_fd);

    // Set server address structure
    address.sin_family = AF_INET;              // Use IPv4
    address.sin_addr.s_addr = INADDR_ANY;      // Accept connections from any IP
    address.sin_port = htons(8080);            // Port number (8080 for this example)

    // Bind the socket to the address and port
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Socket bound to port 8080\n");

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
            continue; // Try again on failure
        }
        printf("New client connected, socket descriptor: %d\n", client_fd);
        enqueue_socket(client_fd); // Pass the socket to the next step
    }

    // Note: This close is unreachable due to the infinite loop, but included for completeness
    close(server_fd);
    return NULL;
}

int main() {
    pthread_t accept_thread;
    init_client_threadpool();

    // Create a thread to run the accept loop
    if (pthread_create(&accept_thread, NULL, accept_connections, NULL) != 0) {
        perror("Thread creation failed");
        exit(EXIT_FAILURE);
    }
    printf("Accept thread started\n");

    // Wait for the thread  
    // ************** add MULTI THREADING in next phase
    // pthread_join(accept_thread, NULL);
    pause();
    cleanup_client_threadpool();  

    return 0;
}