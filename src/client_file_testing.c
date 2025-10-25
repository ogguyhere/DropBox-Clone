#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

// ====== Config ======
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 8192
#define BASE64_LINE 76

// ====== Helper Prototypes ======
int connect_to_server();
void send_command(int sock, const char *cmd);
void read_response(int sock);
char *encode_base64_file(const char *filename);
void upload_file(int sock, const char *filename);

// ====== Base64 Encoding ======
static const char b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char *encode_base64_file(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("fopen");
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    rewind(fp);

    unsigned char *buffer = malloc(fsize);
    fread(buffer, 1, fsize, fp);
    fclose(fp);

    size_t out_len = 4 * ((fsize + 2) / 3);
    char *encoded = malloc(out_len + 1);
    char *p = encoded;

    for (long i = 0; i < fsize; i += 3) {
        int val = (buffer[i] << 16) + ((i + 1 < fsize ? buffer[i + 1] : 0) << 8)
                  + (i + 2 < fsize ? buffer[i + 2] : 0);
        *p++ = b64_table[(val >> 18) & 0x3F];
        *p++ = b64_table[(val >> 12) & 0x3F];
        *p++ = (i + 1 < fsize) ? b64_table[(val >> 6) & 0x3F] : '=';
        *p++ = (i + 2 < fsize) ? b64_table[val & 0x3F] : '=';
    }
    *p = '\0';

    free(buffer);
    return encoded;
}

// ====== Networking Helpers ======
int connect_to_server() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    printf("Connected to server at %s:%d\n", SERVER_IP, SERVER_PORT);
    return sock;
}

void send_command(int sock, const char *cmd) {
    send(sock, cmd, strlen(cmd), 0);
}

void read_response(int sock) {
    char buffer[BUFFER_SIZE] = {0};
    int n = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (n > 0) {
        buffer[n] = '\0';
        printf("Server: %s\n", buffer);
    } else {
        printf("No response or connection closed.\n");
    }
}

// ====== Upload Logic ======
void upload_file(int sock, const char *filename) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "UPLOAD %s", filename);
    send_command(sock, cmd);
    read_response(sock); // expect READY_TO_RECEIVE

    char *encoded = encode_base64_file(filename);
    if (!encoded) {
        fprintf(stderr, "Error: Failed to encode file.\n");
        return;
    }

    send(sock, encoded, strlen(encoded), 0);
    free(encoded);

    read_response(sock); // expect UPLOAD_SUCCESS
}

// ====== Main ======
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file_to_upload>\n", argv[0]);
        return 1;
    }

    int sock = connect_to_server();

    // Optional: send login command here if server requires auth

    send_command(sock, "signup test1 pass");
    read_response(sock);

    // send_command(sock, "login test2 pass");
    // read_response(sock);


    upload_file(sock, argv[1]);
    close(sock);

    return 0;
}
