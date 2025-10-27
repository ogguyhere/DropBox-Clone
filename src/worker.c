// src/worker.c

// ---------------------------------------------------------------------------
// Each worker thread runs worker_func: Blocks on dequeue, executes task
// based on cmd_t, handles actual file I/O. Infinite loop with flag check.
// ---------------------------------------------------------------------------

#include "worker.h"
#include "task.h"
#include "file_io.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Global shutdown flag
// volatile int shutdown_flag = 0;

// ================= BASE64 DECODE =================
static int base64_decode(const char *in, unsigned char *out, int out_size)
{
    static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int decode_table[256];
    memset(decode_table, -1, sizeof(decode_table));
    for (int i = 0; i < 64; i++)
        decode_table[(unsigned char)b64_table[i]] = i;

    int len = strlen(in);
    int val = 0, valb = -8, out_len = 0;

    for (int i = 0; i < len; i++)
    {
        unsigned char c = in[i];
        if (decode_table[c] == -1)
            continue;
        val = (val << 6) + decode_table[c];
        valb += 6;
        if (valb >= 0)
        {
            if (out_len >= out_size)
                break;
            out[out_len++] = (val >> valb) & 0xFF;
            valb -= 8;
        }
    }
    return out_len;
}

// ================= BASE64 ENCODE =================
static const char base64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int base64_encode(const unsigned char *in, int len, char *out, int out_size)
{
    int out_len = 0;
    for (int i = 0; i < len; i += 3)
    {
        int val = (in[i] << 16) + ((i + 1 < len ? in[i + 1] : 0) << 8) + (i + 2 < len ? in[i + 2] : 0);
        if (out_len + 4 >= out_size)
            break;
        out[out_len++] = base64_table[(val >> 18) & 63];
        out[out_len++] = base64_table[(val >> 12) & 63];
        out[out_len++] = (i + 1 < len) ? base64_table[(val >> 6) & 63] : '=';
        out[out_len++] = (i + 2 < len) ? base64_table[val & 63] : '=';
    }
    out[out_len] = '\0';
    return out_len;
}

