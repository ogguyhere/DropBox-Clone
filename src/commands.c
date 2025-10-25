// It handles the processing of client requests after a socket is passed to a thread from the Client Threadpool.
// It acts as the intermediary between the client's input and the server's response, managing authentication and command execution.

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

void handle_commands(int sockfd, const char *buffer, ClientSession *session) {
    printf("Command received on socket %d: %s (processed)\n", sockfd, buffer);

    char command[10], username[50], password[50];
    int args = sscanf(buffer, "%9s %49s %49s", command, username, password);
    
    // ==================== AUTHENTICATION COMMANDS ====================
    
    if (strcmp(command, "signup") == 0) {
        if (args != 3) {
            write(sockfd, "*** Invalid format. Usage: signup <username> <password>\n", 56);
            return;
        }
        
        // Check if user already exists
        for (int i = 0; i < user_count; i++) {
            if (strcmp(users[i].username, username) == 0) {
                write(sockfd, "*** Error: User already exists\n", 27);
                return;
            }
        }
        
        // Create new user
        if (user_count < 10) {
            strncpy(users[user_count].username, username, 49);
            users[user_count].username[49] = '\0';
            strncpy(users[user_count].password, password, 49);
            users[user_count].password[49] = '\0';
            user_count++;
            
            // Automatically log in after successful signup
            session->authenticated = 1;
            strncpy(session->username, username, 49);
            session->username[49] = '\0';
            
            write(sockfd, "Signup successful. You are now logged in.\n", 42);
        } else {
            write(sockfd, "*** Error: User limit reached\n", 26);
        }
    }
    
    else if (strcmp(command, "login") == 0) {
        if (args != 3) {
            write(sockfd, "*** Invalid format. Usage: login <username> <password>\n", 55);
            return;
        }
        
        // Check if already logged in
        if (session->authenticated) {
            char msg[128];
            snprintf(msg, sizeof(msg), "*** Error: Already logged in as '%s'\n", session->username);
            write(sockfd, msg, strlen(msg));
            return;
        }
        
        // Verify credentials
        for (int i = 0; i < user_count; i++) {
            if (strcmp(users[i].username, username) == 0 && 
                strcmp(users[i].password, password) == 0) {
                session->authenticated = 1;
                strncpy(session->username, username, 49);
                session->username[49] = '\0';
                write(sockfd, "Login successful\n", 17);
                return;
            }
        }
        write(sockfd, "*** Error: Invalid credentials\n", 27);
    }
    
    else if (strcmp(command, "logout") == 0) {
        if (!session->authenticated) {
            write(sockfd, "*** Error: Not logged in\n", 21);
            return;
        }
        
        // Clear session
        session->authenticated = 0;
        memset(session->username, 0, sizeof(session->username));
        write(sockfd, "Logged out successfully\n", 24);
    }
    
    // ==================== FILE COMMANDS (require authentication) ====================
    
    else if (strcmp(command, "UPLOAD") == 0) {
        if (!session->authenticated) {
            write(sockfd, "*** Error: Please login first\n", 26);
            return;
        }
        
        char filename[256];
        if (sscanf(buffer, "%*s %255s", filename) == 1) {
            char msg[300];
            snprintf(msg, sizeof(msg), "Upload command received for file: %s\n", filename);
            write(sockfd, msg, strlen(msg));
        } else {
            write(sockfd, "*** Invalid format. Usage: UPLOAD <filename>\n", 45);
        }
    }
    
    else if (strcmp(command, "DOWNLOAD") == 0) {
        if (!session->authenticated) {
            write(sockfd, "*** Error: Please login first\n", 26);
            return;
        }
        
        char filename[256];
        if (sscanf(buffer, "%*s %255s", filename) == 1) {
            char msg[300];
            snprintf(msg, sizeof(msg), "Download command received for file: %s\n", filename);
            write(sockfd, msg, strlen(msg));
        } else {
            write(sockfd, "*** Invalid format. Usage: DOWNLOAD <filename>\n", 47);
        }
    }
    
    // ==================== UNKNOWN COMMAND ====================
    
    else {
        write(sockfd, "*** Unknown command\n", 20);
    }
}