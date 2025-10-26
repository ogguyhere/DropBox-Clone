#include "client_threadpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "commands.h"

static void *client_worker(void *arg);


// ---------------- Queue Operations ----------------
static void enqueue(client_queue_t *q, int client_sock) {
    pthread_mutex_lock(&q->lock);
    if (q->count == MAX_CLIENT_QUEUE) {
        printf("Client queue full! Dropping connection %d\n", client_sock);
        close(client_sock);
    } else {
        q->queue[q->rear] = client_sock;
        q->rear = (q->rear + 1) % MAX_CLIENT_QUEUE;
        q->count++;
        pthread_cond_signal(&q->not_empty);
    }
    pthread_mutex_unlock(&q->lock);
}

// static int dequeue(client_queue_t *q, int stop_flag) {
//     pthread_mutex_lock(&q->lock);
//     while (q->count == 0 && !stop_flag) {
//         pthread_cond_wait(&q->not_empty, &q->lock);
//     }

//     if (q->count == 0 && stop_flag) {
//         pthread_mutex_unlock(&q->lock);
//         return -1; // exit thread
//     }

//     int client_sock = q->queue[q->front];
//     q->front = (q->front + 1) % MAX_CLIENT_QUEUE;
//     q->count--;
//     pthread_mutex_unlock(&q->lock);
//     return client_sock;
// }

// static void *client_worker(void *arg) {
//     client_threadpool_t *pool = (client_threadpool_t *)arg;

//     while (1) {
//         int client_sock = dequeue(&pool->client_queue, pool->stop);
//         if (client_sock <= 0) break; // shutdown exit

//         handle_client(client_sock, pool->task_queue, pool->metadata);
//         close(client_sock);
//     }

//     return NULL;
// } 


void enqueue_socket(client_threadpool_t *pool, int client_sock) {
    enqueue(&pool->client_queue, client_sock);
}

static int dequeue(client_queue_t *q, volatile int *stop_flag) {
    pthread_mutex_lock(&q->lock);
    while (q->count == 0 && !(*stop_flag)) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }
    if (q->count == 0 && *stop_flag) {
        pthread_mutex_unlock(&q->lock);
        return -1;
    }
    int sock = q->queue[q->front];
    q->front = (q->front + 1) % MAX_CLIENT_QUEUE;
    q->count--;
    pthread_mutex_unlock(&q->lock);
    return sock;
}

static void *client_worker(void *arg) {
    client_threadpool_t *pool = (client_threadpool_t *)arg;
    while (1) {
        int client_sock = dequeue(&pool->client_queue, &pool->stop);
        if (client_sock <= 0) break;
        handle_client(client_sock, pool->task_queue, pool->metadata);
        close(client_sock);
    }
    return NULL;
}

void cleanup_client_threadpool(client_threadpool_t *pool) {
    if (!pool) return;

    // Set stop flag
    pool->stop = 1;

    // Wake up any threads waiting on the client queue
    pthread_mutex_lock(&pool->client_queue.lock);
    pthread_cond_broadcast(&pool->client_queue.not_empty);
    pthread_mutex_unlock(&pool->client_queue.lock);

    // Join all worker threads
    for (int i = 0; i < pool->num_threads; i++)
        pthread_join(pool->threads[i], NULL);

    free(pool->threads);
    pthread_mutex_destroy(&pool->client_queue.lock);
    pthread_cond_destroy(&pool->client_queue.not_empty);
    free(pool);
}



// ------------------ Threadpool API ------------------
client_threadpool_t *init_client_threadpool(queue_t *task_queue, metadata_t *metadata) {
    client_threadpool_t *pool = malloc(sizeof(client_threadpool_t));
    if (!pool) {
        fprintf(stderr, "Failed to allocate client threadpool\n");
        return NULL;
    }

    pool->num_threads = 5;  // set desired client worker threads
    pool->stop = 0;
    pool->task_queue = task_queue;
    pool->metadata = metadata;
    pool->threads = malloc(sizeof(pthread_t) * pool->num_threads);
    if (!pool->threads) {
        fprintf(stderr, "Failed to allocate client threads\n");
        free(pool);
        return NULL;
    }

    pool->client_queue.front = pool->client_queue.rear = pool->client_queue.count = 0;
    pthread_mutex_init(&pool->client_queue.lock, NULL);
    pthread_cond_init(&pool->client_queue.not_empty, NULL);

    for (int i = 0; i < pool->num_threads; i++) {
        if (pthread_create(&pool->threads[i], NULL, client_worker, pool) != 0) {
            fprintf(stderr, "Failed to create client worker thread %d\n", i);
        }
    }

    return pool;
}
