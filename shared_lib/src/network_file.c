#include <string.h>

#include "network_file.h"
#include "utils.h"

struct network_file {
    char* pathname;
    void* data;
    size_t data_size;
    queue_t* notify_lock_queue;
};

network_file_t* create_netfile()
{
    network_file_t* repl;
    CHECK_FATAL_EQ(repl, malloc(sizeof(network_file_t)), NULL, NO_MEM_FATAL);
    memset(repl, 0, sizeof(network_file_t));
    return repl;
}

void netfile_set_data(network_file_t* r, void* data, size_t data_size)
{
    NRET_IF(!r);
    r->data = data;
    r->data_size = data_size;
}

void netfile_set_locks_queue(network_file_t* r, queue_t* queue)
{
    NRET_IF(!r);
    r->notify_lock_queue = queue;
}

void netfile_set_pathname(network_file_t* r, const char* pathname)
{
    NRET_IF(!r);
    r->pathname = (char*)pathname;
}

size_t netfile_get_data_size(network_file_t* r)
{
    RET_IF(!r, 0);
    return r->data_size;
}

void* netfile_get_data(network_file_t* r)
{
    RET_IF(!r, NULL);
    return r->data;
}

queue_t* netfile_get_locks_queue(network_file_t* r)
{
    RET_IF(!r, NULL);
    return r->notify_lock_queue;
}

char* netfile_get_pathname(network_file_t* r)
{
    RET_IF(!r, NULL);
    return r->pathname;
}

void free_netfile(network_file_t* r)
{
    NRET_IF(!r);

    free(r->pathname);
    free(r->data);
    free_q(r->notify_lock_queue, free);
}