#ifndef _LINKED_LIST_H_
#define _LINKED_LIST_H_

#include <stdlib.h>

typedef struct node {
    void* value;
    struct node* next;
} node_t;

typedef struct linked_list {
    size_t count;
    node_t* head;
    node_t* tail;
} linked_list_t;

size_t ll_count(linked_list_t* ll);
int ll_add_head(linked_list_t* ll, void* value);
int ll_add_tail(linked_list_t* ll, void* value);
void ll_remove_first(linked_list_t* ll, void* value);
void ll_remove_last(linked_list_t* ll, void** value);
void* ll_get_first(linked_list_t* ll);
void* ll_get_last(linked_list_t* ll);
node_t* ll_get_head_node(linked_list_t* ll);
int ll_remove_node(linked_list_t* ll, node_t* node);

void ll_empty(linked_list_t* ll);

/* Integer functions */
int ll_int_get_max(linked_list_t* ll);

#define INIT_EMPTY_LL { 0, NULL, NULL }

#endif

