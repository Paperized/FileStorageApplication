#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <pthread.h>

typedef struct queue_item {
    int value;
    struct queue_item* next;
} queue_item_t;

typedef struct queue {
    int count;
    queue_item_t* first;
    queue_item_t*  last;
} queue_t;

int count(queue_t* queue);
int enqueue(queue_t* queue, int value);
int dequeue(queue_t* queue);
void empty(queue_t* queue);

int count_safe(queue_t* queue, pthread_mutex_t* m);
int enqueue_safe(queue_t* queue, int value, pthread_mutex_t* m);
int dequeue_safe(queue_t* queue, pthread_mutex_t* m);
void empty_safe(queue_t* queue, pthread_mutex_t* m);

#define INIT_EMPTY_QUEUE { 0, NULL, NULL }


#endif