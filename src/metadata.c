// rc/metadata.c

#include "metadata.h"
#include <stdio.h> //snprintf
#include <string.h> //strcmp/strncpy
#include <stdlib.h> // malloc for metadat_t

// metadata initialization
metadata_t* metadata_init(void)
{
    metadata_t* m = malloc(sizeof(metadata_t));
    if(!m) return NULL;
    m->num_users=0;
    return m;
}

// metadata_destroy
void metadata_destroy(metadata_t* m) {
    free(m);  // Fixed arrays—no more cleanup
}

// add user 
int metadata_add_user(metadata_t* m, const char* username) {
    if (m->num_users >= MAX_USERS || strlen(username) >= 64) return -1;
    user_t* u = &m->users[m->num_users];
    strncpy(u->username, username, 63);
    u->username[63] = '\0';
    u->num_files = 0;
    u->quota_used = 0;
    u->quota_max = DEFAULT_QUOTA;
    m->num_users++;
    return 0;
}

// get user
int metadata_get_user(metadata_t* m, const char* username, user_t** user) {
    for (int i = 0; i < m->num_users; i++) {
        if (strcmp(m->users[i].username, username) == 0) {
            *user = &m->users[i];
            return 0;
        }
    }
    return -1;
}

// check quota 
int metadata_check_quota(metadata_t* m, const char* username, size_t add_size) {
    user_t* u;
    if (metadata_get_user(m, username, &u) != 0) return 0;
    return (u->quota_used + add_size <= u->quota_max) ? 1 : 0;
}

// add file 
int metadata_add_file(metadata_t* m, const char* username, const char* filename, size_t size) {
    user_t* u;
    if (metadata_get_user(m, username, &u) != 0) return -1;
    if (u->num_files >= MAX_FILES_PER_USER) return -1;
    if (metadata_check_quota(m, username, size) != 1) return -1;

    file_t* f = &u->files[u->num_files];
    strncpy(f->filename, filename, 255);
    f->filename[255] = '\0';
    f->size = size;
    u->num_files++;
    u->quota_used += size;
    return 0;
}

// remove file 

int metadata_remove_file(metadata_t* m, const char* username, const char* filename) {
    user_t* u;
    if (metadata_get_user(m, username, &u) != 0) return -1;

    for (int i = 0; i < u->num_files; i++) {
        if (strcmp(u->files[i].filename, filename) == 0) {
            u->quota_used -= u->files[i].size;
            // Shift to remove (simple array delete)
            for (int j = i; j < u->num_files - 1; j++) {
                u->files[j] = u->files[j + 1];
            }
            u->num_files--;
            return 0;
        }
    }
    return -1;  // Not found
}

// list files 
void metadata_list_files(metadata_t* m, const char* username, char* output, size_t out_size) {
    user_t* u;
    if (metadata_get_user(m, username, &u) != 0) {
        strncpy(output, "ERROR: User not found\n", out_size - 1);
        output[out_size - 1] = '\0';
        return;
    }
    output[0] = '\0';
    char buf[512];
    if (u->num_files == 0) {
        strncat(output, "No files\n", out_size - strlen(output) - 1);
    }
    for (int i = 0; i < u->num_files; i++) {
        snprintf(buf, sizeof(buf), "%s %zu\n", u->files[i].filename, u->files[i].size);
        strncat(output, buf, out_size - strlen(output) - 1);
    }
}


