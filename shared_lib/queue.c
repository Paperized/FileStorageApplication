#include <stdlib.h>
#include "queue.h"

#define CHECK_QUEUE_PARAM(queue) if(queue == NULL) return -1
#define CHECK_QUEUE_PARAM2(queue) if(queue == NULL) return

int count(queue_t* queue)
{
    if(queue == NULL)
        return 0;

    return queue->count;
}

int enqueue(queue_t* queue, int value)
{
    CHECK_QUEUE_PARAM(queue);

    queue_item_t* new_item = malloc(sizeof(queue_item_t));
    if(new_item == NULL)
        return -1;

    new_item->value = value;
    new_item->next = queue->first;
    queue->first = new_item;

    if(queue->last == NULL)
        queue->last = new_item;

    queue->count += 1;
    return 0;
}

int dequeue(queue_t* queue)
{
    CHECK_QUEUE_PARAM(queue);
    if(queue->count <= 0)
        return 0; /* random value (?), this function shouldnt be called without checking its count */
    
    queue_item_t* last = queue->last;
    int value = last->value;

    if(last == queue->first)
    {
        queue->first = queue->last = NULL;
    }
    else
    {
        queue_item_t* curr = queue->first;
        queue_item_t* prev = NULL;
        while(curr != last)
        {
            prev = curr;
            curr = curr->next;
        }

        queue->last = prev;
    }

    free(last);
    queue->count -= 1;
    return value;
}

void empty(queue_t* queue)
{
    CHECK_QUEUE_PARAM2(queue);

    queue_item_t* curr = queue->first;
    while(curr != NULL)
    {
        queue_item_t* tmp = curr->next;
        free(curr);
        curr = tmp;
    }

    queue->count = 0;
    queue->first = queue->last = NULL;
}

int count_safe(queue_t* queue, pthread_mutex_t* m)
{
    pthread_mutex_lock(m);
    int c = count(queue);
    pthread_mutex_unlock(m);

    return c;
}

int enqueue_safe(queue_t* queue, int value, pthread_mutex_t* m)
{
    pthread_mutex_lock(m);
    int res = enqueue(queue, value);
    pthread_mutex_unlock(m);

    return res;
}

int dequeue_safe(queue_t* queue, pthread_mutex_t* m)
{
    pthread_mutex_lock(m);
    int val = dequeue(queue);
    pthread_mutex_unlock(m);

    return val;
}

void empty_safe(queue_t* queue, pthread_mutex_t* m)
{
    pthread_mutex_lock(m);
    empty(queue);
    pthread_mutex_unlock(m);
}