#include <stdlib.h>
#include <string.h>
#include "queue.h"

#define CHECK_QUEUE_PARAM(queue) if(queue == NULL) return -1
#define CHECK_QUEUE_PARAM2(queue) if(queue == NULL) return NULL

size_t count_q(queue_t* queue)
{
    if(queue == NULL)
        return 0;

    return ll_count(&queue->internal_list);
}

int enqueue(queue_t* queue, void* value)
{
    CHECK_QUEUE_PARAM(queue);

    return ll_add_head(&queue->internal_list, value);
}

void* dequeue(queue_t* queue)
{
    CHECK_QUEUE_PARAM2(queue);
    if(count_q(queue) <= 0)
        return NULL; /* random value (?), this function shouldnt be called without checking its count */
    
    void* res;
    ll_remove_last(&queue->internal_list, &res);
    return res;
}

void empty_q(queue_t* queue)
{
    if(queue == NULL) return;

    linked_list_t* ll = &queue->internal_list;
    while(ll->count != 0)
    {
        void* data;
        ll_remove_last(&queue->internal_list, &data);
        free(data);
    }
}

size_t count_safe(queue_t* queue, pthread_mutex_t* m)
{
    pthread_mutex_lock(m);
    size_t c = count_q(queue);
    pthread_mutex_unlock(m);

    return c;
}

int enqueue_safe(queue_t* queue, void* value, pthread_mutex_t* m)
{
    pthread_mutex_lock(m);
    int res = enqueue(queue, value);
    pthread_mutex_unlock(m);

    return res;
}

void* dequeue_safe(queue_t* queue, pthread_mutex_t* m)
{
    pthread_mutex_lock(m);
    void* val = dequeue(queue);
    pthread_mutex_unlock(m);

    return val;
}

void empty_safe(queue_t* queue, pthread_mutex_t* m)
{
    pthread_mutex_lock(m);
    empty_q(queue);
    pthread_mutex_unlock(m);
}