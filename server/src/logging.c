#include "logging.h"

logging_t* create_log()
{
    logging_t* new_log = malloc(sizeof(logging_t));
    new_log->entry_list = ll_create();
    new_log->max_server_size = 0;
    return new_log;
}

void free_log(logging_t* log)
{
    while(ll_count(log->entry_list) > 0)
    {
        logging_entry_t* entry = NULL;
        ll_remove_last(log->entry_list, (void**)&entry);

        // at this moment every event can have strings/ints/NULL, if this keep on growing I might consider a switch case for each event
        if(entry != NULL)
        {
            if(entry->value != NULL)
                free(entry->value);
        }
    }

    free(log->entry_list);
    free(log);
}

int add_logging_entry(logging_t* lg, logging_event_t event, int fd, void* value)
{
    logging_entry_t* entry = malloc(sizeof(logging_entry_t));
    if(entry == NULL)
        return -1;

    entry->type = event;
    entry->fd = fd;
    entry->value = value;

    return ll_add_tail(lg->entry_list, entry);
}

int add_logging_entry_str(logging_t* lg, logging_event_t event, int fd, char* value)
{
    int len = strnlen(value, MAX_PATHNAME_API_LENGTH);
    char* cpy = malloc(sizeof(char) * (len + 1));
    strncpy(cpy, value, len);
    cpy[len] = '\0';

    int res = add_logging_entry(lg, event, fd, cpy);
    if(res == -1)
        free(cpy);

    return res;
}

int add_logging_entry_int(logging_t* lg, logging_event_t event, int fd, int value)
{
    int* cpy = malloc(sizeof(int));
    *cpy = value;

    int res = add_logging_entry(lg, event, fd, cpy);
    if(res == -1)
        free(cpy);

    return res;
}