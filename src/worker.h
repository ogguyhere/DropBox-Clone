// src/worker.h

#ifndef WORKER_H
#define WORKER_H

#include "queue.h"
#include <pthread.h>

typedef struct{
    queue_t* task_queue;
    int id; // Worrker #1,#2, .....

} workers_args_t;

//Funtions

void* worker_func(void* args);

#endif