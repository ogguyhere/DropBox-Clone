#ifndef COMMANDS_H
#define COMMANDS_H

// Define constants or other includes if needed

// Function prototypes
void handle_authentication(int sockfd);
void handle_commands(int sockfd, const char *buffer); // Updated to match new signature

#endif