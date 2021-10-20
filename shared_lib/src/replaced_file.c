#include <string.h>

#include "replaced_file.h"
#include "utils.h"

struct replaced_file {
    char* pathname;
    void* data;
    size_t data_size;
    queue_t* notify_lock_queue;
};

replaced_file_t* create_replfile()
{
    replaced_file_t* repl;
    CHECK_FATAL_EQ(repl, malloc(sizeof(replaced_file_t)), NULL, NO_MEM_FATAL);
    memset(repl, 0, sizeof(replaced_file_t));
    return repl;
}

void replfile_set_data(replaced_file_t* r, void* data, size_t data_size)
{
    NRET_IF(!r);
    r->data = data;
    r->data_size = data_size;
}

void replfile_set_locks_queue(replaced_file_t* r, queue_t* queue)
{
    NRET_IF(!r);
    r->notify_lock_queue = queue;
}

void replfile_set_pathname(replaced_file_t* r, const char* pathname)
{
    NRET_IF(!r);
    r->pathname = (char*)pathname;
}

size_t replfile_get_data_size(replaced_file_t* r)
{
    RET_IF(!r, 0);
    return r->data_size;
}

void* replfile_get_data(replaced_file_t* r)
{
    RET_IF(!r, NULL);
    return r->data;
}

queue_t* replfile_get_locks_queue(replaced_file_t* r)
{
    RET_IF(!r, NULL);
    return r->notify_lock_queue;
}

char* replfile_get_pathname(replaced_file_t* r)
{
    RET_IF(!r, NULL);
    return r->pathname;
}

void free_replfile(replaced_file_t* r)
{
    NRET_IF(!r);

    free(r->pathname);
    free(r->data);
    free_q(r->notify_lock_queue, free);
    free(r);
}