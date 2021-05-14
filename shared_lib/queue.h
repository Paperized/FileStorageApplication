#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <pthread.h>

typedef struct queue_item {
    void* value;
    struct queue_item* next;
} queue_item_t;

typedef struct queue {
    size_t count;
    queue_item_t* first;
    queue_item_t*  last;
    size_t mem_type_size; 
} queue_t;

size_t count(queue_t* queue);
int enqueue(queue_t* queue, void* value);
void* dequeue(queue_t* queue);
void empty(queue_t* queue);

size_t count_safe(queue_t* queue, pthread_mutex_t* m);
int enqueue_safe(queue_t* queue, void* value, pthread_mutex_t* m);
void* dequeue_safe(queue_t* queue, pthread_mutex_t* m);
void empty_safe(queue_t* queue, pthread_mutex_t* m);

#define INIT_EMPTY_QUEUE(mem_per_item) { 0, NULL, NULL, mem_per_item }
#define cast_to(type, value) (type)value

#endif