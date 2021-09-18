#include "file_system.h"

#include <string.h>
#include "replacement_policy.h"
#include "utils.h"

struct file_system {
    pthread_rwlock_t rwlock;
    icl_hash_t* files_stored;

    size_t current_used_memory;
    size_t total_memory;
    int (*fs_policy)(file_stored_t* f1, file_stored_t* f2);
};

file_system_t* create_fs(size_t max_capacity)
{
    file_system_t* fs;
    CHECK_FATAL_EQ(fs, malloc(sizeof(file_system_t)), NULL, NO_MEM_FATAL);
    memset(fs, 0, sizeof(file_system_t));

    fs->files_stored = icl_hash_create(10, NULL, NULL);
    fs->total_memory = max_capacity;

    INIT_RWLOCK(&fs->rwlock);

    return fs;
}

void set_policy_fs(file_system_t* fs, char* policy)
{
    // pick a policy, default is fifo
    if(strncmp(policy, "FIFO", 4) == 0)
        fs->fs_policy = replacement_policy_fifo;
    else if(strncmp(policy, "LRU", 3) == 0)
        fs->fs_policy = replacement_policy_lru;
    else if(strncmp(policy, "LFU", 3) == 0)
        fs->fs_policy = replacement_policy_lfu;
    else
        fs->fs_policy = replacement_policy_fifo;
}

static void free_files_stored_key(void* key)
{

}

static void free_files_stored_entry(void* entry)
{
    
}

void free_fs(file_system_t* fs)
{
    icl_hash_destroy(fs->files_stored, free_files_stored_key, free_files_stored_entry);
    pthread_rwlock_destroy(&fs->rwlock);
    free(fs);
}