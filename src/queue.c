// src/queue.c

#include "queue.h"
#include <stdlib.h>
#include <stdio.h>

queue_t *queue_init(void)
{
    queue_t *q = malloc(sizeof(queue_t));
    if (!q)
        return NULL;
    q->head = q->tail = NULL;
    q->size = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->cond, NULL);
    return q;
}

void queue_destroy(queue_t *q)
{
    if (!q)
        return;
    pthread_mutex_lock(&q->lock);
    node_t *curr = q->head;
    while (curr)
    {
        node_t *next = curr->next;
        free(curr);
        curr = next;
    }
    pthread_mutex_unlock(&q->lock);
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->cond);
    free(q);
}

int queue_enqueue(queue_t *q, task_t task)
{
    if (!q)
        return -1;
    node_t *new_node = malloc(sizeof(node_t));
    if (!new_node)
        return -1;
    new_node->task = task;
    new_node->next = NULL;

    pthread_mutex_lock(&q->lock);
    if (q->tail)
    {
        q->tail->next = new_node;
    }
    else
    {
        q->head = new_node;
    }
    q->tail = new_node;
    q->size++;
    pthread_mutex_unlock(&q->lock);
    pthread_cond_signal(&q->cond); // wakeup the waiter
    return 0;
}

int queue_dequeue(queue_t *q, task_t *task)
{
    if (!q || !task)
        return -1;
    pthread_mutex_lock(&q->lock);
    while (q->size == 0)
    { // Busy wait? No block on cond
        pthread_cond_wait(&q->cond, &q->lock);
    }
    node_t *front = q->head;
    if (!front)
    {
        pthread_mutex_unlock(&q->lock);
        return -1;
    }
    *task = front->task;
    q->head = front->next;
    if (!q->head)
        q->tail = NULL;
    q->size--;
    pthread_mutex_unlock(&q->lock);
    free(front);
    return 0;
}