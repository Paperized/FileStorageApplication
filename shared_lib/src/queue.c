#include <stdlib.h>
#include <string.h>

#include "queue.h"
#include "utils.h"

struct queue {
    linked_list_t* internal_list;
};

queue_t* create_q()
{
    queue_t* queue;
    CHECK_FATAL_EQ(queue, malloc(sizeof(queue_t)), NULL, NO_MEM_FATAL);
    queue->internal_list = ll_create();
    return queue;
}

int remove_node_q(queue_t* queue, node_t* node)
{
    RET_IF(!queue || !node, -1);

    return ll_remove_node(queue->internal_list, node);
}

node_t* get_head_node_q(queue_t* queue)
{
    RET_IF(!queue, NULL);

    return ll_get_head_node(queue->internal_list);
}

size_t count_q(queue_t* queue)
{
    RET_IF(!queue, 0);

    return ll_count(queue->internal_list);
}

int enqueue(queue_t* queue, void* value)
{
    RET_IF(!queue, -1);

    return ll_add_head(queue->internal_list, value);
}

void* dequeue(queue_t* queue)
{
    RET_IF(!queue || count_q(queue) <= 0, NULL);
    
    void* res;
    ll_remove_last(queue->internal_list, &res);
    return res;
}

void empty_q(queue_t* queue, void (*free_func)(void*))
{
    NRET_IF(!queue);

    ll_empty(queue->internal_list, free_func);
}

void free_q(queue_t* queue, void (*free_func)(void*))
{
    NRET_IF(!queue);

    ll_free(queue->internal_list, free_func);
    free(queue);
}

size_t count_qsafe(queue_t* queue, pthread_mutex_t* m)
{
    LOCK_MUTEX(m);
    size_t c = count_q(queue);
    UNLOCK_MUTEX(m);

    return c;
}

int enqueue_qsafe(queue_t* queue, void* value, pthread_mutex_t* m)
{
    LOCK_MUTEX(m);
    int res = enqueue(queue, value);
    pthread_mutex_unlock(m);

    return res;
}

void* dequeue_qsafe(queue_t* queue, pthread_mutex_t* m)
{
    LOCK_MUTEX(m);
    void* val = dequeue(queue);
    UNLOCK_MUTEX(m);

    return val;
}

void empty_qsafe(queue_t* queue, void (*free_func)(void*), pthread_mutex_t* m)
{
    LOCK_MUTEX(m);
    empty_q(queue, free_func);
    UNLOCK_MUTEX(m);
}

void free_qsafe(queue_t* queue, void (*free_func)(void*), pthread_mutex_t* m)
{
    LOCK_MUTEX(m);
    free_q(queue, free_func);
    UNLOCK_MUTEX(m);
}