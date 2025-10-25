// src/worker.c

// ---------------------------------------------------------------------------
// Each worker thread runs worker_func: Blocks on dequeue, executes task
// based on cmd_t, prints stubs (real I/O later). Infinite loop with flag check.(rn no flg check)
// ---------------------------------------------------------------------------

#include "worker.h"
#include <stdio.h>
#include <string.h>
#include "metadata.h" // Adding for metadata funcs
#include "file_io.h"  //file func add

// void *worker_func(void *args)
// {
//     workers_args_t *wargs = (workers_args_t *)args;
//     queue_t *q = wargs->task_queue;
//     printf("Worker %d started\n", wargs->id);

//     while (1)
//     { // infinite loop, will add exit flag later
//         task_t task;
//         if (queue_dequeue(q, &task) == 0)
//         {
//             // read cmds later on just dummy kind of shit rn
//             printf("Worker %d executing: %s for user %s (file: %s, size: %zu)\n",
//                    wargs->id,
//                    (
//                        task.cmd == UPLOAD ? "UPLOAD" :
//                        task.cmd == DELETE ? "DELETE":
//                        task.cmd == LIST     ? "LIST":
//                        task.cmd == DOWNLOAD ? "DOWNLOAD":
//                        task.cmd == SIGNUP   ? "SIGNUP" : "LOGIN"),
//                    task.username, task.filename, task.file_size);

//             // Stub handlers-replace with the real oness later on
//             if (task.cmd == DELETE)
//             {
//                 printf("  Stub: Deleting %s for %s\n", task.filename, task.username);
//                 // Later: unlink(filename), update quota
//             }
//             else if (task.cmd == LIST)
//             {
//                 printf("  Stub: Listing files for %s\n", task.username);
//                 // Later: opendir/readdir
//             }
//             else
//             {
//                 printf("  Stub: %s (not my command yet)\n", task.filename);
//             }

//             // TODO: Set result (e.g., success/error msg) for client thread
//         }
//         else
//         {
//             printf("Worker %d dequeue failed\n", wargs->id);
//             break; // Exit on error
//         }
//     }
//     printf("Worker %d exiting\n", wargs->id);
//     return NULL;
// }

// Global shutdown flag (defined here if not in .h; set by main)
volatile int shutdown_flag = 0;

// changes in function after implementation of meta data

void *worker_func(void *args)
{
    workers_args_t *wargs = (workers_args_t *)args;
    queue_t *q = wargs->task_queue;
    metadata_t *meta = wargs->metadata; // Grab once for convenience
    printf("Worker %d started\n", wargs->id);

    while (1)
    {
        // Stub flag check (expand later)
        if (shutdown_flag)
        {
            printf("Worker %d shutting down (flag set)\n", wargs->id);
            return NULL;
        }

        task_t task;
        if (queue_dequeue(q, &task) == 0)
        {
            // Step 1: Determine cmd_str for logging (full switch, no ternary cutoff)
            const char *cmd_str;
            switch (task.cmd)
            {
            case UPLOAD:
                cmd_str = "UPLOAD";
                break;
            case DOWNLOAD:
                cmd_str = "DOWNLOAD";
                break;
            case DELETE:
                cmd_str = "DELETE";
                break;
            case LIST:
                cmd_str = "LIST";
                break;
            case SIGNUP:
                cmd_str = "SIGNUP";
                break;
            case LOGIN:
                cmd_str = "LOGIN";
                break;
            default:
                cmd_str = "UNKNOWN";
                break;
            }

            // Log the execution
            printf("Worker %d executing: %s for user %s (file: %s, size: %zu)\n",
                   wargs->id, cmd_str, task.username, task.filename, task.file_size);

            // Step 2: Handle command with metadata
            if (task.cmd == DELETE)
            {
                create_user_dir(task.username);                                // Ensure dir
                size_t old_size = get_file_size(task.username, task.filename); // For quota
                int disk_res = delete_file(task.username, task.filename);
                if (disk_res == 0)
                {
                    // Sync metadata (remove + adjust quota)
                    metadata_remove_file(meta, task.username, task.filename); // Safe if not found
                    printf("  SUCCESS: Deleted %s for %s (quota freed: %zu bytes)\n", task.filename, task.username, old_size);
                }
                else
                {
                    printf("  ERROR: Disk delete failed for %s\n", task.filename);
                }
            }
            else if (task.cmd == LIST)
            {
                create_user_dir(task.username); // Ensure empty dir if new
                char list_out[1024] = {0};
                if (list_user_dir(task.username, list_out, sizeof(list_out)) == 0)
                {
                    printf("  %s", list_out);
                    // Optional: Sync metadata files from disk (Phase 2 full sync)
                }
                else
                {
                    printf("  ERROR: List failed for %s\n", task.username);
                }
            }
            else if (task.cmd == UPLOAD)
            {
                create_user_dir(task.username); // Ensure user dir exists
                int ok = metadata_check_quota(meta, task.username, task.file_size);
                if (ok)
                {
                    int res = metadata_add_file(meta, task.username, task.filename, task.file_size); // Declare res here
                    if (res == 0)
                    {
                        printf("  SUCCESS: UPLOAD added %s for %s (quota ok)\n", task.filename, task.username);
                        // TODO: Later - write file to disk (fopen/fwrite at FULL_PATH)
                    }
                    else
                    {
                        printf("  ERROR: UPLOAD failed - file limit for %s\n", task.username);
                    }
                }
                else
                {
                    printf("  ERROR: UPLOAD failed - quota exceeded for %s\n", task.username);
                }
            }
            else if (task.cmd == DOWNLOAD)
            {
                // Reuse existing funcs: Check existence + get size
                user_t *u;
                size_t file_size = 0;
                int found = 0;
                if (metadata_get_user(meta, task.username, &u) == 0)
                {
                    for (int i = 0; i < u->num_files; i++)
                    {
                        if (strcmp(u->files[i].filename, task.filename) == 0)
                        {
                            file_size = u->files[i].size;
                            found = 1;
                            break;
                        }
                    }
                }
                if (found)
                {
                    printf("  SUCCESS: DOWNLOAD %s for %s (size: %zu) - would stream from disk\n", task.filename, task.username, file_size);
                }
                else
                {
                    printf("  ERROR: DOWNLOAD failed - %s not found for %s\n", task.filename, task.username);
                }
            }
            else
            {
                // Fallback for SIGNUP/LOGIN (stubs—auth in client threads)
                printf("  SUCCESS: %s processed for %s\n", cmd_str, task.username);
            }

            // TODO: Set result (e.g., success/error msg) for client thread
            // (Phase 2: Package response, enqueue back to client via sock_fd)
        }
        else
        {
            printf("Worker %d dequeue failed\n", wargs->id);
            if (shutdown_flag)
                return NULL;
            break; // Exit on error
        }

        // Post-exec flag check
        if (shutdown_flag)
        {
            printf("Worker %d shutting down after task\n", wargs->id);
            return NULL;
        }
    }
    printf("Worker %d exiting\n", wargs->id);
    return NULL;
}