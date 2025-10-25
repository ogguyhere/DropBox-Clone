// src/file_io.c

#include "file_io.h"
#include <stdio.h>     // printf/snprintf
#include <stdlib.h>    // malloc? No, buffers
#include <string.h>    // strncpy
#include <sys/stat.h>  // stat, mkdir
#include <sys/types.h> // mode_t
#include <unistd.h>    // rmdir? No
#include <dirent.h>    // opendir/readdir
#include <errno.h>     // errno

// user dir creation s
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

// delete files
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
    printf("  Disk: Removed %s\n", full_path);  // Debug
    return 0;
}

// list user 
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

//get file 
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