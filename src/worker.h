// src/worker.h

// ---------------------------------------------------------------------------
// Defines args for worker threads and the entry function (worker_func).
// Workers consume from task_queue, execute commands, and loop until shutdown.
// Each gets unique ID for debug; passes queue + metadata.
// Usage: Create pool in main: pthread_create(..., worker_func, &wargs).
// ---------------------------------------------------------------------------

#ifndef WORKER_H
#define WORKER_H

#include "queue.h"
#include "metadata.h"
#include <pthread.h>

typedef struct {
    queue_t* task_queue;
    metadata_t* metadata;
    int id;  // Worker #1, #2, ...
} worker_args_t;

// Global shutdown flag
extern volatile int shutdown_flag;

// Functions
void* worker_func(void* args);

#endif