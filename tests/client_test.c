#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>

#define NUM_CLIENTS 5
#define SERVER_PORT 12345

void *client_thread(void *arg) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return NULL; }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        return NULL;
    }

    // Simple test: send login or command
    char *msg = "login user1 password\n";
    send(sock, msg, strlen(msg), 0);

    char buf[256];
    int n = recv(sock, buf, sizeof(buf)-1, 0);
    if (n > 0) { buf[n] = 0; printf("Server: %s", buf); }

    // Keep connection alive for testing multiple sessions
    sleep(2);

    close(sock);
    return NULL;
}

int main() {
    pthread_t threads[NUM_CLIENTS];

    for (int i = 0; i < NUM_CLIENTS; i++) {
        pthread_create(&threads[i], NULL, client_thread, NULL);
    }

    for (int i = 0; i < NUM_CLIENTS; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}
