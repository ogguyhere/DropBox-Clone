#ifndef COMMANDS_H
#define COMMANDS_H

// Session structure to track authentication state per connection
typedef struct {
    int authenticated;      // 0 = not authenticated, 1 = authenticated
    char username[50];      // stores the authenticated username
} ClientSession;

// Handles all commands including authentication (signup/login) and file operations
void handle_commands(int sockfd, const char *buffer, ClientSession *session);

#endif
 