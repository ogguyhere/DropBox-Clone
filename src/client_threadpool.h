// src/client_threadpool.h

#ifndef CLIENT_THREADPOOL_H
#define CLIENT_THREADPOOL_H

#include "queue.h"
#include "metadata.h"
#include <pthread.h>

#define THREAD_POOL_SIZE 5

// Thread pool context
typedef struct {
    pthread_t threads[THREAD_POOL_SIZE];
    int thread_active[THREAD_POOL_SIZE];
    queue_t *task_queue;        // Reference to task queue for workers
    metadata_t *metadata;        // Reference to metadata
    
    // Simple socket queue (will be replaced by proper Client Queue in Phase 2)
    int socket_queue[THREAD_POOL_SIZE];
    int queue_size;
    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_cond;
} client_threadpool_t;

// Initialize and manage client threadpool
client_threadpool_t* init_client_threadpool(queue_t *task_queue, metadata_t *metadata);
void cleanup_client_threadpool(client_threadpool_t *pool);
void enqueue_socket(client_threadpool_t *pool, int sockfd);

#endif