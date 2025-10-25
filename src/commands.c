// It handles the processing of client requests after a socket is passed to a thread from the Client Threadpool.
// It acts as the intermediary between the client’s input and the server’s response, managing authentication and command execution.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "commands.h"

typedef struct {
    char username[50];
    char password[50];
} User;
User users[10];
int user_count = 0;

void handle_authentication(int sockfd) {
    char buffer[1024];
    int bytes_read = read(sockfd, buffer, 1024);
    if (bytes_read > 0) {
        buffer[strcspn(buffer, "\n")] = 0; // Remove newline
        printf("Authentication request on socket %d: %s (bytes read: %d)\n", sockfd, buffer, bytes_read);

        char command[10], username[50], password[50];
        if (sscanf(buffer, "%9s %49s %49s", command, username, password) == 3) {
            if (strcmp(command, "signup") == 0) {
                for (int i = 0; i < user_count; i++) {
                    if (strcmp(users[i].username, username) == 0) {
                        write(sockfd, "User already exists\n", 20);
                        return;
                    }
                }
                if (user_count < 10) {
                    strncpy(users[user_count].username, username, 49);
                    users[user_count].username[49] = '\0';
                    strncpy(users[user_count].password, password, 49);
                    users[user_count].password[49] = '\0';
                    user_count++;
                    write(sockfd, "Signup successful\n", 18);
                } else {
                    write(sockfd, "User limit reached\n", 19);
                }
            } else if (strcmp(command, "login") == 0) {
                for (int i = 0; i < user_count; i++) {
                    if (strcmp(users[i].username, username) == 0 && 
                        strcmp(users[i].password, password) == 0) {
                        write(sockfd, "Login successful\n", 17);
                        return;
                    }
                }
                write(sockfd, "*** Invalid credentials\n", 23);
            } else {
                write(sockfd, "*** Unknown authentication command\n", 34);
            }
        } else {
            write(sockfd, "*** Invalid format\n", 18);
        }
    } else {
        printf("***** Failed to read authentication on socket %d (bytes read: %d)\n", sockfd, bytes_read);
    }
}

void handle_commands(int sockfd, const char *buffer) {
    printf("Command received on socket %d: %s (processed)\n", sockfd, buffer);

    char command[10], filename[256];
    if (sscanf(buffer, "%9s %255s", command, filename) == 2) {
        if (strcmp(command, "UPLOAD") == 0) {
            write(sockfd, "Upload command received\n", 23);
        } else if (strcmp(command, "DOWNLOAD") == 0) {
            write(sockfd, "Download command received\n", 25);
        } else {
            write(sockfd, "*** Unknown command\n", 19);
        }
    } else {
        write(sockfd, "*** Invalid command format\n", 26);
    }
}