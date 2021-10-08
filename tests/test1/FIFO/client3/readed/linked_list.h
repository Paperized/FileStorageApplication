#ifndef _LINKED_LIST_H_
#define _LINKED_LIST_H_

#include <stdlib.h>

typedef struct node node_t;
typedef struct linked_list linked_list_t;

linked_list_t* ll_create();

size_t ll_count(const linked_list_t* ll);
int ll_add_head(linked_list_t* ll, void* value);
int ll_add_tail(linked_list_t* ll, void* value);
void ll_remove_first(linked_list_t* ll, void** value);
void ll_remove_last(linked_list_t* ll, void** value);
void* ll_get_first(linked_list_t* ll);
void* ll_get_last(linked_list_t* ll);
node_t* ll_get_head_node(linked_list_t* ll);
int ll_remove_node(linked_list_t* ll, node_t* node);

void ll_remove_str(linked_list_t* ll, char* str);

void ll_empty(linked_list_t* ll, void (*free_func)(void*));
void ll_free(linked_list_t* ll, void (*free_func)(void*));

void* node_get_value(node_t* node);
node_t* node_get_next(node_t* node);

void* ll_to_array(linked_list_t* ll);

char* ll_explode_str(linked_list_t* ll, char divisor, size_t* len_output);
int ll_contains_str(const linked_list_t* ll, char* str);
int ll_get_max_int(const linked_list_t* ll);

#define FREE_FUNC(func) ((void(*)(void*))(func))
#define FOREACH_LL(ll) for(node_t* local_node = ll_get_head_node(ll); local_node != NULL; local_node = node_get_next(local_node))
#define VALUE_IT_LL(type) ((type)node_get_value(local_node))
#define CURR_IT_LL local_node

#endif