void *worker_func(void *args)
{
    worker_args_t *wargs = (worker_args_t *)args;
    queue_t *q = wargs->task_queue;
    metadata_t *meta = wargs->metadata;

    printf("Worker %d started\n", wargs->id);

    while (1) {
        task_t *task = NULL;

        if (queue_dequeue(q, &task, &shutdown_flag) != 0) {
            printf("Worker %d shutting down\n", wargs->id);
            break;
        }

        if (!task) continue;

        printf("  Worker %d: Processing %s for %s\n", wargs->id, 
               (task->cmd == UPLOAD ? "UPLOAD" : 
                task->cmd == DOWNLOAD ? "DOWNLOAD" : 
                task->cmd == DELETE ? "DELETE" : "LIST"), task->username);

        task->result = -1;  // Assume fail

        if (task->cmd == UPLOAD) {
            // Decode base64 (no lock needed)
            unsigned char dec_data[8192];
            size_t dec_size = base64_decode(task->data, dec_data, sizeof(dec_data));
            if (dec_size == 0 || dec_size != task->file_size) {
                write(task->sock_fd, "*** Error: Invalid data\n", 24);
                goto done;
            }

            // Get user & initial quota check (atomic under user_lock)
            user_t *u;
            if (metadata_get_user(meta, task->username, &u) != 0) {
                write(task->sock_fd, "*** Error: User not found\n", 26);
                goto done;
            }
            pthread_mutex_lock(&u->user_lock);
            if (u->quota_used + dec_size > u->quota_max) {
                pthread_mutex_unlock(&u->user_lock);
                write(task->sock_fd, "*** Error: Quota exceeded\n", 26);
                goto done;
            }
            pthread_mutex_unlock(&u->user_lock);  // Unlock for I/O (non-blocking)

            // I/O: Save to disk
            create_user_dir(task->username);
            if (save_file(task->username, task->filename, dec_data, dec_size) != 0) {
                write(task->sock_fd, "*** Error: Save failed\n", 23);
                goto done;
            }

            // Re-lock for atomic metadata (recheck quota TOCTOU-safe)
            pthread_mutex_lock(&u->user_lock);
            if (u->quota_used + dec_size > u->quota_max) {  // Rare, but concurrent quota change?
                pthread_mutex_unlock(&u->user_lock);
                delete_file(task->username, task->filename);  // Rollback I/O
                write(task->sock_fd, "*** Error: Quota exceeded after save\n", 37);
                goto done;
            }
            int add_ret = metadata_add_file(meta, task->username, task->filename, dec_size);
            pthread_mutex_unlock(&u->user_lock);
            if (add_ret != 0) {
                delete_file(task->username, task->filename);  // Rollback I/O on metadata fail
                write(task->sock_fd, "*** Error: Metadata update failed\n", 32);
                goto done;
            }

            write(task->sock_fd, "UPLOAD_SUCCESS\n", 15);
            printf("  SUCCESS: UPLOAD %s for %s (%zu bytes)\n", task->filename, task->username, dec_size);
            task->result = 0;
        }
        else if (task->cmd == DOWNLOAD)
        {
            file_t *file = metadata_get_and_lock_file(meta, task->username, task->filename);

            if (!file)
            {
                write(task->sock_fd, "*** Error: File not found\n", 27);
                goto done;
            }

            size_t file_size = get_file_size(task->username, task->filename);
            if (file_size == 0) {
                metadata_unlock_file(file);
                write(task->sock_fd, "*** Error: Empty file\n", 22);
                goto done;
            }

            unsigned char data[8192];
            if (load_file(task->username, task->filename, data, (size_t*)&file_size, sizeof(data)) != 0) {
                metadata_unlock_file(file);
                write(task->sock_fd, "*** Error: Load failed\n", 23);
                goto done;
            }

            metadata_unlock_file(file);  // Unlock before encode/send

            char encoded_data[10922];  // 4/3 overhead + null
            int encoded_len = base64_encode(data, file_size, encoded_data, sizeof(encoded_data));
            if (encoded_len <= 0) {
                write(task->sock_fd, "*** Error: Failed to encode\n", 29);
                goto done;
            }

            write(task->sock_fd, encoded_data, encoded_len);
            printf("  SUCCESS: DOWNLOAD %s for %s (%zu bytes)\n", task->filename, task->username, file_size);
            task->result = 0;
        }
        else if (task->cmd == DELETE)
        {
            create_user_dir(task->username);
            // lock before deleting
            file_t *file = metadata_get_and_lock_file(meta, task->username, task->filename);

            if (!file)
            {
                write(task->sock_fd, "*** Error: File not found\n", 27);
                goto done;
            }

            // Delete from disk
            if (delete_file(task->username, task->filename) != 0)
            {
                metadata_unlock_file(file); // ⬅️ Unlock on error
                write(task->sock_fd, "*** Error: Failed to delete file\n", 34);
                goto done;
            }
            // Unlock BEFORE removing(metadata_remove will destroy the lock)
            metadata_unlock_file(file);
            //remove from metadata (this detroys the lock)
            int remove_ret = metadata_remove_file(meta, task->username, task->filename);
            if (remove_ret != 0) {
                // Rare rollback: But I/O already gone—log error, quota safe (idempotent)
                write(task->sock_fd, "*** Error: Metadata cleanup failed\n", 35);
                goto done;
            }
            write(task->sock_fd, "DELETE_SUCCESS\n", 15);
            printf("  SUCCESS: DELETE %s for %s\n", task->filename, task->username);
            task->result = 0;
        }
        else if (task->cmd == LIST)
        {
            create_user_dir(task->username);

            char list_output[2048];
            metadata_list_files(meta, task->username, list_output, sizeof(list_output));

            write(task->sock_fd, list_output, strlen(list_output));
            printf("  SUCCESS: LIST for %s\n", task->username);
            task->result = 0;
        }

        // --- Signal task completion ---
        done:
        pthread_mutex_lock(&task->lock);
        if (task->result != -1)
            task->result = 0;
        task->done = 1;
        pthread_cond_signal(&task->completed);
        pthread_mutex_unlock(&task->lock);
    }

    printf("Worker %d exiting\n", wargs->id);
    return NULL;
}