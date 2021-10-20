#ifndef __FILE_SYSTEM__
#define __FILE_SYSTEM__

#include "icl_hash.h"
#include "file_stored.h"

typedef struct file_system file_system_t;

// FS policy to be used in the replacement algorithm
extern int(*fs_policy)(const void*, const void*);

// Creates and setup a FS
file_system_t* create_fs(size_t max_capacity_mem, size_t max_file_count);

// Set the replacement policy of a FS
void set_policy_fs(file_system_t* fs, char* policy);

// Set the workers which will access a FS, used for metrics purpose
void set_workers_fs(file_system_t* fs, pthread_t* pids, int n);

// Acquire the read lock of a FS
void acquire_read_lock_fs(file_system_t* fs);

// Acquire the write lock of a FS
void acquire_write_lock_fs(file_system_t* fs);

// Release the read lock of a FS
void release_read_lock_fs(file_system_t* fs);

// Release the write lock of a FS
void release_write_lock_fs(file_system_t* fs);

// Get an array rappresentation of the current FS
file_stored_t** get_files_stored(file_system_t* fs);

// Get the current FS count
size_t get_file_count_fs(file_system_t* fs);

// Check whether the current FS file count is full
bool_t is_file_count_full_fs(file_system_t* fs);

// Check whether there is at least size bytes available in the current FS
// The result > 0 rappresent the bytes needed to be able to store those size bytes
int is_size_available(file_system_t* fs, size_t size);

// Check whether this size will overflow the current FS
bool_t is_size_too_big(file_system_t* fs, size_t size);

// Get the file called pathname from the current FS
file_stored_t* find_file_fs(file_system_t* fs, const char* pathname);

// Add this file with this pathname to the current FS
int add_file_fs(file_system_t* fs, const char* pathname, file_stored_t* file);

// Remove the file with pathname from the current FS
// The third parameter if set to TRUE will soft remove the file, meaning that it will be deleted from the FS but the memory will not be freed totally
// (The soft remove is currently used from the replacement algorithm so that the files can be logged and eventually sent back to the client)
int remove_file_fs(file_system_t* fs, const char* pathname, bool_t keep_data);

// Update the current memory used by amount (Can be positive or negative)
int notify_memory_changed_fs(file_system_t* fs, int amount);

// Increase the number of requests handled by a worker
int notify_worker_handled_req_fs(file_system_t* fs, pthread_t pid);

// Notify the current FS a client disconnected from the server, removes the client from the opened files and release the lock owned by it
// (choose another client to own the lock)
int notify_client_disconnected_fs(file_system_t* fs, int fd);

// Executed before freeing the current FS, currently handles the logging/printing of FS Metrics of any kind
void shutdown_fs(file_system_t* fs);

// Free the current FS
void free_fs(file_system_t* fs);

#endif