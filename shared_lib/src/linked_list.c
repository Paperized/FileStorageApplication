#include <string.h>
#include "linked_list.h"
#include "utils.h"

struct node {
    void* value;
    struct node* next;
};

struct linked_list {
    size_t count;
    node_t* head;
    node_t* tail;
};

int malloc_node(node_t** node)
{
    CHECK_FATAL_ERRNO(*node, malloc(sizeof(node_t)), NO_MEM_FATAL);
    if(node == NULL)
        return -1;
    
    (*node)->next = NULL;
    return 0;
}

void* node_get_value(node_t* node)
{
    if(node == NULL) return NULL;
    return node->value;
}

node_t* node_get_next(node_t* node)
{
    if(node == NULL) return NULL;
    return node->next;
}

linked_list_t* ll_create()
{
    linked_list_t* new_list;
    CHECK_FATAL_ERRNO(new_list, malloc(sizeof(linked_list_t)), NO_MEM_FATAL);
    memset(new_list, 0, sizeof(linked_list_t));
    return new_list;
}

size_t ll_count(const linked_list_t* ll)
{
    return ll->count;
}

int ll_add_head(linked_list_t* ll, void* value)
{
    node_t* new_item = NULL;
    malloc_node(&new_item);

    new_item->value = value;
    new_item->next = ll->head;
    ll->head = new_item;

    if(ll->tail == NULL)
        ll->tail = new_item;

    ll->count += 1;
    return 0;
}

int ll_add_tail(linked_list_t* ll, void* value)
{
    node_t* new_item = NULL;
    malloc_node(&new_item);

    new_item->value = value;
    if(ll->count == 0)
    {
        ll->head = ll->tail = new_item;
    }
    else
    {
        ll->tail->next = new_item;
        ll->tail = new_item;
    }

    ll->count += 1;
    return 0;
}

void ll_remove_first(linked_list_t* ll, void** value)
{
    if(ll->count <= 0)
    {
        if(value != NULL)
            *value = NULL;
        return;
    }

    node_t* first = ll->head;
    node_t* next = first->next;
    if(value != NULL)
        *value = first->value;

    if(next == NULL)
    {
        ll->head = ll->tail = NULL;
    }
    else
    {
        ll->head = next;
    }

    free(first);
    ll->count -= 1;
}

void ll_remove_last(linked_list_t* ll, void** value)
{
    if(ll->count <= 0)
    {
        if(value != NULL)
            *value = NULL;
        return;
    }

    node_t* last = ll->tail;
    if(value != NULL)
        *value = last->value;

    if(last == ll->head)
    {
        ll->head = ll->tail = NULL;
    }
    else
    {
        node_t* curr = ll->head;
        node_t* prev = NULL;
        while(curr != last)
        {
            prev = curr;
            curr = curr->next;
        }

        ll->tail = prev;
    }

    free(last);
    ll->count -= 1;
}

void* ll_get_first(linked_list_t* ll)
{
    if(ll->count <= 0)
    {
        return NULL;
    }

    return ll->head->value;
}

void* ll_get_last(linked_list_t* ll)
{
    if(ll->count <= 0)
    {
        return NULL;
    }

    return ll->tail->value;
}

node_t* ll_get_head_node(linked_list_t* ll)
{
    if(ll == NULL) return NULL;

    return ll->head;
}

int ll_remove_node(linked_list_t* ll, node_t* node)
{
    if(ll == NULL || node == NULL) return -1;

    node_t* curr = ll->head;
    node_t* prev = NULL;
    while(curr != node && curr != NULL)
    {
        prev = curr;
        curr = curr->next;
    }

    if(curr == NULL)
        return -1;

    if(prev == NULL)
    {
        ll->head = ll->tail = NULL;
    }
    else
    {
        prev->next = node->next;
    }

    ll->count -= 1;
    free(node);
    return 0;
}

void ll_remove_str(linked_list_t* ll, char* str)
{
    if(ll == NULL || str == NULL) return;

    node_t* curr = ll->head;
    node_t* prev = NULL;
    while(curr != NULL && strncmp(curr->value, str, 108) != 0)
    {
        prev = curr;
        curr = curr->next;
    }

    if(curr != NULL)
    {
        prev->next = curr->next;
        free(curr->value);
        free(curr);
        ll->count -= 1;
    }
}

int ll_int_get_max(const linked_list_t* ll)
{
    if(ll == NULL) return -1;

    int max = -1;
    node_t* curr = ll->head;
    while(curr != NULL)
    {
        int new_val = (int)curr->value;
        if(max < new_val)
            max = new_val;

        curr = curr->next;
    }

    return max;
}

void ll_empty(linked_list_t* ll, void (*free_func)(void*))
{
    if(ll == NULL) return;

    while(ll->count > 0)
    {
        void* value;
        ll_remove_last(ll, &value);
        if(free_func)
            free_func(value);
    }
}

void ll_free(linked_list_t* ll, void (*free_func)(void*))
{
    if(ll == NULL) return;

    ll_empty(ll, free_func ? free_func : free);
    free(ll);
}

int ll_contains_str(const linked_list_t* ll, char* str)
{
    node_t* curr = ll->head;
    while(curr != NULL)
    {
        if(strncmp(curr->value, str, 108) == 0)
            return 1;

        curr = curr->next;
    }

    return 0; 
}