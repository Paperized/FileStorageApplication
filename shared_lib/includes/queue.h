#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <pthread.h>
#include "linked_list.h"

typedef struct queue queue_t;

size_t count_q(queue_t* queue);
int enqueue(queue_t* queue, void* value);
void* dequeue(queue_t* queue);
void empty_q(queue_t* queue);

size_t count_safe(queue_t* queue, pthread_mutex_t* m);
int enqueue_safe(queue_t* queue, void* value, pthread_mutex_t* m);
void* dequeue_safe(queue_t* queue, pthread_mutex_t* m);
void empty_safe(queue_t* queue, pthread_mutex_t* m);

#define INIT_EMPTY_QUEUE { INIT_EMPTY_LL }
#define enqueue_m(queue, value) enqueue(queue, (void*) value)
#define enqueue_safe_m(queue, value, m) enqueue_safe(queue, (void*) value, m)
#define cast_to(type, value) (type)value

#endif