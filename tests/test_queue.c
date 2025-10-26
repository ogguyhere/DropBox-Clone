#include "../src/queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// Simple test to validate enqueue/dequeue correctness
int main(void) {
    queue_t *queue = queue_init();
    if (!queue) {
        fprintf(stderr, "Queue initialization failed.\n");
        return 1;
    }

    // Create and enqueue a few dummy tasks
    for (int i = 0; i < 5; i++) {
        task_t *t = malloc(sizeof(task_t));
        if (!t) {
            perror("malloc");
            return 1;
        }

        t->cmd = UPLOAD;
        snprintf(t->username, sizeof(t->username), "user%d", i);
        snprintf(t->filename, sizeof(t->filename), "file%d.txt", i);
        t->file_size = 1024 * (i + 1);
        t->sock_fd = i;
        pthread_mutex_init(&t->lock, NULL);
        pthread_cond_init(&t->completed, NULL);
        t->done = 0;
        t->result = 0;

        if (queue_enqueue(queue, t) != 0) {
            fprintf(stderr, "enqueue_task failed for %d\n", i);
            return 1;
        }
    }

    printf("All tasks enqueued successfully.\n");

    // Dequeue all and verify order
    for (int i = 0; i < 5; i++) {
        task_t *t = NULL;
        if (queue_dequeue(queue, &t) != 0 || !t) {
            fprintf(stderr, "dequeue_task failed at %d\n", i);
            return 1;
        }

        printf("Dequeued task: username=%s, filename=%s, sock=%d, size=%zu\n",
               t->username, t->filename, t->sock_fd, t->file_size);

        pthread_mutex_destroy(&t->lock);
        pthread_cond_destroy(&t->completed);
        free(t);
    }

    queue_destroy(queue);
    printf("Queue destroyed successfully.\n");
    return 0;
}
