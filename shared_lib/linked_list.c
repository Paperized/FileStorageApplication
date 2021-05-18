#include <string.h>
#include "linked_list.h"

#define CHECK_MALLOC_NODE(output) if(malloc_node(output) != 0) return -1;

int malloc_node(node_t** node)
{
    *node = malloc(sizeof(node_t));
    if(node == NULL)
        return -1;
        
    return 0;
}

size_t ll_count(linked_list_t* ll)
{
    return ll->count;
}

int ll_add(linked_list_t* ll, void* value)
{
    node_t* new_item = NULL;
    CHECK_MALLOC_NODE(&new_item);

    new_item->value = value;
    new_item->next = ll->head;
    ll->head = new_item;

    if(ll->tail == NULL)
        ll->tail = new_item;

    ll->count += 1;
    return 0;
}

void ll_remove_first(linked_list_t* ll, void* value)
{
    if(ll->count <= 0)
    {
        value = NULL;
        return;
    }

    node_t* first = ll->head;
    node_t* next = first->next;
    value = first->value;

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
        value = NULL;
        return;
    }

    node_t* last = ll->tail;
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

int ll_int_get_max(linked_list_t* ll)
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