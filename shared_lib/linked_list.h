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
    size_t mem_value_size;
} linked_list_t;

size_t ll_count(linked_list_t* ll);
int ll_add(linked_list_t* ll, void* value);
void ll_remove_first(linked_list_t* ll, void* value);
void ll_remove_last(linked_list_t* ll, void* value);
void* ll_get_first(linked_list_t* ll);
void* ll_get_last(linked_list_t* ll);
node_t* ll_get_head_node(linked_list_t* ll);
int ll_remove_node(linked_list_t* ll, node_t* node);

#define INIT_EMPTY_LL(mem_per_item) { 0, NULL, NULL, mem_per_item }

#endif

