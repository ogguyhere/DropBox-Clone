// src/client_file_testing.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>

// ====== Config ======
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 8192

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

int base64_decode(const char *in, unsigned char *out, int out_size) {
    int decode_table[256];
    memset(decode_table, -1, sizeof(decode_table));
    for (int i = 0; i < 64; i++)
        decode_table[(unsigned char)b64_table[i]] = i;

    int val = 0, valb = -8, out_len = 0;
    for (int i = 0; in[i]; i++) {
        unsigned char c = in[i];
        if (decode_table[c] == -1) continue;
        val = (val << 6) + decode_table[c];
        valb += 6;
        if (valb >= 0) {
            if (out_len >= out_size) break;
            out[out_len++] = (val >> valb) & 0xFF;
            valb -= 8;
        }
    }
    return out_len;
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
    read_response(sock); // Expect READY_TO_RECEIVE

    char *encoded = encode_base64_file(filename);
    if (!encoded) {
        fprintf(stderr, "Error: Failed to encode file.\n");
        return;
    }

    send(sock, encoded, strlen(encoded), 0);
    free(encoded);

    read_response(sock); // Expect UPLOAD_SUCCESS
}

void download_file(int sock, const char *filename) {
    // Make downloads folder if missing
    mkdir("downloads", 0755);

    // Send DOWNLOAD command
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "DOWNLOAD %s", filename);
    send_command(sock, cmd);

    // Receive Base64 stream from server
    char enc_buf[65536];
    int total = 0;
    int n;

    while ((n = recv(sock, enc_buf + total, sizeof(enc_buf) - 1 - total, 0)) > 0) {
        total += n;
        if (n < sizeof(enc_buf)/2) break; // Simple heuristic for EOF
    }
    enc_buf[total] = '\0';

    // Decode entire Base64 at once
    unsigned char dec_buf[49152];
    int dec_len = base64_decode(enc_buf, dec_buf, sizeof(dec_buf));
    if (dec_len <= 0) {
        fprintf(stderr, "Failed to decode Base64\n");
        return;
    }

    // Write to downloads folder
    char out_path[512];
    snprintf(out_path, sizeof(out_path), "downloads/%s", filename);
    FILE *out = fopen(out_path, "wb");
    if (!out) { 
        perror("fopen"); 
        return; 
    }
    fwrite(dec_buf, 1, dec_len, out);
    fclose(out);

    printf("Downloaded: %s (%d bytes)\n", out_path, dec_len);
}



void alice_test(int sock) {
    printf("\n=== Signup alice ===\n");
    send_command(sock, "signup alice testpass");
    read_response(sock);
    sleep(1);

    // --- Test multiple uploads ---
    printf("\n=== Testing UPLOAD ===\n");
    upload_file(sock, "test1.txt"); // Base64: "This is test 1 content"
    upload_file(sock, "test2.txt"); // Base64: "This is test 2 content"
    upload_file(sock, "test3.txt"); // Base64: "This is test 3 content"

    sleep(1);

    // --- List files ---
    printf("\n=== Testing LIST ===\n");
    send_command(sock, "LIST");
    read_response(sock);
    sleep(1);

    // --- Download files ---
    printf("\n=== Testing DOWNLOAD ===\n");
    download_file(sock, "test1.txt");
    download_file(sock, "test2.txt");
    download_file(sock, "test3.txt");
    sleep(1);

    // --- Delete files ---
    printf("\n=== Testing DELETE ===\n");
    send_command(sock, "DELETE test1.txt");
    read_response(sock); 
    send_command(sock, "DELETE test3.txt");
    read_response(sock);
    sleep(1);

    // --- List again ---
    printf("\n=== Testing LIST (after delete) ===\n");
    send_command(sock, "LIST");
    read_response(sock);

    close(sock);
    printf("\n=== Test Complete ===\n");
}

// ==================================== Main ==========================================
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file_to_upload>\n", argv[0]);
        return 1;
    }

    int sock = connect_to_server();
    alice_test(sock);
    return 0;
}


// testuser file op tests:
    // // Signup
    // send_command(sock, "signup testuser testpass");
    // read_response(sock);
    // sleep(1);

    // // // Upload file
    // // printf("\n=== Testing UPLOAD ===\n");
    // // upload_file(sock, argv[1]);
    // // read_response(sock);
    // // sleep(1);

    // // List files
    // printf("\n=== Testing LIST ===\n");
    // send_command(sock, "LIST");
    // read_response(sock);
    // sleep(1);

    // // Download file
    // printf("\n=== Testing DOWNLOAD ===\n");
    // download_file(sock, argv[1]);
    // sleep(1);

    // // Delete file
    // printf("\n=== Testing DELETE ===\n");
    // char delete_cmd[256];
    // snprintf(delete_cmd, sizeof(delete_cmd), "DELETE %s", argv[1]);
    // send_command(sock, delete_cmd);
    // read_response(sock);
    // sleep(1);

    // // List again (should be empty)
    // printf("\n=== Testing LIST (after delete) ===\n");
    // send_command(sock, "LIST");
    // read_response(sock);

    // close(sock);
    // printf("\n=== Test Complete ===\n");

    // return 0;