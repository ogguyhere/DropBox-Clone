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
 

// ================= BASE64 ENCODE =================
static const char base64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int base64_encode(const unsigned char *in, int len, char *out, int out_size) {
    int out_len = 0;
    for (int i = 0; i < len; i += 3) {
        int val = (in[i] << 16) + ((i + 1 < len ? in[i + 1] : 0) << 8) + (i + 2 < len ? in[i + 2] : 0);
        if (out_len + 4 >= out_size) break; // safety
        out[out_len++] = base64_table[(val >> 18) & 63];
        out[out_len++] = base64_table[(val >> 12) & 63];
        out[out_len++] = (i + 1 < len) ? base64_table[(val >> 6) & 63] : '=';
        out[out_len++] = (i + 2 < len) ? base64_table[val & 63] : '=';
    }
    out[out_len] = '\0';
    return out_len;
}

// ================= FILE → BASE64 STRING =================
int encode_file_to_base64(const char *filename, char *out, int out_size) {
    FILE *f = fopen(filename, "rb");
    if (!f) return -1;

    unsigned char data[4096];
    size_t len = fread(data, 1, sizeof(data), f);
    fclose(f);

    if (len == 0) return -2;
    return base64_encode(data, len, out, out_size);
}
int base64_decode(const char *in, unsigned char *out, int out_size) {
    int len = strlen(in);
    int out_len = 0;
    int val = 0, valb = -8;

    int decode_table[256];
    memset(decode_table, -1, sizeof(decode_table));
    for (int i = 0; i < 64; i++)
        decode_table[(unsigned char)"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;

    for (int i = 0; i < len; i++) {
        unsigned char c = in[i];
        if (decode_table[c] == -1) continue; // skip padding or invalid chars
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

// ================= SEND UPLOAD =================
void send_upload_command(int sock, const char *command, const char *filename) {
    char buffer[8192];
    write(sock, command, strlen(command));

    int valread = read(sock, buffer, sizeof(buffer) - 1);
    buffer[valread] = '\0';
    printf("[Server] %s", buffer);

    if (!strstr(buffer, "READY_TO_RECEIVE")) {
        printf("*** Server not ready for upload.\n");
        return;
    }

    char encoded_data[8192];
    int encoded_len = encode_file_to_base64(filename, encoded_data, sizeof(encoded_data));
    if (encoded_len < 0) {
        printf("*** Error: failed to read or encode %s\n", filename);
        return;
    }

    write(sock, encoded_data, encoded_len);

    memset(buffer, 0, sizeof(buffer));
    valread = read(sock, buffer, sizeof(buffer) - 1);
    buffer[valread] = '\0';
    printf("[Server] %s\n", buffer);
}

static void send_response(int sockfd, const char *msg) {
    write(sockfd, msg, strlen(msg));
}

static void handle_signup(int sockfd, char *username, char *password, ClientSession *session) {
    extern User users[10];
    extern int user_count;

    if (!username || !password) {
        send_response(sockfd, "*** Invalid format. Usage: signup <username> <password>\n");
        return;
    }

    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            send_response(sockfd, "*** Error: User already exists\n");
            return;
        }
    }

    if (user_count < 10) {
        strncpy(users[user_count].username, username, 49);
        strncpy(users[user_count].password, password, 49);
        user_count++;
        session->authenticated = 1;
        strncpy(session->username, username, 49);
        send_response(sockfd, "Signup successful. You are now logged in.\n");
    } else {
        send_response(sockfd, "*** Error: User limit reached\n");
    }
}

static void handle_login(int sockfd, char *username, char *password, ClientSession *session) {
    extern User users[10];
    extern int user_count;

    if (session->authenticated) {
        char msg[128];
        snprintf(msg, sizeof(msg), "*** Error: Already logged in as '%s'\n", session->username);
        send_response(sockfd, msg);
        return;
    }

    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0 &&
            strcmp(users[i].password, password) == 0) {
            session->authenticated = 1;
            strncpy(session->username, username, 49);
            send_response(sockfd, "Login successful\n");
            return;
        }
    }

    send_response(sockfd, "*** Error: Invalid credentials\n");
}

