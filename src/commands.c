// src/commands.c
// ---------------------------------------------------------------------------
// Handles client commands: signup, login, upload, download, delete, list.
// Uses condition variables (no busy wait) for worker completion signaling.
// ---------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "commands.h"
#include "file_io.h"
#include "task.h"

// ================= BASE64 DECODE =================
static int base64_decode(const char *in, unsigned char *out, int out_size) {
    static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int decode_table[256];
    memset(decode_table, -1, sizeof(decode_table));
    for (int i = 0; i < 64; i++)
        decode_table[(unsigned char)b64_table[i]] = i;

    int val = 0, valb = -8, out_len = 0;
    for (int i = 0; in[i]; i++) {
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

static void send_response(int sockfd, const char *msg) {
    write(sockfd, msg, strlen(msg));
}

// ========================================================
// Helper to enqueue task and wait until worker completes
// Now uses heap-allocated task_t so worker and client share the same object.
// ========================================================
static void enqueue_and_wait(queue_t *queue, task_t *local_task) {
    if (!queue || !local_task) return;

    // Allocate heap task that will be shared with worker
    task_t *task = malloc(sizeof(task_t));
    if (!task) {
        fprintf(stderr, "Failed to allocate task\n");
        return;
    }
    // Copy contents
    *task = *local_task;

    // Init per-task sync
    pthread_mutex_init(&task->lock, NULL);
    pthread_cond_init(&task->completed, NULL);
    task->done = 0;
    task->result = -1;

    // Enqueue (queue takes ownership of 'task')
    if (queue_enqueue(queue, task) != 0) {
        fprintf(stderr, "Failed to enqueue task\n");
        pthread_mutex_destroy(&task->lock);
        pthread_cond_destroy(&task->completed);
        free(task);
        return;
    }

    // Wait for worker to signal completion on same task object
    pthread_mutex_lock(&task->lock);
    while (!task->done) {
        pthread_cond_wait(&task->completed, &task->lock);
    }
    pthread_mutex_unlock(&task->lock);

    // Capture result if needed (task->result)
    // Cleanup sync resources and free task (worker may also free in some designs;
    // here we free after waiting to ensure consistent ownership)
    pthread_mutex_destroy(&task->lock);
    pthread_cond_destroy(&task->completed);
    // free the task memory (worker should not free it in this design)
    free(task);
}

// ========================================================
// Account management commands
// ========================================================
static void handle_signup(int sockfd, char *username, char *password, ClientSession *session, metadata_t *metadata) {
    if (!username || !password) {
        send_response(sockfd, "*** Invalid format. Usage: signup <username> <password>\n");
        return;
    }

    int result = metadata_add_user(metadata, username, password);
    if (result == -2) {
        send_response(sockfd, "*** Error: User already exists\n");
        return;
    } else if (result != 0) {
        send_response(sockfd, "*** Error: Signup failed\n");
        return;
    }

    session->authenticated = 1;
    strncpy(session->username, username, sizeof(session->username) - 1);
    session->username[sizeof(session->username) - 1] = '\0';
    create_user_dir(username);
    send_response(sockfd, "Signup successful. You are now logged in.\n");
}

static void handle_login(int sockfd, char *username, char *password, ClientSession *session, metadata_t *metadata) {
    if (session->authenticated) {
        char msg[128];
        snprintf(msg, sizeof(msg), "*** Error: Already logged in as '%s'\n", session->username);
        send_response(sockfd, msg);
        return;
    }

    if (!username || !password) {
        send_response(sockfd, "*** Invalid format. Usage: login <username> <password>\n");
        return;
    }

    if (metadata_authenticate(metadata, username, password)) {
        session->authenticated = 1;
        strncpy(session->username, username, sizeof(session->username) - 1);
        session->username[sizeof(session->username) - 1] = '\0';
        send_response(sockfd, "Login successful\n");
    } else {
        send_response(sockfd, "*** Error: Invalid credentials\n");
    }
}

static void handle_logout(int sockfd, ClientSession *session) {
    if (!session->authenticated) {
        send_response(sockfd, "*** Error: Not logged in\n");
        return;
    }
    session->authenticated = 0;
    memset(session->username, 0, sizeof(session->username));
    send_response(sockfd, "Logged out successfully\n");
}

// ========================================================
// File operation handlers
// ========================================================
static void handle_upload(int sockfd, char *filename, ClientSession *session, queue_t *task_queue) {
    if (!session->authenticated) {
        send_response(sockfd, "*** Error: Please login first\n");
        return;
    }

    if (!filename) {
        send_response(sockfd, "*** Invalid format. Usage: UPLOAD <filename>\n");
        return;
    }

    send_response(sockfd, "READY_TO_RECEIVE\n");

    char encoded_data[8192];
    int bytes_read = read(sockfd, encoded_data, sizeof(encoded_data) - 1);
    if (bytes_read <= 0) {
        send_response(sockfd, "*** Error: Failed to receive file data\n");
        return;
    }
    encoded_data[bytes_read] = '\0';

    task_t local = {0};
    local.cmd = UPLOAD;
    strncpy(local.username, session->username, sizeof(local.username) - 1);
    strncpy(local.filename, filename, sizeof(local.filename) - 1);
    strncpy(local.data, encoded_data, sizeof(local.data) - 1);
    local.sock_fd = sockfd;
    local.file_size = bytes_read;

    enqueue_and_wait(task_queue, &local);
}

static void handle_download(int sockfd, char *filename, ClientSession *session, queue_t *task_queue) {
    if (!session->authenticated) {
        send_response(sockfd, "*** Error: Please login first\n");
        return;
    }

    if (!filename) {
        send_response(sockfd, "*** Invalid format. Usage: DOWNLOAD <filename>\n");
        return;
    }

    task_t local = {0};
    local.cmd = DOWNLOAD;
    strncpy(local.username, session->username, sizeof(local.username) - 1);
    strncpy(local.filename, filename, sizeof(local.filename) - 1);
    local.sock_fd = sockfd;

    enqueue_and_wait(task_queue, &local);
}

static void handle_delete(int sockfd, char *filename, ClientSession *session, queue_t *task_queue) {
    if (!session->authenticated) {
        send_response(sockfd, "*** Error: Please login first\n");
        return;
    }

    if (!filename) {
        send_response(sockfd, "*** Invalid format. Usage: DELETE <filename>\n");
        return;
    }

    task_t local = {0};
    local.cmd = DELETE;
    strncpy(local.username, session->username, sizeof(local.username) - 1);
    strncpy(local.filename, filename, sizeof(local.filename) - 1);
    local.sock_fd = sockfd;

    enqueue_and_wait(task_queue, &local);
}

static void handle_list(int sockfd, ClientSession *session, queue_t *task_queue) {
    if (!session->authenticated) {
        send_response(sockfd, "*** Error: Please login first\n");
        return;
    }

    task_t local = {0};
    local.cmd = LIST;
    strncpy(local.username, session->username, sizeof(local.username) - 1);
    local.sock_fd = sockfd;

    enqueue_and_wait(task_queue, &local);
}

// ========================================================
// Dispatcher
// ========================================================
void handle_commands(int sockfd, const char *buffer, ClientSession *session, 
                     queue_t *task_queue, metadata_t *metadata) {
    char command[10], arg1[50], arg2[50];
    int args = sscanf(buffer, "%9s %49s %49s", command, arg1, arg2);

    if (args < 1) {
        send_response(sockfd, "*** Invalid command\n");
        return;
    }

    if (strcmp(command, "signup") == 0) {
        handle_signup(sockfd, args >= 2 ? arg1 : NULL, args == 3 ? arg2 : NULL, session, metadata);
    } else if (strcmp(command, "login") == 0) {
        handle_login(sockfd, args >= 2 ? arg1 : NULL, args == 3 ? arg2 : NULL, session, metadata);
    } else if (strcmp(command, "logout") == 0) {
        handle_logout(sockfd, session);
    } else if (strcmp(command, "UPLOAD") == 0) {
        handle_upload(sockfd, args >= 2 ? arg1 : NULL, session, task_queue);
    } else if (strcmp(command, "DOWNLOAD") == 0) {
        handle_download(sockfd, args >= 2 ? arg1 : NULL, session, task_queue);
    } else if (strcmp(command, "DELETE") == 0) {
        handle_delete(sockfd, args >= 2 ? arg1 : NULL, session, task_queue);
    } else if (strcmp(command, "LIST") == 0) {
        handle_list(sockfd, session, task_queue);
    } else {
        send_response(sockfd, "*** Unknown command\n");
    }
}
