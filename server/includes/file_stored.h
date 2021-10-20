#ifndef __FILE_STORED__
#define __FILE_STORED__

#include <stdlib.h>
#include <stdint.h>
#include "queue.h"
#include "linked_list.h"
#include "utils.h"

typedef struct file_stored file_stored_t;

// Create and initialize a file with pathname
file_stored_t* create_file(const char* pathname);

// Get pathname of a file
char* file_get_pathname(file_stored_t* file);

// Get data buffer of file
char* file_get_data(file_stored_t* file);

// Get data size of buffer of file
size_t file_get_size(file_stored_t* file);

// Get current lock owner of file
int file_get_lock_owner(file_stored_t* file);

// Get creation time of file
struct timespec* file_get_creation_time(file_stored_t* file);

// Get last use time of file
struct timespec* file_get_last_use_time(file_stored_t* file);

// Get use frenquency of file
int file_get_use_frequency(file_stored_t* file);

// Let a client open this file
int file_add_client(file_stored_t* file, int client);

// Check whether the client had open this file
bool_t file_is_opened_by(file_stored_t* file, int client);

// Let a client close this file
int file_close_client(file_stored_t* file, int client);

// Enqueue the client to the lock queue of this file
int file_enqueue_lock(file_stored_t* file, int client);

// Check whether the client is already in the lock queue of this file
bool_t file_is_client_already_queued(file_stored_t* file, int client);

// Delete the client from the lock queue of this file
int file_delete_lock_client(file_stored_t* file, int client);

// Dequeue the client from the lock queue of this file
int file_dequeue_lock(file_stored_t* file);

// Set the current lock owner of this file
void file_set_lock_owner(file_stored_t* file, int lock_owner);

// Set the current last use time of this file
void file_set_last_use_time(file_stored_t* file, struct timespec new_use_time);

// Set the write mode of this file
void file_set_write_enabled(file_stored_t* file, bool_t is_enabled);

// Get the write mode of this file
bool_t file_is_write_enabled(file_stored_t* file);

// Replace the current data content with the new one of this file
int file_replace_content(file_stored_t* file, void* content, size_t content_size);

// Append the new content data to the old one of this file
int file_append_content(file_stored_t* file, void* content, size_t content_size);

// Acquire the read lock of this file
void acquire_read_lock_file(file_stored_t* file);

// Acquire the write lock of this file
void acquire_write_lock_file(file_stored_t* file);

// Release the read lock of this file
void release_read_lock_file(file_stored_t* file);

// Release the write lock of this file
void release_write_lock_file(file_stored_t* file);

// Notify when this file is getting used
void notify_used_file(file_stored_t* file);

// Increment the frequency by a step of this file
uint32_t file_inc_frequency(file_stored_t* file, int step);

// Get the lock queue of this file
queue_t* file_get_locks_queue(file_stored_t* file);

// Free this file
void free_file(file_stored_t* file);

// Free this file partially(Currently used by FS replacement)
void free_file_for_replacement(file_stored_t* file);

#endif