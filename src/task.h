// src/task.h 

// ---------------------------------------------------------------------------
// This header define the task_t struct used to package client commands, It contains cmd, username, filename, file_size, and a socket. Workers deque it and execute the cmd_t enum. 
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
    size_t file_size; // e.g 1024 bytes (0 if no upload)
    int sock_fd;  // Client socket for results (-1 stub s)

    //Later : char * data for upload content 

} task_t; 

#endif