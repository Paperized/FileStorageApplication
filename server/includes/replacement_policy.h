#ifndef __REPLACEMENT_POLICY__
#define __REPLACEMENT_POLICY__

#include "server.h"

bool_t run_replacement_algorithm(const char* skip_file, size_t mem_needed, linked_list_t** output);

int replacement_policy_fifo(file_stored_t* file1, file_stored_t* file2);
int replacement_policy_lfu(file_stored_t* file1, file_stored_t* file2);
int replacement_policy_lru(file_stored_t* file1, file_stored_t* file2);

#endif