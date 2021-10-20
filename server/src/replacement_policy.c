#include <string.h>

#include "replacement_policy.h"
#include "replaced_file.h"

bool_t run_replacement_algorithm(const char* skip_file, size_t mem_needed, linked_list_t** output)
{
    file_system_t* fs = get_fs();
    linked_list_t* freed = ll_create();

    file_stored_t** all_files = get_files_stored(fs);
    size_t files_count = get_file_count_fs(fs);
    qsort(all_files, files_count, sizeof(file_stored_t*), fs_policy);

    size_t mem_freed = 0;
    int i = 0;
    // run simulation, to avoid deleting everything without even adding the actual file
    while(mem_freed < mem_needed && i < files_count)
    {
        file_stored_t* curr = all_files[i];
        char* curr_pathname = file_get_pathname(curr);
        if(strncmp(curr_pathname, skip_file, MAX_PATHNAME_API_LENGTH) == 0)
        {
            ++i;
            continue;
        }
        
        size_t curr_size = file_get_size(curr);
        queue_t* locks_queue = file_get_locks_queue(curr);
        void* data = file_get_data(curr);

        replaced_file_t* entry = create_replfile();
        replfile_set_pathname(entry, curr_pathname);
        replfile_set_data(entry, data, curr_size);
        replfile_set_locks_queue(entry, locks_queue);

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
        replaced_file_t* entry = node_get_value(removing_node);
        remove_file_fs(fs, replfile_get_pathname(entry), TRUE);
        removing_node = node_get_next(removing_node);
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
        return (t1->tv_sec - t2->tv_sec);

    return (t1->tv_nsec - t2->tv_nsec);
}

static int replacement_cmp_size(file_stored_t* f1, file_stored_t* f2)
{
    return file_get_size(f1) - file_get_size(f2);
}

int replacement_policy_fifo(const void* f1_ptr, const void* f2_ptr)
{
    file_stored_t* f1 = *(file_stored_t**)f1_ptr;
    file_stored_t* f2 = *(file_stored_t**)f2_ptr;
    
    int cmp = cmp_time(file_get_creation_time(f1), file_get_creation_time(f2));
    if(cmp == 0)
        return replacement_cmp_size(f1, f2);

    return cmp;
    return 0;
}

int replacement_policy_lru(const void* f1_ptr, const void* f2_ptr)
{
    file_stored_t* f1 = *(file_stored_t**)f1_ptr;
    file_stored_t* f2 = *(file_stored_t**)f2_ptr;

    int cmp = cmp_time(file_get_last_use_time(f1), file_get_last_use_time(f2));
    if(cmp == 0)
        return replacement_cmp_size(f1, f2);

    return cmp;
}

int replacement_policy_lfu(const void* f1_ptr, const void* f2_ptr)
{
    file_stored_t* f1 = *(file_stored_t**)f1_ptr;
    file_stored_t* f2 = *(file_stored_t**)f2_ptr;

    int cmp = file_get_use_frequency(f1) - file_get_use_frequency(f2);
    if(cmp == 0)
        return replacement_cmp_size(f1, f2);

    return cmp;
}