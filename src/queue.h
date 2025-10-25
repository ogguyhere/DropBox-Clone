// src/queue.h

#ifndef QUEUE_H
#define QUEUE_H

#include "task.h"
#include <pthread.h>

typedef struct node {
    task_t task; 
    struct node* next;
} node_t; 

typedef struct {
    node_t* head;
    node_t* tail;
    int size; 
    pthread_mutex_t lock;
    pthread_cond_t cond ; //Wakeup waiters when not empty 
} queue_t;

// Functions 
queue_t* queue_init(void);
void queue_destroy(queue_t* q);
int queue_enqueue(queue_t* q, task_t task);
int queue_dequeue(queue_t* q, task_t* task); //Blocks if it is empty

#endif