static void handle_logout(int sockfd, ClientSession *session) {
    if (!session->authenticated) {
        send_response(sockfd, "*** Error: Not logged in\n");
        return;
    }
    session->authenticated = 0;
    memset(session->username, 0, sizeof(session->username));
    send_response(sockfd, "Logged out successfully\n");
}

static void handle_upload(int sockfd, char *filename, ClientSession *session) {
    if (!session->authenticated) {
        send_response(sockfd, "*** Error: Please login first\n");
        return;
    }

    if (!filename) {
        send_response(sockfd, "*** Invalid format. Usage: UPLOAD <filename>\n");
        return;
    }

    send_response(sockfd, "READY_TO_RECEIVE\n");

    char encoded_data[8192];
    int bytes_read = read(sockfd, encoded_data, sizeof(encoded_data) - 1);
    if (bytes_read <= 0) {
        send_response(sockfd, "*** Error: Failed to receive file data\n");
        return;
    }
    encoded_data[bytes_read] = '\0';

    // Decode before saving (bonus implementation)
    unsigned char decoded_data[8192];
    int decoded_len = base64_decode(encoded_data, decoded_data, sizeof(decoded_data));
    if (decoded_len <= 0) {
        send_response(sockfd, "*** Error: Failed to decode Base64 data\n");
        return;
    }

    // Save decoded file to disk
    char save_path[300];
    snprintf(save_path, sizeof(save_path), "uploads/%s", filename);
    FILE *f = fopen(save_path, "wb");
    if (!f) {
        send_response(sockfd, "*** Error: Could not save file\n");
        return;
    }

    fwrite(decoded_data, 1, decoded_len, f);
    fclose(f);

    send_response(sockfd, "UPLOAD_SUCCESS\n");
}


static void handle_download(int sockfd, char *filename, ClientSession *session) {
    if (!session->authenticated) {
        send_response(sockfd, "*** Error: Please login first\n");
        return;
    }

    if (!filename) {
        send_response(sockfd, "*** Invalid format. Usage: DOWNLOAD <filename>\n");
        return;
    }

    // Construct full path
    char filepath[300];
    snprintf(filepath, sizeof(filepath), "uploads/%s", filename);

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        send_response(sockfd, "*** Error: File not found on server\n");
        return;
    }

    // Read file in chunks
    unsigned char file_buf[4096];
    char encoded_buf[8192]; // Base64 output buffer
    size_t bytes_read;

    while ((bytes_read = fread(file_buf, 1, sizeof(file_buf), f)) > 0) {
        int encoded_len = base64_encode(file_buf, bytes_read, encoded_buf, sizeof(encoded_buf));
        if (encoded_len <= 0) {
            send_response(sockfd, "*** Error: Failed to encode file\n");
            fclose(f);
            return;
        }
        write(sockfd, encoded_buf, encoded_len);
    }

    fclose(f);
}


void handle_commands(int sockfd, const char *buffer, ClientSession *session) {
    char command[10], arg1[50], arg2[50];
    int args = sscanf(buffer, "%9s %49s %49s", command, arg1, arg2);

    if (args < 1) {
        send_response(sockfd, "*** Invalid command\n");
        return;
    }

    if (strcmp(command, "signup") == 0) {
        handle_signup(sockfd, args == 3 ? arg1 : NULL, args == 3 ? arg2 : NULL, session);
    } else if (strcmp(command, "login") == 0) {
        handle_login(sockfd, args == 3 ? arg1 : NULL, args == 3 ? arg2 : NULL, session);
    } else if (strcmp(command, "logout") == 0) {
        handle_logout(sockfd, session);
    } else if (strcmp(command, "UPLOAD") == 0) {
        handle_upload(sockfd, args >= 2 ? arg1 : NULL, session);
    } else if (strcmp(command, "DOWNLOAD") == 0) {
        handle_download(sockfd, args >= 2 ? arg1 : NULL, session);
    } else {
        send_response(sockfd, "*** Unknown command\n");
    }
}
