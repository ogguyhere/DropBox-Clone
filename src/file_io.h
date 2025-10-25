// src.file_io.h

// -------------------------------------------------------------------------
// File I/O Utilities for User Storage
// Helpers for disk ops: Create user dir, delete file, list dir contents.
// Paths: ./storage/{username}/filename (creates dir if missing).
// -------------------------------------------------------------------------

#ifndef FILE_IO_H
#define FILE_IO_H

#include <stddef.h> //size_t for list 

//USER base dir 
#define STORAGE_DIR "./storage"
#define USER_DIR_FORMAT STORAGE_DIR "/%s" // e.g ./storage/kay
#define FULL_PATH_FORMAT USER_DIR_FORMAT "/%s" // .storage/kay/lol.txt

// API 
int create_user_dir(const char* username);
int delete_file(const char* username, const char* filename);
int list_user_dir(const char* username, char* output, size_t out_size);  // formats "lol.txt 1024\n"
size_t get_file_size(const char* username, const char* filename);  // For quota sync



#endif
