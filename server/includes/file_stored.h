#ifndef __FILE_STORED__
#define __FILE_STORED__

#include <stdlib.h>
#include <stdint.h>
#include "queue.h"
#include "linked_list.h"
#include "utils.h"

typedef struct file_stored file_stored_t;

file_stored_t* create_file();

// getter and setters
char* file_get_data(file_stored_t* file);
size_t file_get_size(file_stored_t* file);
int file_get_lock_owner(file_stored_t* file);
struct timespec* file_get_creation_time(file_stored_t* file);
struct timespec* file_get_last_use_time(file_stored_t* file);
int file_get_use_frequency(file_stored_t* file);

int file_add_client(file_stored_t* file, int client);
bool_t file_is_opened_by(file_stored_t* file, int client);
int file_remove_client(file_stored_t* file, int client);

int file_enqueue_lock(file_stored_t* file, int client);
bool_t file_is_client_already_queued(file_stored_t* file, int client);
int file_dequeue_lock(file_stored_t* file);

void file_set_lock_owner(file_stored_t* file, int lock_owner);
void file_set_last_use_time(file_stored_t* file, struct timespec new_use_time);

uint32_t file_inc_frequency(file_stored_t* file, int step);

void free_file(file_stored_t* file);


#endif