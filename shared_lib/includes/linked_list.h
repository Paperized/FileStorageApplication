#ifndef _LINKED_LIST_H_
#define _LINKED_LIST_H_

#include <stdlib.h>

typedef struct node node_t;
typedef struct linked_list linked_list_t;

// Create a new linked list
linked_list_t* ll_create();

// Get count of this linked list
size_t ll_count(const linked_list_t* ll);

// Add value as the first element in this linked list
int ll_add_head(linked_list_t* ll, void* value);

// Add value as the last element in this linked list
int ll_add_tail(linked_list_t* ll, void* value);

// Remove the first node from this linked list and set value to the data pointed by that node
void ll_remove_first(linked_list_t* ll, void** value);

// Remove the last node from this linked list and set value to the data pointed by that node
void ll_remove_last(linked_list_t* ll, void** value);

// Get data pointed by the first node in linked list
void* ll_get_first(linked_list_t* ll);

// Get data pointed by the last node in linked list
void* ll_get_last(linked_list_t* ll);

// Get the first node of this linked list
node_t* ll_get_head_node(linked_list_t* ll);

// Remove a node inside this linked list, does NOT free data pointer by it
int ll_remove_node(linked_list_t* ll, node_t* node);

// Remove a string from this linked list
// Assumes the nodes contains strings
void ll_remove_str(linked_list_t* ll, char* str);

// Remove all nodes inside this linked list and count to zero, the data pointed by nodes will be free with the second argument
// If free_func is NULL the data will not be freed
void ll_empty(linked_list_t* ll, void (*free_func)(void*));

// Remove all nodes inside this linked list and count to zero, the data pointed by nodes will be free with the second argument
// If free_func is NULL it will be set automatically with free(---)
void ll_free(linked_list_t* ll, void (*free_func)(void*));

// Get the data pointed by this node
void* node_get_value(node_t* node);

// Get the next node pointed by this node
node_t* node_get_next(node_t* node);

// All strings insides this linked list will be concatenated in a unique malloc'ed string with a divisor, len_output will be it's length
// Assumes the nodes contains strings
char* ll_explode_str(linked_list_t* ll, char divisor, size_t* len_output);

// Check whether this linked list contains a string
// Assumes the nodes contains strings
int ll_contains_str(const linked_list_t* ll, char* str);

// Get the max integer inside this linked list
// Assumes the nodes contains ints
int ll_get_max_int(const linked_list_t* ll);

// Helper macro to cast a function to a free-like prototype
#define FREE_FUNC(func) ((void(*)(void*))(func))
// Helper macro to cycle through nodes
#define FOREACH_LL(ll) for(node_t* local_node = ll_get_head_node(ll); local_node != NULL; local_node = node_get_next(local_node))
// Helper macro to get the data pointer by the current node and casted by a type
#define VALUE_IT_LL(type) ((type)node_get_value(local_node))
// Helper macro to get the current node
#define CURR_IT_LL local_node

#endif

