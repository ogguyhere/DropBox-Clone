// src/queue.h

// ---------------------------------------------------------------------------
// Defines the queue_t struct and API for producer-consumer queue
// used link list for dynamic sizing 
// protected by mutex + conditional variable 
// concurrent enqueues from client threads and dequeues from workers 
// Core of the architecture—prevents race conditions on shared task buffer
// no busy waiting rn (its a bonus)
//Mutex locks the whole queue; cond signals on enqueue to wake waiters
// ---------------------------------------------------------------------------

#ifndef QUEUE_H
#define QUEUE_H

#include "task.h"
#include <pthread.h>

typedef struct node {
    task_t task; //payload from task.h
    struct node* next; // linked list chain
} node_t; 

typedef struct {
    node_t* head; // front for dequeue
    node_t* tail; // back for enqueue
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