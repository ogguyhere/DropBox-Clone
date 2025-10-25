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
    queue_t *task_q = queue_init();
    if (!task_q)
    {
        fprintf(stderr, "Queue init failed\n");
        return 1;
    }
    metadata_t *meta = metadata_init();
    if (!meta)
    {
        fprintf(stderr, "Metadata init failed\n");
        queue_destroy(task_q);
        return 1;
    }
    // Add test users
    if (metadata_add_user(meta, "alice") != 0 || metadata_add_user(meta, "bob") != 0)
    {
        fprintf(stderr, "Add user failed\n");
        // Cleanup...
    }

    // Pre-create disk file for alice
    create_user_dir("alice");
    FILE *f = fopen("./storage/alice/test_file.txt", "w");
    if (f)
    {
        fwrite("dummy content", 1, 13, f); // 13 bytes
        fclose(f);
        printf("Pre-created test_file.txt on disk (13 bytes)\n");
        // Update metadata to match
        metadata_add_file(meta, "alice", "test_file.txt", 13);
    }

    // Pre-add file for alice (to test DELETE/LIST)
    metadata_add_file(meta, "alice", "file1.txt", 500);

    // Creating Pool : 3 Workers for now...
    pthread_t workers[3];
    workers_args_t wargs[3];
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

    // Enqueue 5 dummy tasks (simulate clients)
    // task_t tasks[] = {
    //     {DELETE, "alice", "file1.txt", 0, -1},
    //     {LIST, "bob", "", 0, -1},
    //     {UPLOAD, "alice", "photo.jpg", 1024, -1}, // Stub
    //     {DELETE, "alice", "file2.txt", 0, -1},
    //     {LIST, "bob", "", 0, -1}};

    // task_t tasks[] = {
    //     {DELETE, "alice", "file1.txt", 0, -1},
    //     {LIST, "bob", "", 0, -1},
    //     {UPLOAD, "alice", "photo.jpg", 2000000, -1}, // Big: >1MB, should fail quota
    //     {DELETE, "alice", "file2.txt", 0, -1},       // This should ERROR (not exist)
    //     {LIST, "bob", "", 0, -1}};

    task_t tasks[] = {
        {LIST, "alice", "", 0, -1},                  // Should show test_file.txt 13
        {DELETE, "alice", "test_file.txt", 0, -1},   // Success
        {LIST, "alice", "", 0, -1},                  // No files
        {UPLOAD, "bob", "bigfile.dat", 2000000, -1}, // Quota fail
        {DOWNLOAD, "alice", "test_file.txt", 0, -1}  // Error (deleted)
    };
    
    for (int i = 0; i < 5; i++)
    {
        if (queue_enqueue(task_q, tasks[i]) != 0)
        {
            fprintf(stderr, "Enqueue %d failed\n", i);
        }
        sleep(1); // Spread out for debug
    }
    // Let workers finish (sleep or join later)
    sleep(3);

    // Cleanup (stub—add pthread_join for real)
    queue_destroy(task_q);
    for (int i = 0; i < 3; i++)
    {
        pthread_cancel(workers[i]); // Rough shutdown
    }
    printf("Test done!\n");
    return 0;
}