// src/worker.c

#include "worker.h"
#include <stdio.h>
#include <string.h>

void *worker_func(void *args)
{
    workers_args_t *wargs = (workers_args_t *)args;
    queue_t *q = wargs->task_queue;
    printf("Worker %d started\n", wargs->id);

    while (1)
    { // infinite loop, will add exit flag later
        task_t task;
        if (queue_dequeue(q, &task) == 0)
        {
            // read cmds later on just dummy kind of shit rn
            printf("Worker %d executing: %s for user %s (file: %s, size: %zu)\n",
                   wargs->id,
                   (
                       task.cmd == UPLOAD ? "UPLOAD" : 
                       task.cmd == DELETE ? "DELETE": 
                       task.cmd == LIST     ? "LIST": 
                       task.cmd == DOWNLOAD ? "DOWNLOAD": 
                       task.cmd == SIGNUP   ? "SIGNUP" : "LOGIN"),
                   task.username, task.filename, task.file_size);

            // Stub handlers-replace with the real oness later on
            if (task.cmd == DELETE)
            {
                printf("  Stub: Deleting %s for %s\n", task.filename, task.username);
                // Later: unlink(filename), update quota
            }
            else if (task.cmd == LIST)
            {
                printf("  Stub: Listing files for %s\n", task.username);
                // Later: opendir/readdir
            }
            else
            {
                printf("  Stub: %s (not my command yet)\n", task.filename);
            }

            // TODO: Set result (e.g., success/error msg) for client thread
        }
        else
        {
            printf("Worker %d dequeue failed\n", wargs->id);
            break; // Exit on error
        }
    }
    printf("Worker %d exiting\n", wargs->id);
    return NULL;
}
