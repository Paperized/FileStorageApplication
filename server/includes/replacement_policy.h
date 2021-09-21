#ifndef __REPLACEMENT_POLICY__
#define __REPLACEMENT_POLICY__

#include "server.h"

typedef struct replacement_entry replacement_entry_t;

bool_t run_replacement_algorithm(const char* skip_file, size_t mem_needed, linked_list_t** output);

int replacement_policy_fifo(file_stored_t* file1, file_stored_t* file2);
int replacement_policy_lfu(file_stored_t* file1, file_stored_t* file2);
int replacement_policy_lru(file_stored_t* file1, file_stored_t* file2);

size_t repl_get_data_size(replacement_entry_t* r);
queue_t* repl_get_locks_queue(replacement_entry_t* r);
void* repl_get_data(replacement_entry_t* r);
char* repl_get_pathname(replacement_entry_t* r);
void free_repl(replacement_entry_t* r);

#endif