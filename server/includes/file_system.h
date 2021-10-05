#ifndef __FILE_SYSTEM__
#define __FILE_SYSTEM__

#include "icl_hash.h"
#include "file_stored.h"

typedef struct file_system file_system_t;

extern int(*fs_policy)(const void*, const void*);

file_system_t* create_fs(size_t max_capacity_mem, size_t max_file_count);
void set_policy_fs(file_system_t* fs, char* policy);
void set_workers_fs(file_system_t* fs, pthread_t* pids, int n);

void acquire_read_lock_fs(file_system_t* fs);
void acquire_write_lock_fs(file_system_t* fs);
void release_read_lock_fs(file_system_t* fs);
void release_write_lock_fs(file_system_t* fs);

file_stored_t** get_files_stored(file_system_t* fs);
size_t get_file_count_fs(file_system_t* fs);
bool_t is_file_count_full_fs(file_system_t* fs);

int is_size_available(file_system_t* fs, size_t size);
bool_t is_size_too_big(file_system_t* fs, size_t size);

file_stored_t* find_file_fs(file_system_t* fs, const char* pathname);
int add_file_fs(file_system_t* fs, const char* pathname, file_stored_t* file);
int remove_file_fs(file_system_t* fs, const char* pathname, bool_t keep_data);
int notify_memory_changed_fs(file_system_t* fs, int amount);
int notify_worker_handled_req_fs(file_system_t* fs, pthread_t pid);
int notify_client_disconnected_fs(file_system_t* fs, int fd);

void shutdown_fs(file_system_t* fs);

void free_fs(file_system_t* fs);

#endif