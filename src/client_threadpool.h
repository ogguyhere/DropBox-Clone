#ifndef CLIENT_THREADPOOL_H
#define CLIENT_THREADPOOL_H

#include <pthread.h>
#include "queue.h"
#include "metadata.h"

#define MAX_CLIENT_QUEUE 100

typedef struct {
    int queue[MAX_CLIENT_QUEUE];
    int front, rear, count;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
} client_queue_t;

typedef struct {
    client_queue_t client_queue;
    pthread_t *threads;
    int num_threads;
    int stop;


    queue_t *task_queue;     
    metadata_t *metadata;   

} client_threadpool_t;

// API
client_threadpool_t *init_client_threadpool(queue_t *task_queue, metadata_t *metadata);
void enqueue_socket(client_threadpool_t *pool, int client_sock);
void cleanup_client_threadpool(client_threadpool_t *pool);
 
void handle_client(int client_sock, queue_t *task_queue, metadata_t *metadata);


#endif
