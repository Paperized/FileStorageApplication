#include "replacement_policy.h"
#include "network_file.h"

#define FS_POLICY_FUNC ((int(*)(const void*, const void *))fs_policy)

bool_t run_replacement_algorithm(const char* skip_file, size_t mem_needed, linked_list_t** output)
{
    file_system_t* fs = get_fs();
    linked_list_t* freed = ll_create();

    file_stored_t** all_files = get_files_stored(fs);
    size_t files_count = get_file_count_fs(fs);
    qsort(all_files, files_count, sizeof(file_stored_t*), FS_POLICY_FUNC);

    size_t mem_freed = 0;
    int i = 0;
    // run simulation, to avoid deleting everything without even adding the actual file
    while(mem_freed < mem_needed && i < files_count)
    {
        file_stored_t* curr = all_files[i];
        char* curr_pathname = file_get_pathname(curr);
        if(strncmp(curr_pathname, skip_file, MAX_PATHNAME_API_LENGTH) == 0)
            continue;
        
        size_t curr_size = file_get_size(curr);
        queue_t* locks_queue = file_get_locks_queue(curr);
        void* data = file_get_data(curr);

        network_file_t* entry = create_netfile();
        netfile_set_pathname(entry, curr_pathname);
        netfile_set_data(entry, data, curr_size);
        netfile_set_locks_queue(entry, locks_queue);

        ll_add_tail(freed, entry);
        mem_freed += curr_size;
        ++i;
    }

    if(mem_freed < mem_needed)
    {
        if(output)
            *output = NULL;
        free(all_files);
        ll_free(freed, free);
        return FALSE;
    }

    node_t* removing_node = ll_get_head_node(freed);
    while(removing_node)
    {
        network_file_t* entry = node_get_value(removing_node);
        remove_file_fs(fs, netfile_get_pathname(entry), TRUE);
    }

    free(all_files);
    if(output)
        *output = freed;
    return TRUE;
}

// 0 == equal, > 0 t1 greater, < 0 t1 less
static int cmp_time(struct timespec* t1, struct timespec* t2)
{
    if(t1->tv_sec != t2->tv_sec)
        return t1->tv_sec - t2->tv_sec;

    return t1->tv_nsec - t2->tv_nsec;
}

int replacement_policy_fifo(file_stored_t* f1, file_stored_t* f2)
{
    return cmp_time(file_get_creation_time(f1), file_get_creation_time(f2));
}

int replacement_policy_lru(file_stored_t* f1, file_stored_t* f2)
{
    return cmp_time(file_get_last_use_time(f1), file_get_last_use_time(f2));
}

int replacement_policy_lfu(file_stored_t* f1, file_stored_t* f2)
{
    return file_get_use_frequency(f1) - file_get_use_frequency(f2);
}