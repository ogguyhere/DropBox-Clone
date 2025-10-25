// src/worker.h

// ---------------------------------------------------------------------------
//  Defines args for worker threads and the entry function (worker_func).
//  Workers consume from task_queue, execute commands, and loop until shutdown.
//  Each gets unique ID for debug; passes queue + future metadata.
//  Usage: Create pool in main/test: pthread_create(..., worker_func, &wargs).
// ---------------------------------------------------------------------------

#ifndef WORKER_H
#define WORKER_H

#include "queue.h"
#include "metadata.h"
#include <pthread.h>

typedef struct{
    queue_t* task_queue;
    int id; // Worrker #1,#2, .....
    metadata_t* metadata; //added metadata 

} workers_args_t;

//Funtions

void* worker_func(void* args);

#endif