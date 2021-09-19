#ifndef __FILE_SYSTEM__
#define __FILE_SYSTEM__

#include "icl_hash.h"
#include "file_stored.h"

typedef struct file_system file_system_t;

file_system_t* create_fs(size_t max_capacity_mem, size_t max_file_count);
void set_policy_fs(file_system_t* fs, char* policy);

void acquire_read_lock_fs(file_system_t* fs);
void acquire_write_lock_fs(file_system_t* fs);
void release_read_lock_fs(file_system_t* fs);
void release_write_lock_fs(file_system_t* fs);

size_t get_file_count_fs(file_system_t* fs);
bool_t is_file_count_full_fs(file_system_t* fs);

file_stored_t* find_file_fs(file_system_t* fs, const char* pathname);
int add_file_fs(file_system_t* fs, const char* pathname, file_stored_t* file);
int remove_file_fs(file_system_t* fs, const char* pathname);

void free_fs(file_system_t* fs);

#endif