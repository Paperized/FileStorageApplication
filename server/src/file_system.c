#include "file_system.h"

#include <string.h>
#include "handle_client.h"
#include "replacement_policy.h"
#include "utils.h"

// Used to metrics each worker with the number of reqs handled
typedef struct pair_pthread_int {
    pthread_t pid;
    int val;
} pair_pthread_int_t;

// Metrics to be logged
struct file_system_metrics {
    size_t max_memory_reached;
    size_t max_num_files_reached;

    linked_list_t* max_req_threads;
};

struct file_system {
    pthread_rwlock_t  rwlock;
    icl_hash_t* files_stored;
    linked_list_t* filenames_stored;

    size_t current_used_memory;
    size_t current_file_count;
    size_t max_memory_size;
    size_t max_file_count;

    struct file_system_metrics metrics;
    pthread_rwlock_t    rwlock_metrics;
};

int(*fs_policy)(const void*, const void*) = replacement_policy_fifo;

file_system_t* create_fs(size_t max_capacity, size_t max_file_count)
{
    file_system_t* fs;
    CHECK_FATAL_EQ(fs, malloc(sizeof(file_system_t)), NULL, NO_MEM_FATAL);
    memset(fs, 0, sizeof(file_system_t));

    fs->files_stored = icl_hash_create(10, NULL, NULL);
    fs->filenames_stored = ll_create();
    fs->max_memory_size = max_capacity;
    fs->max_file_count = max_file_count;

    fs->metrics.max_req_threads = ll_create();

    INIT_RWLOCK(&fs->rwlock);
    INIT_RWLOCK(&fs->rwlock_metrics);
    return fs;
}

void set_workers_fs(file_system_t* fs, pthread_t* pids, int n)
{
    NRET_IF(!fs || !pids || n <= 0);

    WLOCK_RWLOCK(&fs->rwlock_metrics);
    if(ll_count(fs->metrics.max_req_threads) > 0)
        ll_empty(fs->metrics.max_req_threads, free);
        
    // match boundaries [0, n - 1]
    --n;
    while(n >= 0)
    {
        struct pair_pthread_int* pair;
        CHECK_FATAL_EQ(pair, malloc(sizeof(struct pair_pthread_int)), NULL, NO_MEM_FATAL);
        pair->pid = pids[n];
        pair->val = 0;
        ll_add_head(fs->metrics.max_req_threads, pair);
        --n;
    }
    UNLOCK_RWLOCK(&fs->rwlock_metrics);
}

int notify_worker_handled_req_fs(file_system_t* fs, pthread_t pid)
{
    RET_IF(!fs, -1);
    RET_IF(!fs->metrics.max_req_threads, 0);

    int has_inc = 0;
    WLOCK_RWLOCK(&fs->rwlock_metrics);
    FOREACH_LL(fs->metrics.max_req_threads) {
        pair_pthread_int_t* pair = VALUE_IT_LL(pair_pthread_int_t*);
        if(pair && pair->pid == pid)
        {
            ++pair->val;
            has_inc = 1;
            break;
        }
    }
    UNLOCK_RWLOCK(&fs->rwlock_metrics);
    return has_inc;
}

