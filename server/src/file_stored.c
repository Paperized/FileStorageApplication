#include "file_stored.h"

#include <string.h>

struct file_stored {
    char* data;
    size_t size;
    int locked_by;
    linked_list_t* opened_by;
    queue_t* lock_queue;
    struct timespec creation_time;
    struct timespec last_use_time;
    bool_t can_be_removed;
    uint32_t use_frequency;
    pthread_mutex_t rw_mutex;
};

file_stored_t* create_file()
{
    file_stored_t* file;
    CHECK_FATAL_EQ(file, malloc(sizeof(file_stored_t)), NULL, NO_MEM_FATAL);
    memset(file, 0, sizeof(file_stored_t));    

    file->locked_by = -1;
    file->opened_by = ll_create();
    file->lock_queue = create_q();
    clock_gettime(CLOCK_REALTIME, &file->creation_time);
    file->last_use_time = file->creation_time;
    pthread_mutex_init(&file->rw_mutex, NULL);

    return file;
}

void free_file(file_stored_t* file)
{
    free(file->data);
    ll_free(file->opened_by, free);
    free_q(file->lock_queue, free);
}

int file_add_client(file_stored_t* file, int client)
{
    RET_IF(!file, -1);

    int* new_client;
    CHECK_FATAL_EQ(new_client, sizeof(int), NULL, NO_MEM_FATAL);
    *new_client = client;

    return ll_add_head(file->opened_by, new_client);
}

bool_t file_is_opened_by(file_stored_t* file, int client)
{
    RET_IF(!file, FALSE);

    node_t* curr = ll_get_head_node(file->opened_by);
    while(curr)
    {
        if(*((int*)node_get_value(curr)) == client)
            break;

        curr = node_get_next(curr);
    }

    return curr != NULL;
}

int file_remove_client(file_stored_t* file, int client)
{
    RET_IF(!file, -1);

    node_t* curr = ll_get_head_node(file->opened_by);
    while(curr)
    {
        if(*((int*)node_get_value(curr)) == client)
            break;

        curr = node_get_next(curr);
    }

    return curr ? ll_remove_node(file->opened_by, curr) : -1;
}

int file_enqueue_lock(file_stored_t* file, int client)
{
    RET_IF(!file, -1);

    int* new_client;
    CHECK_FATAL_EQ(new_client, sizeof(int), NULL, NO_MEM_FATAL);
    *new_client = client;

    return enqueue(file->lock_queue, new_client);
}

bool_t file_is_client_already_queued(file_stored_t* file, int client)
{
    RET_IF(!file, -1);

    node_t* curr = get_head_node_q(file->lock_queue);
    while(curr)
    {
        if(*((int*)node_get_value(curr)) == client)
            break;

        curr = node_get_next(curr);
    }

    return curr != NULL;
}

int file_dequeue_lock(file_stored_t* file)
{
    RET_IF(!file, -1);

    int* ptr = dequeue(file->lock_queue);
    int new_client = *ptr;
    free(ptr);

    return new_client;
}

uint32_t file_inc_frequency(file_stored_t* file, int step)
{
    RET_IF(!file, 0);

    file->use_frequency += step;
    return file->use_frequency;
}

char* file_get_data(file_stored_t* file)
{
    RET_IF(!file, NULL);
    return file->data;
}

size_t file_get_size(file_stored_t* file)
{
    RET_IF(!file, 0);
    return file->size;
}

int file_get_lock_owner(file_stored_t* file)
{
    RET_IF(!file, -1);
    return file->locked_by;
}

struct timespec* file_get_creation_time(file_stored_t* file)
{
    RET_IF(!file, NULL);
    return &file->creation_time;
}

struct timespec* file_get_last_use_time(file_stored_t* file)
{
    RET_IF(!file, NULL);
    return &file->last_use_time;
}

int file_get_use_frequency(file_stored_t* file)
{
    RET_IF(!file, 0);
    return file->use_frequency;
}

void file_set_lock_owner(file_stored_t* file, int lock_owner)
{
    NRET_IF(!file);
    file->locked_by = lock_owner;
}

void file_set_last_use_time(file_stored_t* file, struct timespec new_use_time)
{
    NRET_IF(!file);
    file->last_use_time = new_use_time;
}
