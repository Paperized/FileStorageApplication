#ifndef __NETWORK_FILE__
#define __NETWORK_FILE__

#include <stdlib.h>
#include "queue.h"

typedef struct replaced_file replaced_file_t;

// Create a new replaced file
replaced_file_t* create_replfile();

// Set a lock queue to this replaced file
void replfile_set_locks_queue(replaced_file_t* r, queue_t* queue);

// Set a data to this replaced file
void replfile_set_data(replaced_file_t* r, void* data, size_t data_size);

// Set a pathname to this replaced file
void replfile_set_pathname(replaced_file_t* r, const char* pathname);

// Get the data size of this replaced file
size_t replfile_get_data_size(replaced_file_t* r);

// Get the lock queue of this replaced file
queue_t* replfile_get_locks_queue(replaced_file_t* r);

// Get the data of this replaced file
void* replfile_get_data(replaced_file_t* r);

// Get the pathname of this replaced file
char* replfile_get_pathname(replaced_file_t* r);

// Free this replaced file
void free_replfile(replaced_file_t* r);

#endif