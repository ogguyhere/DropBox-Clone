// src/client_threadpool.c

// Each thread handles one client at a time, performing lightweight tasks like authentication and command parsing,
// then passing work to Member B's Worker Threadpool via the Task Queue.

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include "client_threadpool.h" 
#include "commands.h"

// Dequeue a socket from the simple queue
static int dequeue_socket(client_threadpool_t *pool) {
    pthread_mutex_lock(&pool->queue_mutex);
    while (pool->queue_size == 0) {
        pthread_cond_wait(&pool->queue_cond, &pool->queue_mutex);
    }
    pool->queue_size--;
    int sock = pool->socket_queue[pool->queue_size];
    pthread_mutex_unlock(&pool->queue_mutex);
    return sock;
}

// Client thread function
static void* client_thread(void* arg) {
    client_threadpool_t *pool = (client_threadpool_t *)arg;
    
    // Find my thread ID
    int thread_id = -1;
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        if (pthread_equal(pthread_self(), pool->threads[i])) {
            thread_id = i;
            break;
        }
    }
    
    while (pool->thread_active[thread_id]) {
        int sockfd = dequeue_socket(pool);
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
                    // Pass session, task_queue, and metadata to handle_commands
                    handle_commands(sockfd, buffer, &session, pool->task_queue, pool->metadata);
                } else {
                    break; // Exit loop if no more data
                }
            }
            
            close(sockfd); // Close socket after processing
        }
        
        usleep(100000); // 0.1 second sleep
    }
    return NULL;
}

// Initialize the threadpool
client_threadpool_t* init_client_threadpool(queue_t *task_queue, metadata_t *metadata) {
    client_threadpool_t *pool = malloc(sizeof(client_threadpool_t));
    if (!pool) return NULL;
    
    pool->task_queue = task_queue;
    pool->metadata = metadata;
    pool->queue_size = 0;
    pthread_mutex_init(&pool->queue_mutex, NULL);
    pthread_cond_init(&pool->queue_cond, NULL);
    
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pool->thread_active[i] = 1;  
        if (pthread_create(&pool->threads[i], NULL, client_thread, pool) != 0) {
            perror("**** client thread creation failed");
            free(pool);
            return NULL;
        }
    }
    
    printf("Client threadpool initialized with %d threads\n", THREAD_POOL_SIZE);
    return pool;
}

// Enqueue socket to pool
void enqueue_socket(client_threadpool_t *pool, int sockfd) {
    pthread_mutex_lock(&pool->queue_mutex);
    if (pool->queue_size < THREAD_POOL_SIZE) {
        pool->socket_queue[pool->queue_size] = sockfd;
        pool->queue_size++;
        printf("Socket %d enqueued to threadpool (queue size: %d)\n", sockfd, pool->queue_size);
    } else {
        printf("***** Queue full, dropping socket %d\n", sockfd);
        close(sockfd);
    }
    pthread_mutex_unlock(&pool->queue_mutex);
    pthread_cond_signal(&pool->queue_cond);
}

// Cleanup function
void cleanup_client_threadpool(client_threadpool_t *pool) {
    if (!pool) return;
    
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pool->thread_active[i] = 0; // Signal threads to stop
    }
    
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_join(pool->threads[i], NULL); // Wait for threads to finish
    }
    
    pthread_mutex_destroy(&pool->queue_mutex);
    pthread_cond_destroy(&pool->queue_cond);
    free(pool);
    
    printf("Client threadpool cleaned up\n");
}