// src/worker.c

// ---------------------------------------------------------------------------
// Each worker thread runs worker_func: Blocks on dequeue, executes task
// based on cmd_t, handles actual file I/O. Infinite loop with flag check.
// ---------------------------------------------------------------------------

#include "worker.h"
#include "file_io.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Global shutdown flag
volatile int shutdown_flag = 0;

// ================= BASE64 DECODE =================
static int base64_decode(const char *in, unsigned char *out, int out_size) {
    static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int decode_table[256];
    memset(decode_table, -1, sizeof(decode_table));
    for (int i = 0; i < 64; i++)
        decode_table[(unsigned char)b64_table[i]] = i;

    int len = strlen(in);
    int val = 0, valb = -8, out_len = 0;

    for (int i = 0; i < len; i++) {
        unsigned char c = in[i];
        if (decode_table[c] == -1) continue;
        val = (val << 6) + decode_table[c];
        valb += 6;
        if (valb >= 0) {
            if (out_len >= out_size) break;
            out[out_len++] = (val >> valb) & 0xFF;
            valb -= 8;
        }
    }
    return out_len;
}

// ================= BASE64 ENCODE =================
static const char base64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int base64_encode(const unsigned char *in, int len, char *out, int out_size) {
    int out_len = 0;
    for (int i = 0; i < len; i += 3) {
        int val = (in[i] << 16) + ((i + 1 < len ? in[i + 1] : 0) << 8) + (i + 2 < len ? in[i + 2] : 0);
        if (out_len + 4 >= out_size) break;
        out[out_len++] = base64_table[(val >> 18) & 63];
        out[out_len++] = base64_table[(val >> 12) & 63];
        out[out_len++] = (i + 1 < len) ? base64_table[(val >> 6) & 63] : '=';
        out[out_len++] = (i + 2 < len) ? base64_table[val & 63] : '=';
    }
    out[out_len] = '\0';
    return out_len;
}

void* worker_func(void* args) {
    worker_args_t *wargs = (worker_args_t *)args;
    queue_t *q = wargs->task_queue;
    metadata_t *meta = wargs->metadata;
    
    printf("Worker %d started\n", wargs->id);

    while (1) {
        if (shutdown_flag) {
            printf("Worker %d shutting down (flag set)\n", wargs->id);
            return NULL;
        }

        task_t task;
        if (queue_dequeue(q, &task) != 0) {
            printf("Worker %d dequeue failed\n", wargs->id);
            break;
        }

        // Determine command string for logging
        const char *cmd_str;
        switch (task.cmd) {
            case UPLOAD:   cmd_str = "UPLOAD";   break;
            case DOWNLOAD: cmd_str = "DOWNLOAD"; break;
            case DELETE:   cmd_str = "DELETE";   break;
            case LIST:     cmd_str = "LIST";     break;
            case SIGNUP:   cmd_str = "SIGNUP";   break;
            case LOGIN:    cmd_str = "LOGIN";    break;
            default:       cmd_str = "UNKNOWN";  break;
        }

        printf("Worker %d executing: %s for user %s (file: %s, size: %zu)\n",
               wargs->id, cmd_str, task.username, task.filename, task.file_size);

        // Handle command execution
        if (task.cmd == UPLOAD) {
            create_user_dir(task.username);
            
            // Decode base64 data
            unsigned char decoded_data[8192];
            int decoded_len = base64_decode(task.data, decoded_data, sizeof(decoded_data));
            
            if (decoded_len <= 0) {
                write(task.sock_fd, "*** Error: Failed to decode file data\n", 39);
                continue;
            }
            
            // Check quota
            if (!metadata_check_quota(meta, task.username, decoded_len)) {
                write(task.sock_fd, "*** Error: Quota exceeded\n", 27);
                continue;
            }
            
            // Save to disk
            if (save_file(task.username, task.filename, decoded_data, decoded_len) != 0) {
                write(task.sock_fd, "*** Error: Failed to save file\n", 32);
                continue;
            }
            
            // Update metadata
            if (metadata_add_file(meta, task.username, task.filename, decoded_len) != 0) {
                write(task.sock_fd, "*** Error: Failed to update metadata\n", 38);
                continue;
            }
            
            write(task.sock_fd, "UPLOAD_SUCCESS\n", 15);
            printf("  SUCCESS: UPLOAD %s for %s (%d bytes)\n", task.filename, task.username, decoded_len);
            
        } else if (task.cmd == DOWNLOAD) {
            // Load file from disk
            unsigned char file_data[8192];
            size_t file_size = 0;
            
            if (load_file(task.username, task.filename, file_data, &file_size, sizeof(file_data)) != 0) {
                write(task.sock_fd, "*** Error: File not found on server\n", 37);
                continue;
            }
            
            // Encode to base64
            char encoded_data[12288];
            int encoded_len = base64_encode(file_data, file_size, encoded_data, sizeof(encoded_data));
            
            if (encoded_len <= 0) {
                write(task.sock_fd, "*** Error: Failed to encode file\n", 34);
                continue;
            }
            
            // Send encoded data
            write(task.sock_fd, encoded_data, encoded_len);
            printf("  SUCCESS: DOWNLOAD %s for %s (%zu bytes)\n", task.filename, task.username, file_size);
            
        } else if (task.cmd == DELETE) {
            create_user_dir(task.username);
            
            // Delete from disk
            if (delete_file(task.username, task.filename) != 0) {
                write(task.sock_fd, "*** Error: File not found\n", 27);
                continue;
            }
            
            // Update metadata
            metadata_remove_file(meta, task.username, task.filename);
            write(task.sock_fd, "DELETE_SUCCESS\n", 15);
            printf("  SUCCESS: DELETE %s for %s\n", task.filename, task.username);
            
        } else if (task.cmd == LIST) {
            create_user_dir(task.username);
            
            char list_output[2048];
            metadata_list_files(meta, task.username, list_output, sizeof(list_output));
            
            write(task.sock_fd, list_output, strlen(list_output));
            printf("  SUCCESS: LIST for %s\n", task.username);
        }

        // --- Signal task completion (Phase 2 addition) ---
        pthread_mutex_lock(&task.lock);
        task.done = 1;
        task.result = 0;  // or -1 if error handling was added
        pthread_cond_signal(&task.completed);
        pthread_mutex_unlock(&task.lock);


        // Check shutdown flag after task
        if (shutdown_flag) {
            printf("Worker %d shutting down after task\n", wargs->id);
            return NULL;
        }
    }
    
    printf("Worker %d exiting\n", wargs->id);
    return NULL;
}