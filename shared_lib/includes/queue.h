#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <pthread.h>
#include "linked_list.h"

typedef struct queue queue_t;

queue_t* create_q();

size_t count_q(queue_t* queue);
int enqueue(queue_t* queue, void* value);
void* dequeue(queue_t* queue);
void empty_q(queue_t* queue, void (*free_func)(void*));

int remove_node_q(queue_t* queue, node_t* node);

node_t* get_head_node_q(queue_t* queue);

void free_q(queue_t* queue, void (*free_func)(void*));

size_t count_qsafe(queue_t* queue, pthread_mutex_t* m);
int enqueue_qsafe(queue_t* queue, void* value, pthread_mutex_t* m);
void* dequeue_qsafe(queue_t* queue, pthread_mutex_t* m);
void empty_qsafe(queue_t* queue, void (*free_func)(void*), pthread_mutex_t* m);
void free_qsafe(queue_t* queue, void (*free_func)(void*), pthread_mutex_t* m);

#define FOREACH_Q(q) for(node_t* local_node = get_head_node_q(q); local_node != NULL; local_node = node_get_next(local_node))
#define VALUE_IT_Q(type) ((type)node_get_value(local_node))

#endif