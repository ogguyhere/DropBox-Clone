// src/metadata.c

#include "metadata.h"
#include <stdio.h>  // snprintf
#include <string.h> // strcmp/strncpy
#include <stdlib.h> // malloc

// Metadata initialization
metadata_t *metadata_init(void)
{
    metadata_t *m = malloc(sizeof(metadata_t));
    if (!m)
        return NULL;

    m->num_users = 0;
    pthread_mutex_init(&m->meta_lock, NULL);

    // Initialize per-user locks
    for (int i = 0; i < MAX_USERS; i++)
    {
        pthread_mutex_init(&m->users[i].user_lock, NULL);
    }

    return m;
}

// Metadata destroy
void metadata_destroy(metadata_t *m)
{
    if (!m)
        return;

    pthread_mutex_destroy(&m->meta_lock);
    for (int i = 0; i < MAX_USERS; i++)
    {
        pthread_mutex_destroy(&m->users[i].user_lock);
    }
    free(m);
}

// Add user with password
int metadata_add_user(metadata_t *m, const char *username, const char *password)
{
    if (!m || !username || !password)
        return -1;
    if (m->num_users >= MAX_USERS || strlen(username) >= 64 || strlen(password) >= 64)
        return -1;

    pthread_mutex_lock(&m->meta_lock);

    // Check if user already exists
    for (int i = 0; i < m->num_users; i++)
    {
        if (strcmp(m->users[i].username, username) == 0)
        {
            pthread_mutex_unlock(&m->meta_lock);
            return -2; // User exists
        }
    }

    user_t *u = &m->users[m->num_users];
    strncpy(u->username, username, 63);
    u->username[63] = '\0';
    strncpy(u->password, password, 63);
    u->password[63] = '\0';
    u->num_files = 0;
    u->quota_used = 0;
    u->quota_max = DEFAULT_QUOTA;
    m->num_users++;

    pthread_mutex_unlock(&m->meta_lock);
    return 0;
}

// Get user (returns pointer to user in metadata)
int metadata_get_user(metadata_t *m, const char *username, user_t **user)
{
    if (!m || !username || !user)
        return -1;

    pthread_mutex_lock(&m->meta_lock);
    for (int i = 0; i < m->num_users; i++)
    {
        if (strcmp(m->users[i].username, username) == 0)
        {
            *user = &m->users[i];
            pthread_mutex_unlock(&m->meta_lock);
            return 0;
        }
    }
    pthread_mutex_unlock(&m->meta_lock);
    return -1;
}

// Authenticate user
int metadata_authenticate(metadata_t *m, const char *username, const char *password)
{
    if (!m || !username || !password)
        return 0;

    pthread_mutex_lock(&m->meta_lock);
    for (int i = 0; i < m->num_users; i++)
    {
        if (strcmp(m->users[i].username, username) == 0 &&
            strcmp(m->users[i].password, password) == 0)
        {
            pthread_mutex_unlock(&m->meta_lock);
            return 1; // Success
        }
    }
    pthread_mutex_unlock(&m->meta_lock);
    return 0; // Failed
}

// Check quota
int metadata_check_quota(metadata_t *m, const char *username, size_t add_size)
{
    user_t *u;
    if (metadata_get_user(m, username, &u) != 0)
        return 0;

    pthread_mutex_lock(&u->user_lock);
    int ok = (u->quota_used + add_size <= u->quota_max) ? 1 : 0;
    pthread_mutex_unlock(&u->user_lock);
    return ok;
}

// Add file
int metadata_add_file(metadata_t *m, const char *username, const char *filename, size_t size)
{
    user_t *u;
    if (metadata_get_user(m, username, &u) != 0)
        return -1;

    pthread_mutex_lock(&u->user_lock);

    // Check if file already exists
    for (int i = 0; i < u->num_files; i++)
    {
        if (strcmp(u->files[i].filename, filename) == 0)
        {
            // File exists - just update size
            u->quota_used = u->quota_used - u->files[i].size + size;
            u->files[i].size = size;
            pthread_mutex_unlock(&u->user_lock);
            return 0;
        }
    }

    if (u->num_files >= MAX_FILES_PER_USER)
    {
        pthread_mutex_unlock(&u->user_lock);
        return -1;
    }

    if (u->quota_used + size > u->quota_max)
    {
        pthread_mutex_unlock(&u->user_lock); // atomic metadata update
        return -2;                           // Quota exceeded
    }

    file_t *f = &u->files[u->num_files];
    strncpy(f->filename, filename, 255);
    f->filename[255] = '\0';
    f->size = size;

    //initializing the file lock 
    pthread_mutex_init(&f->file_lock, NULL);
    f->locked = 1;

    u->num_files++;
    u->quota_used += size;

    pthread_mutex_unlock(&u->user_lock);
    return 0;
}

// Remove file
int metadata_remove_file(metadata_t *m, const char *username, const char *filename)
{
    user_t *u;
    if (metadata_get_user(m, username, &u) != 0)
        return -1;

    pthread_mutex_lock(&u->user_lock);

    for (int i = 0; i < u->num_files; i++)
    {
        if (strcmp(u->files[i].filename, filename) == 0)
        {
            u->quota_used -= u->files[i].size;

            //Destroy file lock before removal
            if (u->files[i].locked) {
                pthread_mutex_destroy(&u->files[i].file_lock);
                u->files[i].locked = 0;
            }

            // Shift to remove (simple array delete)
            for (int j = i; j < u->num_files - 1; j++)
            {
                u->files[j] = u->files[j + 1];
            }
            u->num_files--;
            pthread_mutex_unlock(&u->user_lock);
            return 0;
        }
    }

    pthread_mutex_unlock(&u->user_lock);
    return -1; // Not found
}

// List files
void metadata_list_files(metadata_t *m, const char *username, char *output, size_t out_size)
{
    user_t *u;
    if (metadata_get_user(m, username, &u) != 0)
    {
        strncpy(output, "ERROR: User not found\n", out_size - 1);
        output[out_size - 1] = '\0';
        return;
    }

    pthread_mutex_lock(&u->user_lock);

    output[0] = '\0';
    char buf[512];
    if (u->num_files == 0)
    {
        strncat(output, "No files\n", out_size - strlen(output) - 1);
    }
    else
    {
        for (int i = 0; i < u->num_files; i++)
        {
            snprintf(buf, sizeof(buf), "%s %zu\n", u->files[i].filename, u->files[i].size);
            strncat(output, buf, out_size - strlen(output) - 1);
        }
    }

    pthread_mutex_unlock(&u->user_lock);
}

// Returns pointer to file if found, NULL otherwise
// Locks the file's mutex if found
file_t* metadata_get_and_lock_file(metadata_t* m, const char* username, const char* filename) {
    user_t* u;
    if (metadata_get_user(m, username, &u) != 0) return NULL;
    
    pthread_mutex_lock(&u->user_lock);
    
    for (int i = 0; i < u->num_files; i++) {
        if (strcmp(u->files[i].filename, filename) == 0) {
            file_t* f = &u->files[i];
            pthread_mutex_lock(&f->file_lock);  // Lock the file
            pthread_mutex_unlock(&u->user_lock);
            return f;
        }
    }
    
    pthread_mutex_unlock(&u->user_lock);
    return NULL;  // File not found
}

// ADD THIS to unlock a file
void metadata_unlock_file(file_t* f) {
    if (f) {
        pthread_mutex_unlock(&f->file_lock);
    }
}