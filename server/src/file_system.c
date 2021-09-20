#include "file_system.h"

#include <string.h>
#include "replacement_policy.h"
#include "utils.h"

struct file_system {
    pthread_rwlock_t  rwlock;
    icl_hash_t* files_stored;

    size_t current_used_memory;
    size_t current_file_count;
    size_t max_memory_size;
    size_t max_file_count;
    int (*fs_policy)(file_stored_t* f1, file_stored_t* f2);
};

file_system_t* create_fs(size_t max_capacity, size_t max_file_count)
{
    file_system_t* fs;
    CHECK_FATAL_EQ(fs, malloc(sizeof(file_system_t)), NULL, NO_MEM_FATAL);
    memset(fs, 0, sizeof(file_system_t));

    fs->files_stored = icl_hash_create(10, NULL, NULL);
    fs->max_memory_size = max_capacity;
    fs->max_file_count = max_file_count;

    INIT_RWLOCK(&fs->rwlock);

    return fs;
}

void acquire_read_lock_fs(file_system_t* fs)
{
    NRET_IF(!fs);
    RLOCK_RWLOCK(&fs->rwlock);
}

void acquire_write_lock_fs(file_system_t* fs)
{
    NRET_IF(!fs);
    WLOCK_RWLOCK(&fs->rwlock);
}

void release_read_lock_fs(file_system_t* fs)
{
    NRET_IF(!fs);
    UNLOCK_RWLOCK(&fs->rwlock);
}

void release_write_lock_fs(file_system_t* fs)
{
    NRET_IF(!fs);
    UNLOCK_RWLOCK(&fs->rwlock);
}

void set_policy_fs(file_system_t* fs, char* policy)
{
    NRET_IF(!fs);

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

int is_size_available(file_system_t* fs, size_t size)
{
    RET_IF(!fs, 0);

    int remaining = fs->max_memory_size - (int)fs->current_used_memory;
    return remaining - size;
}

bool_t is_size_too_big(file_system_t* fs, size_t size)
{
    RET_IF(!fs, TRUE);

    return fs->max_memory_size <= size;
}

size_t get_file_count_fs(file_system_t* fs)
{
    RET_IF(!fs, 0);
    return fs->current_file_count;
}

bool_t is_file_count_full_fs(file_system_t* fs)
{
    RET_IF(!fs, TRUE);
    return ((int)fs->max_file_count - (int)fs->current_file_count) > 0;
}

file_stored_t* find_file_fs(file_system_t* fs, const char* pathname)
{
    RET_IF(!fs, NULL);
    return icl_hash_find(fs->files_stored, (char*)pathname);
}

int add_file_fs(file_system_t* fs, const char* pathname, file_stored_t* file)
{
    RET_IF(!fs, -1);

    char* pathname_cpy;
    int len = strlen(pathname);
    MAKE_COPY_BYTES(pathname_cpy, len, pathname);

    bool_t res = icl_hash_insert(fs->files_stored, pathname_cpy, file) != NULL;
    if(!res)
        free(pathname_cpy);
    else
        ++fs->current_file_count;

    return res;
}

int remove_file_fs(file_system_t* fs, const char* pathname)
{
    RET_IF(!fs, -1);
    bool_t res = icl_hash_delete(fs->files_stored, (char*)pathname, free, (void(*)(void*))free_file) == 0;
    if(res)
        --fs->current_file_count;

    return res;
}


void free_fs(file_system_t* fs)
{
    NRET_IF(!fs);

    icl_hash_destroy(fs->files_stored, free, (void (*)(void*))free_file);
    pthread_rwlock_destroy(&fs->rwlock);
    free(fs);
}