// src/client.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\nSocket creation error\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/Address not supported\n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed\n");
        return -1;
    }
    
    printf("Connected to server on port 8080\n");
    printf("==========================================\n\n");

    // *** Comprehensive test with logout ***
    char *commands[] = {
        "signup user1 pass1\n",         // Should succeed and auto-login
        "LIST\n",                        // Should work - logged in (empty list)
        "login user2 pass2\n",          // Should fail - already logged in as user1
        "logout\n",                     // Should succeed
        "LIST\n",                        // Should fail - logged out
        "login user1 pass1\n",          // Should succeed - log back in
        "signup user1 pass1\n",         // Should fail - user exists
        "logout\n",                     // Logout again
        "signup user2 pass2\n",         // Should succeed - create new user
        "LIST\n",                        // Should work - logged in as user2
        "INVALID cmd\n"                 // Should fail - unknown command
    };
    
    for (int i = 0; i < 11; i++) {
        printf("[Client] Sending: %s", commands[i]);
        write(sock, commands[i], strlen(commands[i]));
        
        // Read server response
        memset(buffer, 0, sizeof(buffer));
        valread = read(sock, buffer, sizeof(buffer) - 1);
        if (valread > 0) {
            buffer[valread] = '\0';
            printf("[Server] %s", buffer);
            if (buffer[valread - 1] != '\n') {
                printf("\n");
            }
        } else {
            printf("[Client] Error: No response from server\n");
        }
        
        printf("\n");
        sleep(1); // Delay between commands
    }
    
    printf("==========================================\n");
    printf("[Client] Closing connection...\n");
    sleep(1);
    close(sock);
    return 0;
}