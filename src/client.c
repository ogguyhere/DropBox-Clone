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
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);

    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }
    printf("Connected to server on port 8080\n");

    char *commands[] = {
        "signup user1 pass1\n",
        "login user1 pass1\n",
        "signup user1 pass1\n",
        "login user1 wrongpass\n",
        "UPLOAD test.txt\n",
        "DOWNLOAD test.txt\n",
        "INVALID cmd\n"
    };
    for (int i = 0; i < 7; i++) {
        write(sock, commands[i], strlen(commands[i]));
        valread = read(sock, buffer, 1024);
        if (valread > 0) {
            buffer[valread] = '\0';
            printf("Server response: %s", buffer);
        }
        sleep(1); // Add delay to ensure server processes each command
    }
    sleep(2); // Wait before closing
    close(sock);
    return 0;
}