void shutdown_fs(file_system_t* fs)
{
    NRET_IF(!fs);

    size_t files_str_len;
    size_t files_num;
    RLOCK_RWLOCK(&fs->rwlock);
    char* files_printable = ll_explode_str(fs->filenames_stored, ',', &files_str_len);
    files_num = ll_count(fs->filenames_stored);
    UNLOCK_RWLOCK(&fs->rwlock);

    LOG_EVENT("FINAL_METRICS last files remaining(%zu): [%s]", 60 + files_str_len, files_num, files_str_len > 0 ? files_printable : "NONE");
    free(files_printable);

    struct file_system_metrics* metrics = &fs->metrics;
    RLOCK_RWLOCK(&fs->rwlock_metrics);

    PRINT_INFO_DEBUG("%zu max file count.", metrics->max_num_files_reached);
    LOG_EVENT("FINAL_METRICS Max file count %zu!", -1, metrics->max_num_files_reached);
    PRINT_INFO_DEBUG("%zuB max storage size.", metrics->max_memory_reached);
    LOG_EVENT("FINAL_METRICS Max storage size %zu!", -1, metrics->max_memory_reached);

    FOREACH_LL(metrics->max_req_threads) {
        pair_pthread_int_t* pair = VALUE_IT_LL(pair_pthread_int_t*);

        PRINT_INFO_DEBUG("Thread %lu handled %d requests!", pair->pid, pair->val);
        LOG_EVENT("FINAL_METRICS Thread %lu handled %d requests!", -1, pair->pid, pair->val);
    }
    UNLOCK_RWLOCK(&fs->rwlock_metrics);
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

file_stored_t** get_files_stored(file_system_t* fs)
{
    RET_IF(!fs, NULL);

    file_stored_t** files;
    size_t num = ll_count(fs->filenames_stored);
    if(num == 0)
        return NULL;
    
    CHECK_FATAL_EQ(files, malloc(sizeof(file_stored_t*) * num), NULL, NO_MEM_FATAL);
    
    int i = 0;
    FOREACH_LL(fs->filenames_stored) {
        char* filename = VALUE_IT_LL(char*);
        files[i++] = icl_hash_find(fs->files_stored, filename);
    }

    return files;
}

void set_policy_fs(file_system_t* fs, char* policy)
{
    NRET_IF(!fs);

    // pick a policy, default is fifo
    if(strncmp(policy, "FIFO", 4) == 0)
        fs_policy = replacement_policy_fifo;
    else if(strncmp(policy, "LRU", 3) == 0)
        fs_policy = replacement_policy_lru;
    else if(strncmp(policy, "LFU", 3) == 0)
        fs_policy = replacement_policy_lfu;
    else
        fs_policy = replacement_policy_fifo;
}

int is_size_available(file_system_t* fs, size_t size)
{
    RET_IF(!fs, 0);

    int remaining = fs->max_memory_size - (int)fs->current_used_memory;
    return -(remaining - size);
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
    return ((int)fs->max_file_count - (int)fs->current_file_count) <= 0;
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
    int len = strnlen(pathname, MAX_PATHNAME_API_LENGTH);
    MAKE_COPY_BYTES(pathname_cpy, len + 1, pathname);

    bool_t res = icl_hash_insert(fs->files_stored, pathname_cpy, file) != NULL;
    if(!res)
        free(pathname_cpy);
    else {
        // new copy to have unique ptrs
        MAKE_COPY_BYTES(pathname_cpy, len + 1, pathname);

        ll_add_head(fs->filenames_stored, pathname_cpy);
        notify_memory_changed_fs(fs, file_get_size(file));
        ++fs->current_file_count;
        SET_VAR_RWLOCK(fs->metrics.max_num_files_reached,
                        MAX(fs->metrics.max_num_files_reached, fs->current_file_count),
                        &fs->rwlock_metrics);
    }

    return res;
}

int remove_file_fs(file_system_t* fs, const char* pathname, bool_t is_replacement)
{
    RET_IF(!fs, -1);
    file_stored_t* file = icl_hash_find(fs->files_stored, (char*)pathname);
    if(!file)
        return 0;

    size_t data_size = file_get_size(file);
    ll_remove_str(fs->filenames_stored, (char*)pathname);
    bool_t res = icl_hash_delete(fs->files_stored, (char*)pathname, free, FREE_FUNC(is_replacement ? free_file_for_replacement : free_file)) == 0;
    if(res)
    {
        fs->current_used_memory -= data_size;
        --fs->current_file_count;
    }

    return res;
}

int notify_memory_changed_fs(file_system_t* fs, int amount)
{
    RET_IF(!fs, 0);
    if(amount == 0)
        return fs->current_used_memory;
    
    fs->current_used_memory += amount;
    SET_VAR_RWLOCK(fs->metrics.max_memory_reached,
                        MAX(fs->metrics.max_memory_reached, fs->current_used_memory),
                        &fs->rwlock_metrics);

    return fs->current_used_memory;
}

int notify_client_disconnected_fs(file_system_t* fs, int fd)
{
    RET_IF(!fs || fd == -1, -1);

    FOREACH_LL(fs->filenames_stored)
    {
        file_stored_t* file = icl_hash_find(fs->files_stored, VALUE_IT_LL(void*));
        file_close_client(file, fd);
        int new_owner = file_delete_lock_client(file, fd);
        if(new_owner != -1)
        {
            notify_given_lock(new_owner);
        }
    }

    return 0;
}

void free_fs(file_system_t* fs)
{
    NRET_IF(!fs);

    icl_hash_destroy(fs->files_stored, free, FREE_FUNC(free_file));
    ll_free(fs->filenames_stored, free);
    pthread_rwlock_destroy(&fs->rwlock);
    pthread_rwlock_destroy(&fs->rwlock_metrics);
    ll_free(fs->metrics.max_req_threads, free);
    free(fs);
}