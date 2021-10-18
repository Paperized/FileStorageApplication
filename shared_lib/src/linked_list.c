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
    RET_IF(!node, -1);
    CHECK_FATAL_ERRNO(*node, malloc(sizeof(node_t)), NO_MEM_FATAL);
    
    (*node)->next = NULL;
    return 0;
}

void* node_get_value(node_t* node)
{
    RET_IF(!node, NULL);
    return node->value;
}

node_t* node_get_next(node_t* node)
{
    RET_IF(!node, NULL);
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
    RET_IF(!ll, 0);

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
    NRET_IF(!ll);

    if(ll->count <= 0)
    {
        if(value != NULL)
            *value = NULL;
        return;
    }

    node_t* last = ll->tail;
    if(value)
        *value = last->value;

    if(ll->count == 1)
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
            curr = curr->next; // SEGV ogni tanto
        }

        ll->tail = prev;
    }

    free(last);
    ll->count -= 1;
}

void* ll_get_first(linked_list_t* ll)
{
    RET_IF(!ll || ll->count <= 0, NULL);

    return ll->head->value;
}

void* ll_get_last(linked_list_t* ll)
{
    RET_IF(!ll || ll->count <= 0, NULL);

    return ll->tail->value;
}

node_t* ll_get_head_node(linked_list_t* ll)
{
    RET_IF(!ll, NULL);

    return ll->head;
}

int ll_remove_node(linked_list_t* ll, node_t* node)
{
    RET_IF(!ll || !node, -1);

    node_t* curr = ll->head;
    node_t* prev = NULL;
    while(curr && curr != node)
    {
        prev = curr;
        curr = curr->next;
    }

    if(curr == NULL)
        return 0;

    if(prev == NULL)
    {
        ll->head = curr->next;
        if(ll->head == NULL)
            ll->tail = NULL;
    }
    else
        prev->next = node->next;

    ll->count -= 1;
    free(node);

    return 1;
}

void ll_remove_str(linked_list_t* ll, char* str)
{
    NRET_IF(!ll || !str);

    node_t* curr = ll->head;
    node_t* prev = NULL;
    while(curr && strncmp(curr->value, str, 108) != 0)
    {
        prev = curr;
        curr = curr->next;
    }

    if(curr)
    {
        if(prev)
        {
            prev->next = curr->next;
            if(curr == ll->tail)
                ll->tail = prev;
            ll->count -= 1;
        }
        else
        {
            ll->head = curr->next;
            if(ll->head == NULL)
                ll->tail = NULL;

            ll->count -= 1;
        }

        free(curr->value);
        free(curr);
    }
}

char* ll_explode_str(linked_list_t* ll, char divisor, size_t* len_output)
{
    RET_IF(!ll, NULL);

    char* full_str;
    size_t max_size = ll->count * (108 + 1);
    CHECK_FATAL_EQ(full_str, malloc(max_size), NULL, NO_MEM_FATAL);

    int str_index = 0;
    FOREACH_LL(ll)
    {
        char* file_path = VALUE_IT_LL(char*);
        bool_t is_last = node_get_next(CURR_IT_LL) == NULL;
        
        char next_token[4] = "%s \0";
        next_token[2] = is_last ? '\0' : divisor;

        snprintf(full_str + str_index, max_size, next_token, file_path);
        str_index += strnlen(file_path, 108) + !is_last;
    }

    if(len_output)
        *len_output = str_index;

    return full_str;
}

void ll_empty(linked_list_t* ll, void (*free_func)(void*))
{
    NRET_IF(!ll);

    while(ll->count > 0)
    {
        void* value;
        ll_remove_last(ll, &value);
        if(free_func)
            free_func(value);
    }
}

void* ll_to_array(linked_list_t* ll)
{
    RET_IF(!ll, NULL);

    void** array;
    CHECK_FATAL_EQ(array, malloc(sizeof(void*) * ll->count), NULL, NO_MEM_FATAL);
    node_t* node = ll->head;
    int i = 0;
    while(node)
    {
        array[i] = node->value;
        ++i;
    }

    return array;
}

void ll_free(linked_list_t* ll, void (*free_func)(void*))
{
    NRET_IF(!ll);

    ll_empty(ll, free_func ? free_func : free);
    free(ll);
}

int ll_contains_str(const linked_list_t* ll, char* str)
{
    RET_IF(!ll || !str, 0);

    node_t* curr = ll->head;
    while(curr != NULL)
    {
        if(strncmp(curr->value, str, 108) == 0)
            return 1;

        curr = curr->next;
    }

    return 0; 
}

int ll_get_max_int(const linked_list_t* ll)
{
    RET_IF(!ll || ll->count == 0, -1);

    int max = *((int*)ll->head->value);
    node_t* curr = ll->head->next;
    while(curr)
    {
        if(max < *((int*)curr->value))
            max = *((int*)curr->value);

        curr = curr->next;
    }

    return max;
}