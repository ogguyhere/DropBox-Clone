#ifndef COMMANDS_H
#define COMMANDS_H

#include "queue.h"
#include "metadata.h"

typedef struct {
    int authenticated;
    char username[50];
} ClientSession;

void handle_commands(int sockfd, const char *buffer, ClientSession *session, 
                     queue_t *task_queue, metadata_t *metadata);

// ⚠️ Fix here: handle_client now takes pool objects
void handle_client(int client_sock, queue_t *task_queue, metadata_t *metadata);

#endif
