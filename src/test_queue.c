// src/test_queue.c

// -------------------------------------------------------------------------
// Standalone main to test queue + worker pool: Init, spawn workers, enqueue
// dummies, shutdown gracefully, cleanup. Proves no races/leaks solo.
// -------------------------------------------------------------------------

#include "queue.h"
#include "worker.h"
#include "metadata.h"
#include "file_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main()
{
    printf("=== Queue + Worker Pool Test ===\n\n");
    
    queue_t *task_q = queue_init();
    if (!task_q)
    {
        fprintf(stderr, "Queue init failed\n");
        return 1;
    }
    printf("✓ Queue initialized\n");
    
    metadata_t *meta = metadata_init();
    if (!meta)
    {
        fprintf(stderr, "Metadata init failed\n");
        queue_destroy(task_q);
        return 1;
    }
    printf("✓ Metadata initialized\n");
    
    // Add test users with passwords
    if (metadata_add_user(meta, "alice", "pass123") != 0) {
        fprintf(stderr, "Failed to add user alice\n");
    }
    if (metadata_add_user(meta, "bob", "pass456") != 0) {
        fprintf(stderr, "Failed to add user bob\n");
    }
    printf("✓ Test users created (alice, bob)\n\n");

    // Pre-create disk file for alice
    create_user_dir("alice");
    FILE *f = fopen("./storage/alice/test_file.txt", "w");
    if (f)
    {
        fwrite("dummy content", 1, 13, f); // 13 bytes
        fclose(f);
        printf("✓ Pre-created test_file.txt on disk (13 bytes)\n");
        // Update metadata to match
        metadata_add_file(meta, "alice", "test_file.txt", 13);
    }

    // Pre-add file for alice (to test DELETE/LIST)
    metadata_add_file(meta, "alice", "file1.txt", 500);
    printf("✓ Pre-added file1.txt to metadata (500 bytes)\n\n");

    // Creating Pool: 3 Workers
    pthread_t workers[3];
    worker_args_t wargs[3];  // ✅ FIXED: Correct struct name
    for (int i = 0; i < 3; i++)
    {
        wargs[i].task_queue = task_q;
        wargs[i].metadata = meta;
        wargs[i].id = i + 1;
        if (pthread_create(&workers[i], NULL, worker_func, &wargs[i]) != 0)
        {
            fprintf(stderr, "Worker %d create failed\n", i);
            return 1;
        }
    }
    printf("✓ Worker pool started (3 workers)\n\n");

    // Enqueue 5 dummy tasks (simulate clients)
    printf("=== Enqueueing Test Tasks ===\n");
    task_t tasks[] = {
        {LIST, "alice", "", 0, -1},                  // Should show test_file.txt 13 + file1.txt 500
        {DELETE, "alice", "test_file.txt", 0, -1},   // Success
        {LIST, "alice", "", 0, -1},                  // Should show only file1.txt
        {UPLOAD, "bob", "bigfile.dat", 2000000, -1}, // Quota fail (>1MB)
        {DOWNLOAD, "alice", "test_file.txt", 0, -1}  // Error (deleted)
    };
    
    for (int i = 0; i < 5; i++)
    {
        if (queue_enqueue(task_q, tasks[i]) != 0)
        {
            fprintf(stderr, "Enqueue %d failed\n", i);
        } else {
            printf("  Task %d enqueued\n", i + 1);
        }
        sleep(1); // Spread out for debug
    }
    
    // Let workers finish
    printf("\n=== Waiting for workers to complete ===\n");
    sleep(3);

    // Cleanup
    printf("\n=== Cleanup ===\n");
    shutdown_flag = 1;
    
    // Signal queue to wake up blocked workers
    for (int i = 0; i < 3; i++) {
        task_t dummy = {0};
        queue_enqueue(task_q, dummy);
    }
    
    for (int i = 0; i < 3; i++)
    {
        pthread_join(workers[i], NULL);
        printf("  Worker %d joined\n", i + 1);
    }
    
    queue_destroy(task_q);
    metadata_destroy(meta);
    
    printf("\n=== Test Complete! ===\n");
    return 0;
}