// Each thread handles one client at a time, performing lightweight tasks like authentication and command parsing,
// then passing work to Member B's Worker Threadpool via the Task Queue.

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include "client_threadpool.h" 
#include "commands.h"           // For auth and command handling 

// to manage threads and sockets
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;
pthread_t threads[THREAD_POOL_SIZE];        // holds thread IDs
int thread_active[THREAD_POOL_SIZE];        // Tracks if threads are running i.e status
int socket_queue[THREAD_POOL_SIZE];         // Placeholder Client Queue
int queue_size = 0;                         // Number of sockets in queue
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for thread safety

// ***************** member B
// Placeholder to dequeue a socket (to be replaced by Member B)
int dequeue_socket() {
    pthread_mutex_lock(&queue_mutex);
    while (queue_size == 0) {
        pthread_cond_wait(&queue_cond, &queue_mutex);
    }
    queue_size--;
    int sock = socket_queue[queue_size];
    pthread_mutex_unlock(&queue_mutex);
    return sock;
}

// Initialize the threadpool
void init_client_threadpool() {
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        thread_active[i] = 1;  
        if (pthread_create(&threads[i], NULL, client_thread, (void*)(intptr_t)i) != 0) {
            perror("**** client thread creation failed");
            exit(EXIT_FAILURE);
        }
    }
    printf("Client threadpool initialized with %d threads\n", THREAD_POOL_SIZE);
}

// Client thread function
void* client_thread(void* arg) {
    int thread_id = (intptr_t)arg;
    while (thread_active[thread_id]) {
        int sockfd = dequeue_socket();
        if (sockfd != -1) {
            printf("Thread %d handling socket %d\n", thread_id, sockfd);
            
            // Create session for this connection (starts unauthenticated)
            ClientSession session = {0};
            
            // *** COMMAND LOOP - no upfront authentication required ***
            while (1) {
                char buffer[1024] = {0};
                int bytes_read = read(sockfd, buffer, sizeof(buffer) - 1);
                
                if (bytes_read <= 0) {
                    printf("Thread %d detected socket %d closed (bytes read: %d)\n", 
                           thread_id, sockfd, bytes_read);
                    break;
                }
                
                buffer[bytes_read] = '\0';
                // Remove trailing newline if present
                buffer[strcspn(buffer, "\n")] = '\0';
                
                if (strlen(buffer) > 0) {
                    printf("Command received on socket %d: %s (bytes read: %d)\n", 
                           sockfd, buffer, bytes_read);
                    // Pass session to handle_commands for state tracking
                    handle_commands(sockfd, buffer, &session);
                } else {
                    break; // Exit loop if no more data
                }
            }
            
            close(sockfd); // Close socket after processing
        }
        // *** FIX 3: SHORTER SLEEP ***
        usleep(100000); // 0.1 second instead of 1 second
    }
    return NULL;
}

// Cleanup function (basic for now)
void cleanup_client_threadpool() {
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        thread_active[i] = 0; // Signal threads to stop
    }
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_join(threads[i], NULL); // Wait for threads to finish
    }
    printf("Client threadpool cleaned up\n");
}