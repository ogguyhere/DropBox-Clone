// src/metadata.h

// ---------------------------------------------------------------------------
// Simple in-memory store for user data: username → files array + quota tracking.
// Fixed arrays (no dynamic alloc for simplicity). Enforces per-user quotas (1MB default).
// ---------------------------------------------------------------------------

#ifndef METADATA_H
#define METADATA_H

#include <stddef.h>  // size_t
#include <pthread.h>

#define MAX_USERS 100
#define MAX_FILES_PER_USER 50
#define DEFAULT_QUOTA (1024 * 1024)  // 1MB

typedef struct {
    char filename[256];
    size_t size; 
    pthread_mutex_t file_lock; // so that multiple users wont update or delete file at a time (avoiding race condition)
    int locked; // traking if locked is initialized 
} file_t;

typedef struct {
    char username[64];
    char password[64];         // For authentication
    file_t files[MAX_FILES_PER_USER];
    int num_files;
    size_t quota_used;
    size_t quota_max;
    pthread_mutex_t user_lock; // Per-user lock for Phase 2
} user_t; 

typedef struct {
    user_t users[MAX_USERS];
    int num_users;
    pthread_mutex_t meta_lock; // Global metadata lock
} metadata_t;

// API: Basic CRUD + quota + authentication
metadata_t* metadata_init(void);
void metadata_destroy(metadata_t* m);
int metadata_add_user(metadata_t* m, const char* username, const char* password);
int metadata_get_user(metadata_t* m, const char* username, user_t** user);
int metadata_authenticate(metadata_t* m, const char* username, const char* password);
int metadata_add_file(metadata_t* m, const char* username, const char* filename, size_t size);
int metadata_remove_file(metadata_t* m, const char* username, const char* filename);
void metadata_list_files(metadata_t* m, const char* username, char* output, size_t out_size);
int metadata_check_quota(metadata_t* m, const char* username, size_t add_size);  // 1=ok, 0=over
file_t* metadata_get_and_lock_file(metadata_t* m, const char* username, const char* filename);
void metadata_unlock_file(file_t* f);


#endifs