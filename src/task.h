// src/task.h 

// ---------------------------------------------------------------------------
// This header defines the task_t struct used to package client commands.
// It contains cmd, username, filename, file_size, and a socket.
// Workers dequeue it and execute based on cmd_t enum.
// ---------------------------------------------------------------------------

#ifndef TASK_H 
#define TASK_H

#include <stddef.h>

typedef enum {
    UPLOAD, 
    DOWNLOAD, 
    DELETE,
    LIST,
    SIGNUP,
    LOGIN
} cmd_t;

typedef struct {
    cmd_t cmd; 
    char username[64]; 
    char filename[256];
    size_t file_size;     // e.g 1024 bytes (0 if no upload)
    int sock_fd;          // Client socket for results
    char data[8192];      // For file content (base64 encoded)
} task_t; 

#endif