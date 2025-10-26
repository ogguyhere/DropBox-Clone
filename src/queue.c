// src/queue.c

// ---------------------------------------------------------------------------
// Implements the queue API using a singly linked list of heap-allocated task_t pointers.
// Enqueue: Lock, add to tail, signal cond, unlock.
// Dequeue: Lock, wait on cond if empty, pop head, unlock, return pointer.
// Error Handling: Malloc fails return -1.
// Memory: Nodes freed on dequeue; destroy frees all remaining nodes and tasks.
// ---------------------------------------------------------------------------

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
        if (curr->task) free(curr->task);
        free(curr);
        curr = next;
    }
    pthread_mutex_unlock(&q->lock);
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->cond);
    free(q);
}

int queue_enqueue(queue_t *q, task_t *task)
{
    if (!q || !task)
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

int queue_dequeue(queue_t *q, task_t **out_task)
{
    if (!q || !out_task)
        return -1;
    pthread_mutex_lock(&q->lock);
    while (q->size == 0)
    {
        pthread_cond_wait(&q->cond, &q->lock); // block until something enqueued
    }
    node_t *front = q->head;
    if (!front)
    {
        pthread_mutex_unlock(&q->lock);
        return -1;
    }
    *out_task = front->task; // return pointer to heap-allocated task
    q->head = front->next;
    if (!q->head)
        q->tail = NULL; // last item
    q->size--;
    pthread_mutex_unlock(&q->lock);
    free(front); // free node, not the task
    return 0;
}
