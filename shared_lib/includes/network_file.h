#ifndef __NETWORK_FILE__
#define __NETWORK_FILE__

#include <stdlib.h>
#include "queue.h"

typedef struct network_file network_file_t;

network_file_t* create_netfile();
void netfile_set_locks_queue(network_file_t* r, queue_t* queue);
void netfile_set_data(network_file_t* r, void* data, size_t data_size);
void netfile_set_pathname(network_file_t* r, const char* pathname);

size_t netfile_get_data_size(network_file_t* r);
queue_t* netfile_get_locks_queue(network_file_t* r);
void* netfile_get_data(network_file_t* r);
char* netfile_get_pathname(network_file_t* r);
void free_netfile(network_file_t* r);

#endif