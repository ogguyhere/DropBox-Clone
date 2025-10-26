// src/file_io.c

#include "file_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

// User dir creation
int create_user_dir(const char* username) {
    if (!username) return -1;
    
    char user_dir[256];
    snprintf(user_dir, sizeof(user_dir), USER_DIR_FORMAT, username);
    
    struct stat st = {0};
    if (stat(STORAGE_DIR, &st) == -1) {
        if (mkdir(STORAGE_DIR, 0700) == -1) {
            perror("mkdir storage");
            return -1;
        }
    }
    
    if (stat(user_dir, &st) == -1) {
        if (mkdir(user_dir, 0700) == -1) {
            perror("mkdir user dir");
            return -1;
        }
    }
    return 0;
}

// Save file to disk
int save_file(const char* username, const char* filename, const unsigned char* data, size_t size) {
    if (!username || !filename || !data) return -1;
    
    char full_path[512];
    snprintf(full_path, sizeof(full_path), FULL_PATH_FORMAT, username, filename);
    
    FILE* f = fopen(full_path, "wb");
    if (!f) {
        perror("fopen save");
        return -1;
    }
    
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    
    if (written != size) {
        fprintf(stderr, "Write incomplete: %zu/%zu bytes\n", written, size);
        return -1;
    }
    
    printf("  Disk: Saved %s (%zu bytes)\n", full_path, size);
    return 0;
}

// Load file from disk
int load_file(const char* username, const char* filename, unsigned char* data, size_t* size, size_t max_size) {
    if (!username || !filename || !data || !size) return -1;
    
    char full_path[512];
    snprintf(full_path, sizeof(full_path), FULL_PATH_FORMAT, username, filename);
    
    FILE* f = fopen(full_path, "rb");
    if (!f) {
        perror("fopen load");
        return -1;
    }
    
    size_t read_size = fread(data, 1, max_size, f);
    fclose(f);
    
    *size = read_size;
    printf("  Disk: Loaded %s (%zu bytes)\n", full_path, read_size);
    return 0;
}

// Delete file
int delete_file(const char* username, const char* filename) {
    if (!username || !filename) return -1;
    
    char full_path[512];
    snprintf(full_path, sizeof(full_path), FULL_PATH_FORMAT, username, filename);
    
    if (unlink(full_path) == -1) {
        if (errno == ENOENT) {
            fprintf(stderr, "File not found: %s\n", full_path);
        } else {
            perror("unlink");
        }
        return -1;
    }
    
    printf("  Disk: Removed %s\n", full_path);
    return 0;
}

// List user directory
int list_user_dir(const char* username, char* output, size_t out_size) {
    if (!username || !output) return -1;
    
    char user_dir[256];
    snprintf(user_dir, sizeof(user_dir), USER_DIR_FORMAT, username);
    
    DIR* d = opendir(user_dir);
    if (!d) {
        if (errno == ENOENT) {
            create_user_dir(username);  // Auto-create empty
        }
        d = opendir(user_dir);
        if (!d) {
            perror("opendir");
            strncpy(output, "ERROR: Could not open dir\n", out_size - 1);
            output[out_size - 1] = '\0';
            return -1;
        }
    }
    
    output[0] = '\0';
    struct dirent* entry;
    char buf[512];
    
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_type == DT_REG) {  // Files only
            char full[512];
            snprintf(full, sizeof(full), FULL_PATH_FORMAT, username, entry->d_name);
            struct stat st;
            if (stat(full, &st) == 0) {
                snprintf(buf, sizeof(buf), "%s %zu\n", entry->d_name, (size_t)st.st_size);
                strncat(output, buf, out_size - strlen(output) - 1);
            }
        }
    }
    closedir(d);
    
    if (strlen(output) == 0) {
        strncat(output, "No files\n", out_size - strlen(output) - 1);
    }
    return 0;
}

// Get file size
size_t get_file_size(const char* username, const char* filename) {
    if (!username || !filename) return 0;
    
    char full_path[512];
    snprintf(full_path, sizeof(full_path), FULL_PATH_FORMAT, username, filename);
    
    struct stat st;
    if (stat(full_path, &st) == 0 && S_ISREG(st.st_mode)) {
        return (size_t)st.st_size;
    }
    return 0;
}