#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include "task.h"   // include task_t definition
// typedef struct task task_t;

typedef struct node {
    task_t *task;
    struct node *next;
} node_t;

typedef struct {
    node_t *head;
    node_t *tail;
    int size;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} queue_t;

queue_t *queue_init();
void queue_destroy(queue_t *q);
int queue_enqueue(queue_t *q, task_t *task);
int queue_dequeue(queue_t *q, task_t **task,_Atomic int *stop_flag); // stop_flag support

#endif
