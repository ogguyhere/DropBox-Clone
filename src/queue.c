#include "queue.h"
#include <stdlib.h>

queue_t *queue_init() {
    queue_t *q = malloc(sizeof(queue_t));
    q->head = q->tail = NULL;
    q->size = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->cond, NULL);
    return q;
}

void queue_destroy(queue_t *q) {
    node_t *cur = q->head;
    while (cur) {
        node_t *tmp = cur;
        cur = cur->next;
        free(tmp);
    }
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->cond);
    free(q);
}

int queue_enqueue(queue_t *q, task_t *task) {
    node_t *n = malloc(sizeof(node_t));
    if (!n) return -1;
    n->task = task;
    n->next = NULL;

    pthread_mutex_lock(&q->lock);
    if (!q->tail) {
        q->head = q->tail = n;
    } else {
        q->tail->next = n;
        q->tail = n;
    }
    q->size++;
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->lock);
    return 0;
}

int queue_dequeue(queue_t *q, task_t **task, volatile int *stop_flag) {
    pthread_mutex_lock(&q->lock);
    while (q->size == 0 && !(*stop_flag)) {
        pthread_cond_wait(&q->cond, &q->lock);
    }

    if (q->size == 0 && *stop_flag) {
        *task = NULL;
        pthread_mutex_unlock(&q->lock);
        return -1; // shutdown
    }

    node_t *n = q->head;
    *task = n->task;
    q->head = n->next;
    if (!q->head) q->tail = NULL;
    q->size--;
    free(n);
    pthread_mutex_unlock(&q->lock);
    return 0;
}
