#ifndef _LOGGING_H_
#define _LOGGING_H_

#include "linked_list.h"
#include "server_api_utils.h"
#include <stdlib.h>
#include <string.h>

typedef enum logging_event {
    CLIENT_JOINED,
    CLIENT_LEFT,
    FILE_OPENED,
    FILE_ADDED,
    FILE_REMOVED,
    FILE_WROTE,
    FILE_APPEND,
    FILE_READ,
    FILE_NREAD,
    FILE_CLOSED,
    CACHE_REPLACEMENT
} logging_event_t;

typedef struct logging_entry {
    logging_event_t type;
    int fd;
    void* value;
} logging_entry_t;

typedef struct logging {
    linked_list_t* entry_list;
    size_t max_server_size;
} logging_t;

logging_t* create_log();
void free_log(logging_t* log);

int add_logging_entry(logging_t* lg, logging_event_t event, int fd, void* value);
int add_logging_entry_str(logging_t* lg, logging_event_t event, int fd, char* value);
int add_logging_entry_int(logging_t* lg, logging_event_t event, int fd, int value);

void print_logging(logging_t* lg);

#define UPDATE_LOGGING_MAX_SIZE(lg, new_size) if(lg->max_server_size < new_size) \
                                                lg->max_server_size = new_size;

#endif