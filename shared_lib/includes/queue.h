#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <pthread.h>
#include "linked_list.h"

typedef struct queue queue_t;

// Create a new queue
queue_t* create_q();

// Get count of this queue
size_t count_q(queue_t* queue);

// Enqueue a value to this queue
int enqueue(queue_t* queue, void* value);

// Dequeue a value from this queue
void* dequeue(queue_t* queue);

// Remove all nodes inside this queue and count to zero, the data pointed by nodes will be free with the second argument
// If free_func is NULL the data will not be freed
void empty_q(queue_t* queue, void (*free_func)(void*));

// Remove a node inside this queue, does NOT free data pointer by it
int remove_node_q(queue_t* queue, node_t* node);

// Get the first node of this queue
node_t* get_head_node_q(queue_t* queue);

// Remove all nodes inside this queue and count to zero, the data pointed by nodes will be free with the second argument
// If free_func is NULL it will be set automatically with free(---)
void free_q(queue_t* queue, void (*free_func)(void*));

// Helper macro to cycle through nodes
#define FOREACH_Q(q) for(node_t* local_node = get_head_node_q(q); local_node != NULL; local_node = node_get_next(local_node))
// Helper macro to get the data pointer by the current node and casted by a type
#define VALUE_IT_Q(type) ((type)node_get_value(local_node))

#endif